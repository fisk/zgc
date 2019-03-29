/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "gc/z/zList.inline.hpp"
#include "gc/z/zNUMA.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zPageCache.hpp"
#include "gc/z/zStat.hpp"
#include "logging/log.hpp"

static const ZStatCounter ZCounterPageCacheHitL1("Memory", "Page Cache Hit L1", ZStatUnitOpsPerSecond);
static const ZStatCounter ZCounterPageCacheHitL2("Memory", "Page Cache Hit L2", ZStatUnitOpsPerSecond);
static const ZStatCounter ZCounterPageCacheMiss("Memory", "Page Cache Miss", ZStatUnitOpsPerSecond);

ZPageCache::ZPageCache() :
    _available(0),
    _small(),
    _medium(),
    _large() {}

ZPage* ZPageCache::alloc_small_page() {
  const uint32_t numa_id = ZNUMA::id();
  const uint32_t numa_count = ZNUMA::count();

  // Try NUMA local page cache
  ZPage* const l1_page = _small.get(numa_id).remove_first();
  if (l1_page != NULL) {
    ZStatInc(ZCounterPageCacheHitL1);
    return l1_page;
  }

  // Try NUMA remote page cache(s)
  uint32_t remote_numa_id = numa_id + 1;
  const uint32_t remote_numa_count = numa_count - 1;
  for (uint32_t i = 0; i < remote_numa_count; i++) {
    if (remote_numa_id == numa_count) {
      remote_numa_id = 0;
    }

    ZPage* const l2_page = _small.get(remote_numa_id).remove_first();
    if (l2_page != NULL) {
      ZStatInc(ZCounterPageCacheHitL2);
      return l2_page;
    }

    remote_numa_id++;
  }

  ZStatInc(ZCounterPageCacheMiss);
  return NULL;
}

ZPage* ZPageCache::alloc_medium_page() {
  ZPage* const l1_page = _medium.remove_first();
  if (l1_page != NULL) {
    ZStatInc(ZCounterPageCacheHitL1);
    return l1_page;
  }

  ZStatInc(ZCounterPageCacheMiss);
  return NULL;
}

ZPage* ZPageCache::alloc_large_page(size_t size) {
  // Find a page with the right size
  ZListIterator<ZPage> iter(&_large);
  for (ZPage* l1_page; iter.next(&l1_page);) {
    if (l1_page->size() == size) {
      // Page found
      _large.remove(l1_page);
      ZStatInc(ZCounterPageCacheHitL1);
      return l1_page;
    }
  }

  ZStatInc(ZCounterPageCacheMiss);
  return NULL;
}

ZPage* ZPageCache::alloc_page(uint8_t type, size_t size) {
  ZPage* page;

  if (type == ZPageTypeSmall) {
    page = alloc_small_page();
  } else if (type == ZPageTypeMedium) {
    page = alloc_medium_page();
  } else {
    page = alloc_large_page(size);
  }

  if (page != NULL) {
    _available -= page->size();
  }

  return page;
}

void ZPageCache::free_page(ZPage* page) {
  const uint8_t type = page->type();
  if (type == ZPageTypeSmall) {
    _small.get(page->numa_id()).insert_first(page);
  } else if (type == ZPageTypeMedium) {
    _medium.insert_first(page);
  } else {
    _large.insert_first(page);
  }

  _available += page->size();
}

void ZPageCache::flush_list(ZPageCacheFlushClosure* cl, ZList<ZPage>* from, ZList<ZPage>* to) {
  for (;;) {
    ZPage* const page = from->last();
    if (page == NULL || !cl->do_page(page)) {
      // Done
      return;
    }

    // Flush page
    _available -= page->size();
    from->remove(page);
    to->insert_last(page);
  }
}

void ZPageCache::flush_per_numa_lists(ZPageCacheFlushClosure* cl, ZPerNUMA<ZList<ZPage> >* from, ZList<ZPage>* to) {
  const uint32_t numa_count = ZNUMA::count();
  uint32_t numa_done = 0;
  uint32_t numa_next = 0;

  // Flush lists round-robin
  while (numa_done < numa_count) {
    ZList<ZPage>& numa_list = from->get(numa_next);
    if (++numa_next == numa_count) {
      numa_next = 0;
    }

    ZPage* const page = numa_list.last();
    if (page == NULL || !cl->do_page(page)) {
      // Done
      numa_done++;
      continue;
    }

    numa_done = 0;

    // Flush page
    _available -= page->size();
    numa_list.remove(page);
    to->insert_last(page);
  }
}

void ZPageCache::flush(ZPageCacheFlushClosure* cl, ZList<ZPage>* to) {
  // Prefer flushing large, then medium and last small pages
  flush_list(cl, &_large, to);
  flush_list(cl, &_medium, to);
  flush_per_numa_lists(cl, &_small, to);
}
