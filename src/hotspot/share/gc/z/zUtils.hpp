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

#ifndef SHARE_GC_Z_ZUTILS_HPP
#define SHARE_GC_Z_ZUTILS_HPP

#include "memory/allocation.hpp"

class ZUtils : public AllStatic {
public:
  // Allocation
  static uintptr_t alloc_aligned(size_t alignment, size_t size);

  // Power of two
  static size_t round_up_power_of_2(size_t value);
  static size_t round_down_power_of_2(size_t value);

  // Size convertion
  static size_t bytes_to_words(size_t size_in_words);
  static size_t words_to_bytes(size_t size_in_words);

  // Object
  static size_t object_size(uintptr_t addr);
  static void object_copy(uintptr_t from, uintptr_t to, size_t size);

  // Filler
  static void insert_filler_object(uintptr_t addr, size_t size);
};

#endif // SHARE_GC_Z_ZUTILS_HPP
