/* Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_JVMTI_WEAK_TABLE_H_
#define ART_RUNTIME_OPENJDKJVMTI_JVMTI_WEAK_TABLE_H_

#include <museum/8.0.0/external/libcxx/unordered_map>

#include <museum/8.0.0/art/runtime/base/macros.h>
#include <museum/8.0.0/art/runtime/base/mutex.h>
#include <museum/8.0.0/art/runtime/gc/system_weak.h>
#include <museum/8.0.0/art/runtime/gc_root-inl.h>
#include <museum/8.0.0/art/runtime/globals.h>
#include "jvmti.h"
#include <museum/8.0.0/art/runtime/mirror/object.h>
#include <museum/8.0.0/art/runtime/thread-inl.h>

namespace openjdkjvmti {

class EventHandler;

// A system-weak container mapping objects to elements of the template type. This corresponds
// to a weak hash map. For historical reasons the stored value is called "tag."
template <typename T>
class JvmtiWeakTable : public facebook::museum::MUSEUM_VERSION::art::gc::SystemWeakHolder {
 public:
  JvmtiWeakTable()
      : facebook::museum::MUSEUM_VERSION::art::gc::SystemWeakHolder(facebook::museum::MUSEUM_VERSION::art::kTaggingLockLevel),
        update_since_last_sweep_(false) {
  }

  // Remove the mapping for the given object, returning whether such a mapping existed (and the old
  // value).
  bool Remove(facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_);
  bool RemoveLocked(facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  // Set the mapping for the given object. Returns true if this overwrites an already existing
  // mapping.
  virtual bool Set(facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, T tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_);
  virtual bool SetLocked(facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, T tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  // Return the value associated with the given object. Returns true if the mapping exists, false
  // otherwise.
  bool GetTag(facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* result)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_) {
    facebook::museum::MUSEUM_VERSION::art::Thread* self = facebook::museum::MUSEUM_VERSION::art::Thread::Current();
    facebook::museum::MUSEUM_VERSION::art::MutexLock mu(self, allow_disallow_lock_);
    Wait(self);

    return GetTagLocked(self, obj, result);
  }
  bool GetTagLocked(facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* result)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_) {
    facebook::museum::MUSEUM_VERSION::art::Thread* self = facebook::museum::MUSEUM_VERSION::art::Thread::Current();
    allow_disallow_lock_.AssertHeld(self);
    Wait(self);

    return GetTagLocked(self, obj, result);
  }

  // Sweep the container. DO NOT CALL MANUALLY.
  void Sweep(facebook::museum::MUSEUM_VERSION::art::IsMarkedVisitor* visitor)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_);

  // Return all objects that have a value mapping in tags.
  jvmtiError GetTaggedObjects(jvmtiEnv* jvmti_env,
                              jint tag_count,
                              const T* tags,
                              /* out */ jint* count_ptr,
                              /* out */ jobject** object_result_ptr,
                              /* out */ T** tag_result_ptr)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_);

  // Locking functions, to allow coarse-grained locking and amortization.
  void Lock() ACQUIRE(allow_disallow_lock_);
  void Unlock() RELEASE(allow_disallow_lock_);
  void AssertLocked() ASSERT_CAPABILITY(allow_disallow_lock_);

  facebook::museum::MUSEUM_VERSION::art::mirror::Object* Find(T tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_);

 protected:
  // Should HandleNullSweep be called when Sweep detects the release of an object?
  virtual bool DoesHandleNullOnSweep() {
    return false;
  }
  // If DoesHandleNullOnSweep returns true, this function will be called.
  virtual void HandleNullSweep(T tag ATTRIBUTE_UNUSED) {}

 private:
  bool SetLocked(facebook::museum::MUSEUM_VERSION::art::Thread* self, facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, T tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  bool RemoveLocked(facebook::museum::MUSEUM_VERSION::art::Thread* self, facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* tag)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  bool GetTagLocked(facebook::museum::MUSEUM_VERSION::art::Thread* self, facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* result)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_) {
    auto it = tagged_objects_.find(facebook::museum::MUSEUM_VERSION::art::GcRoot<facebook::museum::MUSEUM_VERSION::art::mirror::Object>(obj));
    if (it != tagged_objects_.end()) {
      *result = it->second;
      return true;
    }

    // Performance optimization: To avoid multiple table updates, ensure that during GC we
    // only update once. See the comment on the implementation of GetTagSlowPath.
    if (facebook::museum::MUSEUM_VERSION::art::kUseReadBarrier &&
        self != nullptr &&
        self->GetIsGcMarking() &&
        !update_since_last_sweep_) {
      return GetTagSlowPath(self, obj, result);
    }

    return false;
  }

  // Slow-path for GetTag. We didn't find the object, but we might be storing from-pointers and
  // are asked to retrieve with a to-pointer.
  bool GetTagSlowPath(facebook::museum::MUSEUM_VERSION::art::Thread* self, facebook::museum::MUSEUM_VERSION::art::mirror::Object* obj, /* out */ T* result)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  // Update the table by doing read barriers on each element, ensuring that to-space pointers
  // are stored.
  void UpdateTableWithReadBarrier()
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  template <bool kHandleNull>
  void SweepImpl(facebook::museum::MUSEUM_VERSION::art::IsMarkedVisitor* visitor)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_);

  enum TableUpdateNullTarget {
    kIgnoreNull,
    kRemoveNull,
    kCallHandleNull
  };

  template <typename Updater, TableUpdateNullTarget kTargetNull>
  void UpdateTableWith(Updater& updater)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_)
      REQUIRES(allow_disallow_lock_);

  template <typename Storage, class Allocator = std::allocator<T>>
  struct ReleasableContainer;

  struct HashGcRoot {
    size_t operator()(const facebook::museum::MUSEUM_VERSION::art::GcRoot<facebook::museum::MUSEUM_VERSION::art::mirror::Object>& r) const
        REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_) {
      return reinterpret_cast<uintptr_t>(r.Read<facebook::museum::MUSEUM_VERSION::art::kWithoutReadBarrier>());
    }
  };

  struct EqGcRoot {
    bool operator()(const facebook::museum::MUSEUM_VERSION::art::GcRoot<facebook::museum::MUSEUM_VERSION::art::mirror::Object>& r1,
                    const facebook::museum::MUSEUM_VERSION::art::GcRoot<facebook::museum::MUSEUM_VERSION::art::mirror::Object>& r2) const
        REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_) {
      return r1.Read<facebook::museum::MUSEUM_VERSION::art::kWithoutReadBarrier>() == r2.Read<facebook::museum::MUSEUM_VERSION::art::kWithoutReadBarrier>();
    }
  };

  std::unordered_map<facebook::museum::MUSEUM_VERSION::art::GcRoot<facebook::museum::MUSEUM_VERSION::art::mirror::Object>,
                     T,
                     HashGcRoot,
                     EqGcRoot> tagged_objects_
      GUARDED_BY(allow_disallow_lock_)
      GUARDED_BY(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_);
  // To avoid repeatedly scanning the whole table, remember if we did that since the last sweep.
  bool update_since_last_sweep_;
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_JVMTI_WEAK_TABLE_H_
