/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_SOLARIS_SPARC_ZPHYSICALMEMORYBACKING_SOLARIS_SPARC_HPP
#define OS_CPU_SOLARIS_SPARC_ZPHYSICALMEMORYBACKING_SOLARIS_SPARC_HPP

#include "gc/z/zMemory.hpp"
#include "memory/allocation.hpp"

class ZErrno;
class ZPhysicalMemory;

class ZPhysicalMemoryBacking VALUE_OBJ_CLASS_SPEC {
private:
  const size_t _granule_size;
  bool         _initialized;

  bool initialize_vamask() const;
  bool initialize_osm() const;
  bool initialize_anonymous() const;

  void map_osm(ZPhysicalMemory pmem, uintptr_t offset) const;
  void unmap_osm(ZPhysicalMemory pmem, uintptr_t offset) const;

  void advise_anonymous(uintptr_t addr, size_t size) const;
  void map_anonymous(ZPhysicalMemory pmem, uintptr_t offset) const;
  void unmap_anonymous(ZPhysicalMemory pmem, uintptr_t offset) const;

public:
  ZPhysicalMemoryBacking(size_t max_capacity, size_t granule_size);

  bool is_initialized() const;

  bool expand(size_t from, size_t to);
  ZPhysicalMemory alloc(size_t size);
  void free(ZPhysicalMemory pmem);

  uintptr_t nmt_address(uintptr_t offset) const;

  void map(ZPhysicalMemory pmem, uintptr_t offset) const;
  void unmap(ZPhysicalMemory pmem, uintptr_t offset) const;
  void flip(ZPhysicalMemory pmem, uintptr_t offset) const;
};

#endif // OS_CPU_SOLARIS_SPARC_ZPHYSICALMEMORYBACKING_SOLARIS_SPARC_HPP
