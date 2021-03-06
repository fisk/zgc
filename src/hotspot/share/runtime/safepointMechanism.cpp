/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#include "precompiled.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "runtime/orderAccess.hpp"
#include "runtime/os.hpp"
#include "runtime/safepointMechanism.inline.hpp"
#include "runtime/stackWatermarkSet.hpp"
#include "services/memTracker.hpp"
#include "utilities/globalDefinitions.hpp"

uintptr_t SafepointMechanism::_poll_word_armed_value;
uintptr_t SafepointMechanism::_poll_word_disarmed_value;
uintptr_t SafepointMechanism::_poll_page_armed_value;
uintptr_t SafepointMechanism::_poll_page_disarmed_value;

void SafepointMechanism::default_initialize() {
  if (uses_thread_local_poll()) {
    // Poll bit values
    _poll_word_armed_value    = poll_bit();
    _poll_word_disarmed_value = ~_poll_word_armed_value;

    bool poll_bit_only = false;

#ifdef USE_POLL_BIT_ONLY
    poll_bit_only = USE_POLL_BIT_ONLY;
#endif

    if (poll_bit_only) {
      _poll_page_armed_value    = poll_bit();
      _poll_page_disarmed_value = 0;
    } else {
      // Polling page
      const size_t page_size = os::vm_page_size();
      const size_t allocation_size = 2 * page_size;
      char* polling_page = os::reserve_memory(allocation_size, NULL, page_size);
      os::commit_memory_or_exit(polling_page, allocation_size, false, "Unable to commit Safepoint polling page");
      MemTracker::record_virtual_memory_type((address)polling_page, mtSafepoint);

      char* bad_page  = polling_page;
      char* good_page = polling_page + page_size;

      os::protect_memory(bad_page,  page_size, os::MEM_PROT_NONE);
      os::protect_memory(good_page, page_size, os::MEM_PROT_READ);

      log_info(os)("SafePoint Polling address, bad (protected) page:" INTPTR_FORMAT ", good (unprotected) page:" INTPTR_FORMAT, p2i(bad_page), p2i(good_page));
      os::set_polling_page((address)(bad_page));

      // Poll address values
      _poll_page_armed_value    = reinterpret_cast<uintptr_t>(bad_page);
      _poll_page_disarmed_value = reinterpret_cast<uintptr_t>(good_page);
    }
  } else {
    const size_t page_size = os::vm_page_size();
    char* polling_page = os::reserve_memory(page_size, NULL, page_size);
    os::commit_memory_or_exit(polling_page, page_size, false, "Unable to commit Safepoint polling page");
    os::protect_memory(polling_page, page_size, os::MEM_PROT_READ);
    MemTracker::record_virtual_memory_type((address)polling_page, mtSafepoint);

    log_info(os)("SafePoint Polling address: " INTPTR_FORMAT, p2i(polling_page));
    os::set_polling_page((address)(polling_page));
  }
}

void SafepointMechanism::process_operation(JavaThread *thread) {
  if (global_poll()) {
    // Any load in ::block must not pass the global poll load.
    // Otherwise we might load an old safepoint counter (for example).
    OrderAccess::loadload();
    SafepointSynchronize::block(thread);
  }
  StackWatermarkSet::on_vm_operation(thread);
  if (uses_thread_local_poll() && thread->has_handshake()) {
    thread->handshake_process_by_self();
  }
}

uintptr_t SafepointMechanism::compute_poll_word(bool armed, uintptr_t stack_watermark) {
  if (armed) {
    log_debug(stackbarrier)("Computed armed at %d", Thread::current()->osthread()->thread_id());
    return SafepointMechanism::poll_word_armed_value();
  }
  if (stack_watermark == 0) {
    log_debug(stackbarrier)("Computed disarmed at %d", Thread::current()->osthread()->thread_id());
    return SafepointMechanism::poll_word_disarmed_value();
  }
  log_debug(stackbarrier)("Computed watermark at %d", Thread::current()->osthread()->thread_id());
  return stack_watermark + sizeof(void*);
}

void SafepointMechanism::update_poll_values(JavaThread* thread) {
  for (;;) {
    bool armed = global_poll() || thread->has_handshake();
    uintptr_t stack_watermark = thread->stack_watermark_set()->lowest_watermark();
    uintptr_t poll_page = armed ? SafepointMechanism::poll_page_armed_value()
                                : SafepointMechanism::poll_page_disarmed_value();
    uintptr_t poll_word = compute_poll_word(armed, stack_watermark);
    thread->set_polling_page(poll_page);
    thread->set_polling_word(poll_word);
    OrderAccess::fence();
    if (!armed && (global_poll() || thread->has_handshake())) {
      // We disarmed an old safepoint, but a new one is synchronizing.
      // We need to arm the poll for the subsequent safepoint poll.
      continue;
    }
    break;
  }
}

void SafepointMechanism::process_operation_if_requested_slow(JavaThread *thread) {
  // Read global poll and has_handshake after local poll
  OrderAccess::loadload();

  // local poll already checked, if used.
  process_operation(thread);
  update_poll_values(thread);
  OrderAccess::cross_modify_fence();
}

void SafepointMechanism::initialize_header(JavaThread* thread) {
  disarm_local_poll(thread);
}

void SafepointMechanism::initialize() {
  pd_initialize();
}
