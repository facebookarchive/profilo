/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_JVMTI_ALLOCATOR_H_
#define ART_RUNTIME_OPENJDKJVMTI_JVMTI_ALLOCATOR_H_

#include "base/logging.h"
#include "base/macros.h"
#include "jvmti.h"

namespace openjdkjvmti {

template <typename T> class JvmtiAllocator;

template <>
class JvmtiAllocator<void> {
 public:
  typedef void value_type;
  typedef void* pointer;
  typedef const void* const_pointer;

  template <typename U>
  struct rebind {
    typedef JvmtiAllocator<U> other;
  };

  explicit JvmtiAllocator(jvmtiEnv* env) : env_(env) {}

  template <typename U>
  JvmtiAllocator(const JvmtiAllocator<U>& other)  // NOLINT, implicit
      : env_(other.env_) {}

  JvmtiAllocator(const JvmtiAllocator& other) = default;
  JvmtiAllocator& operator=(const JvmtiAllocator& other) = default;
  ~JvmtiAllocator() = default;

 private:
  jvmtiEnv* env_;

  template <typename U>
  friend class JvmtiAllocator;

  template <typename U>
  friend bool operator==(const JvmtiAllocator<U>& lhs, const JvmtiAllocator<U>& rhs);
};

template <typename T>
class JvmtiAllocator {
 public:
  typedef T value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef const T* const_pointer;
  typedef const T& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <typename U>
  struct rebind {
    typedef JvmtiAllocator<U> other;
  };

  explicit JvmtiAllocator(jvmtiEnv* env) : env_(env) {}

  template <typename U>
  JvmtiAllocator(const JvmtiAllocator<U>& other)  // NOLINT, implicit
      : env_(other.env_) {}

  JvmtiAllocator(const JvmtiAllocator& other) = default;
  JvmtiAllocator& operator=(const JvmtiAllocator& other) = default;
  ~JvmtiAllocator() = default;

  size_type max_size() const {
    return static_cast<size_type>(-1) / sizeof(T);
  }

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  pointer allocate(size_type n, JvmtiAllocator<void>::pointer hint ATTRIBUTE_UNUSED = nullptr) {
    DCHECK_LE(n, max_size());
    if (env_ == nullptr) {
      T* result = reinterpret_cast<T*>(malloc(n * sizeof(T)));
      CHECK(result != nullptr || n == 0u);  // Abort if malloc() fails.
      return result;
    } else {
      unsigned char* result;
      jvmtiError alloc_error = env_->Allocate(n * sizeof(T), &result);
      CHECK(alloc_error == JVMTI_ERROR_NONE);
      return reinterpret_cast<T*>(result);
    }
  }
  void deallocate(pointer p, size_type n ATTRIBUTE_UNUSED) {
    if (env_ == nullptr) {
      free(p);
    } else {
      jvmtiError dealloc_error = env_->Deallocate(reinterpret_cast<unsigned char*>(p));
      CHECK(dealloc_error == JVMTI_ERROR_NONE);
    }
  }

  void construct(pointer p, const_reference val) {
    new (static_cast<void*>(p)) value_type(val);
  }
  template <class U, class... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }
  void destroy(pointer p) {
    p->~value_type();
  }

  inline bool operator==(JvmtiAllocator const& other) {
    return env_ == other.env_;
  }
  inline bool operator!=(JvmtiAllocator const& other) {
    return !operator==(other);
  }

 private:
  jvmtiEnv* env_;

  template <typename U>
  friend class JvmtiAllocator;

  template <typename U>
  friend bool operator==(const JvmtiAllocator<U>& lhs, const JvmtiAllocator<U>& rhs);
};

template <typename T>
inline bool operator==(const JvmtiAllocator<T>& lhs, const JvmtiAllocator<T>& rhs) {
  return lhs.env_ == rhs.env_;
}

template <typename T>
inline bool operator!=(const JvmtiAllocator<T>& lhs, const JvmtiAllocator<T>& rhs) {
  return !(lhs == rhs);
}

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_JVMTI_ALLOCATOR_H_
