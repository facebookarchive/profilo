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
#include "base/macros.h"
#include "base/mutex.h"
#include "stack.h"  // StackReference

namespace art {

namespace mirror {
class Throwable;
}  // namespace mirror
class ArtMethod;
class Context;
class Thread;
class ShadowFrame;

// Manages exception delivery for Quick backend.
class QuickExceptionHandler {
 public:
  QuickExceptionHandler(Thread* self, bool is_deoptimization)
      SHARED_REQUIRES(Locks::mutator_lock_);

  NO_RETURN ~QuickExceptionHandler() {
    LOG(FATAL) << "UNREACHABLE";  // Expected to take long jump.
    UNREACHABLE();
  }

  // Find the catch handler for the given exception.
  void FindCatch(mirror::Throwable* exception) SHARED_REQUIRES(Locks::mutator_lock_);

  // Deoptimize the stack to the upcall. For every compiled frame, we create a "copy"
  // shadow frame that will be executed with the interpreter.
  void DeoptimizeStack() SHARED_REQUIRES(Locks::mutator_lock_);
  void DeoptimizeSingleFrame() SHARED_REQUIRES(Locks::mutator_lock_);
  void DeoptimizeSingleFrameArchDependentFixup() SHARED_REQUIRES(Locks::mutator_lock_);

  // Update the instrumentation stack by removing all methods that will be unwound
  // by the exception being thrown.
  void UpdateInstrumentationStack() SHARED_REQUIRES(Locks::mutator_lock_);

  // Set up environment before delivering an exception to optimized code.
  void SetCatchEnvironmentForOptimizedHandler(StackVisitor* stack_visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Long jump either to a catch handler or to the upcall.
  NO_RETURN void DoLongJump(bool smash_caller_saves = true) SHARED_REQUIRES(Locks::mutator_lock_);

  void SetHandlerQuickFrame(ArtMethod** handler_quick_frame) {
    handler_quick_frame_ = handler_quick_frame;
  }

  void SetHandlerQuickFramePc(uintptr_t handler_quick_frame_pc) {
    handler_quick_frame_pc_ = handler_quick_frame_pc;
  }

  void SetHandlerMethodHeader(const OatQuickMethodHeader* handler_method_header) {
    handler_method_header_ = handler_method_header;
  }

  void SetHandlerQuickArg0(uintptr_t handler_quick_arg0) {
    handler_quick_arg0_ = handler_quick_arg0;
  }

  ArtMethod* GetHandlerMethod() const {
    return handler_method_;
  }

  void SetHandlerMethod(ArtMethod* handler_quick_method) {
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

  // Walk the stack frames of the given thread, printing out non-runtime methods with their types
  // of frames. Helps to verify that single-frame deopt really only deopted one frame.
  static void DumpFramesWithType(Thread* self, bool details = false)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  Thread* const self_;
  Context* const context_;
  // Should we deoptimize the stack?
  const bool is_deoptimization_;
  // Is method tracing active?
  const bool method_tracing_active_;
  // Quick frame with found handler or last frame if no handler found.
  ArtMethod** handler_quick_frame_;
  // PC to branch to for the handler.
  uintptr_t handler_quick_frame_pc_;
  // Quick code of the handler.
  const OatQuickMethodHeader* handler_method_header_;
  // The value for argument 0.
  uintptr_t handler_quick_arg0_;
  // The handler method to report to the debugger.
  ArtMethod* handler_method_;
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
