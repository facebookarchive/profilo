/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_LOCK_COUNT_DATA_H_
#define ART_RUNTIME_INTERPRETER_LOCK_COUNT_DATA_H_

#include <memory>
#include <vector>

#include "base/mutex.h"

namespace art {

namespace mirror {
  class Object;
}  // namespace mirror

class Thread;

// Counting locks by storing object pointers into a vector. Duplicate entries mark recursive locks.
// The vector will be visited with the ShadowFrame during GC (so all the locked-on objects are
// thread roots).
// Note: implementation is split so that the call sites may be optimized to no-ops in case no
//       lock counting is necessary. The actual implementation is in the cc file to avoid
//       dependencies.
class LockCountData {
 public:
  // Add the given object to the list of monitors, that is, objects that have been locked. This
  // will not throw (but be skipped if there is an exception pending on entry).
  void AddMonitor(Thread* self, mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Try to remove the given object from the monitor list, indicating an unlock operation.
  // This will throw an IllegalMonitorStateException (clearing any already pending exception), in
  // case that there wasn't a lock recorded for the object.
  void RemoveMonitorOrThrow(Thread* self,
                            const mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Check whether all acquired monitors have been released. This will potentially throw an
  // IllegalMonitorStateException, clearing any already pending exception. Returns true if the
  // check shows that everything is OK wrt/ lock counting, false otherwise.
  bool CheckAllMonitorsReleasedOrThrow(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename T, typename... Args>
  void VisitMonitors(T visitor, Args&&... args) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (monitors_ != nullptr) {
      // Visitors may change the Object*. Be careful with the foreach loop.
      for (mirror::Object*& obj : *monitors_) {
        visitor(/* inout */ &obj, std::forward<Args>(args)...);
      }
    }
  }

 private:
  // Stores references to the locked-on objects. As noted, this should be visited during thread
  // marking.
  std::unique_ptr<std::vector<mirror::Object*>> monitors_;
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_LOCK_COUNT_DATA_H_
