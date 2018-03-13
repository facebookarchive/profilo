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

#ifndef ART_RUNTIME_QUICK_EXCEPTION_HANDLER_H_
#define ART_RUNTIME_QUICK_EXCEPTION_HANDLER_H_

#include "base/logging.h"
#include "base/mutex.h"
#include "stack.h"  // StackReference

namespace art {

namespace mirror {
class ArtMethod;
class Throwable;
}  // namespace mirror
class Context;
class Thread;
class ThrowLocation;
class ShadowFrame;

// Manages exception delivery for Quick backend. Not used by Portable backend.
class QuickExceptionHandler {
 public:
  QuickExceptionHandler(Thread* self, bool is_deoptimization)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ~QuickExceptionHandler() {
    LOG(FATAL) << "UNREACHABLE";  // Expected to take long jump.
  }

  void FindCatch(const ThrowLocation& throw_location, mirror::Throwable* exception,
                 bool is_exception_reported)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DeoptimizeStack() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void UpdateInstrumentationStack() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DoLongJump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetHandlerQuickFrame(StackReference<mirror::ArtMethod>* handler_quick_frame) {
    handler_quick_frame_ = handler_quick_frame;
  }

  void SetHandlerQuickFramePc(uintptr_t handler_quick_frame_pc) {
    handler_quick_frame_pc_ = handler_quick_frame_pc;
  }

  mirror::ArtMethod* GetHandlerMethod() const {
    return handler_method_;
  }

  void SetHandlerMethod(mirror::ArtMethod* handler_quick_method) {
    handler_method_ = handler_quick_method;
  }

  uint32_t GetHandlerDexPc() const {
    return handler_dex_pc_;
  }

  void SetHandlerDexPc(uint32_t dex_pc) {
    handler_dex_pc_ = dex_pc;
  }

  void SetClearException(bool clear_exception) {
    clear_exception_ = clear_exception;
  }

  void SetHandlerFrameDepth(size_t frame_depth) {
    handler_frame_depth_ = frame_depth;
  }

 private:
  Thread* const self_;
  Context* const context_;
  const bool is_deoptimization_;
  // Is method tracing active?
  const bool method_tracing_active_;
  // Quick frame with found handler or last frame if no handler found.
  StackReference<mirror::ArtMethod>* handler_quick_frame_;
  // PC to branch to for the handler.
  uintptr_t handler_quick_frame_pc_;
  // The handler method to report to the debugger.
  mirror::ArtMethod* handler_method_;
  // The handler's dex PC, zero implies an uncaught exception.
  uint32_t handler_dex_pc_;
  // Should the exception be cleared as the catch block has no move-exception?
  bool clear_exception_;
  // Frame depth of the catch handler or the upcall.
  size_t handler_frame_depth_;

  DISALLOW_COPY_AND_ASSIGN(QuickExceptionHandler);
};

}  // namespace art
#endif  // ART_RUNTIME_QUICK_EXCEPTION_HANDLER_H_
