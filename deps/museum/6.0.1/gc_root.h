/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_GC_ROOT_H_
#define ART_RUNTIME_GC_ROOT_H_

#include "base/macros.h"
#include "base/mutex.h"       // For Locks::mutator_lock_.
#include "mirror/object_reference.h"

namespace art {

namespace mirror {
class Object;
}  // namespace mirror

template <size_t kBufferSize>
class BufferedRootVisitor;

// Dependent on pointer size so that we don't have frames that are too big on 64 bit.
static const size_t kDefaultBufferedRootCount = 1024 / sizeof(void*);

enum RootType {
  kRootUnknown = 0,
  kRootJNIGlobal,
  kRootJNILocal,
  kRootJavaFrame,
  kRootNativeStack,
  kRootStickyClass,
  kRootThreadBlock,
  kRootMonitorUsed,
  kRootThreadObject,
  kRootInternedString,
  kRootDebugger,
  kRootVMInternal,
  kRootJNIMonitor,
};
std::ostream& operator<<(std::ostream& os, const RootType& root_type);

// Only used by hprof. thread_id_ and type_ are only used by hprof.
class RootInfo {
 public:
  // Thread id 0 is for non thread roots.
  explicit RootInfo(RootType type, uint32_t thread_id = 0)
     : type_(type), thread_id_(thread_id) {
  }
  RootInfo(const RootInfo&) = default;
  virtual ~RootInfo() {
  }
  RootType GetType() const {
    return type_;
  }
  uint32_t GetThreadId() const {
    return thread_id_;
  }
  virtual void Describe(std::ostream& os) const {
    os << "Type=" << type_ << " thread_id=" << thread_id_;
  }
  std::string ToString() const;

 private:
  const RootType type_;
  const uint32_t thread_id_;
};

inline std::ostream& operator<<(std::ostream& os, const RootInfo& root_info) {
  root_info.Describe(os);
  return os;
}

class RootVisitor {
 public:
  virtual ~RootVisitor() { }

  // Single root version, not overridable.
  ALWAYS_INLINE void VisitRoot(mirror::Object** roots, const RootInfo& info)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    VisitRoots(&roots, 1, info);
  }

  // Single root version, not overridable.
  ALWAYS_INLINE void VisitRootIfNonNull(mirror::Object** roots, const RootInfo& info)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (*roots != nullptr) {
      VisitRoot(roots, info);
    }
  }

  virtual void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                          const RootInfo& info)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;
};

// Only visits roots one at a time, doesn't handle updating roots. Used when performance isn't
// critical.
class SingleRootVisitor : public RootVisitor {
 private:
  void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info) OVERRIDE
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      VisitRoot(*roots[i], info);
    }
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                          const RootInfo& info) OVERRIDE
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      VisitRoot(roots[i]->AsMirrorPtr(), info);
    }
  }

  virtual void VisitRoot(mirror::Object* root, const RootInfo& info) = 0;
};

template<class MirrorType>
class GcRoot {
 public:
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE MirrorType* Read() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VisitRoot(RootVisitor* visitor, const RootInfo& info) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(!IsNull());
    mirror::CompressedReference<mirror::Object>* roots[1] = { &root_ };
    visitor->VisitRoots(roots, 1u, info);
    DCHECK(!IsNull());
  }

  void VisitRootIfNonNull(RootVisitor* visitor, const RootInfo& info) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsNull()) {
      VisitRoot(visitor, info);
    }
  }

  ALWAYS_INLINE mirror::CompressedReference<mirror::Object>* AddressWithoutBarrier() {
    return &root_;
  }

  ALWAYS_INLINE bool IsNull() const {
    // It's safe to null-check it without a read barrier.
    return root_.IsNull();
  }

  ALWAYS_INLINE GcRoot(MirrorType* ref = nullptr) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Root visitors take pointers to root_ and place the min CompressedReference** arrays. We use a
  // CompressedReference<mirror::Object> here since it violates strict aliasing requirements to
  // cast CompressedReference<MirrorType>* to CompressedReference<mirror::Object>*.
  mutable mirror::CompressedReference<mirror::Object> root_;

  template <size_t kBufferSize> friend class BufferedRootVisitor;
};

// Simple data structure for buffered root visiting to avoid virtual dispatch overhead. Currently
// only for CompressedReferences since these are more common than the Object** roots which are only
// for thread local roots.
template <size_t kBufferSize>
class BufferedRootVisitor {
 public:
  BufferedRootVisitor(RootVisitor* visitor, const RootInfo& root_info)
      : visitor_(visitor), root_info_(root_info), buffer_pos_(0) {
  }

  ~BufferedRootVisitor() {
    Flush();
  }

  template <class MirrorType>
  ALWAYS_INLINE void VisitRootIfNonNull(GcRoot<MirrorType>& root)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!root.IsNull()) {
      VisitRoot(root);
    }
  }

  template <class MirrorType>
  ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<MirrorType>* root)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  template <class MirrorType>
  void VisitRoot(GcRoot<MirrorType>& root) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    VisitRoot(root.AddressWithoutBarrier());
  }

  template <class MirrorType>
  void VisitRoot(mirror::CompressedReference<MirrorType>* root)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (UNLIKELY(buffer_pos_ >= kBufferSize)) {
      Flush();
    }
    roots_[buffer_pos_++] = root;
  }

  void Flush() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    visitor_->VisitRoots(roots_, buffer_pos_, root_info_);
    buffer_pos_ = 0;
  }

 private:
  RootVisitor* const visitor_;
  RootInfo root_info_;
  mirror::CompressedReference<mirror::Object>* roots_[kBufferSize];
  size_t buffer_pos_;
};

}  // namespace art

#endif  // ART_RUNTIME_GC_ROOT_H_
