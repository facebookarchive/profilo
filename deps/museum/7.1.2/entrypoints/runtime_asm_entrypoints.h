/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_ENTRYPOINTS_RUNTIME_ASM_ENTRYPOINTS_H_
#define ART_RUNTIME_ENTRYPOINTS_RUNTIME_ASM_ENTRYPOINTS_H_

namespace art {

#ifndef BUILDING_LIBART
#error "File and symbols only for use within libart."
#endif

extern "C" void* art_jni_dlsym_lookup_stub(JNIEnv*, jobject);
static inline const void* GetJniDlsymLookupStub() {
  return reinterpret_cast<const void*>(art_jni_dlsym_lookup_stub);
}

// Return the address of quick stub code for handling IMT conflicts.
extern "C" void art_quick_imt_conflict_trampoline(ArtMethod*);
static inline const void* GetQuickImtConflictStub() {
  return reinterpret_cast<const void*>(art_quick_imt_conflict_trampoline);
}

// Return the address of quick stub code for bridging from quick code to the interpreter.
extern "C" void art_quick_to_interpreter_bridge(ArtMethod*);
static inline const void* GetQuickToInterpreterBridge() {
  return reinterpret_cast<const void*>(art_quick_to_interpreter_bridge);
}

// Return the address of quick stub code for handling JNI calls.
extern "C" void art_quick_generic_jni_trampoline(ArtMethod*);
static inline const void* GetQuickGenericJniStub() {
  return reinterpret_cast<const void*>(art_quick_generic_jni_trampoline);
}

// Return the address of quick stub code for handling transitions into the proxy invoke handler.
extern "C" void art_quick_proxy_invoke_handler();
static inline const void* GetQuickProxyInvokeHandler() {
  return reinterpret_cast<const void*>(art_quick_proxy_invoke_handler);
}

// Return the address of quick stub code for resolving a method at first call.
extern "C" void art_quick_resolution_trampoline(ArtMethod*);
static inline const void* GetQuickResolutionStub() {
  return reinterpret_cast<const void*>(art_quick_resolution_trampoline);
}

// Entry point for quick code that performs deoptimization.
extern "C" void art_quick_deoptimize();
static inline const void* GetQuickDeoptimizationEntryPoint() {
  return reinterpret_cast<const void*>(art_quick_deoptimize);
}

// Return address of instrumentation entry point used by non-interpreter based tracing.
extern "C" void art_quick_instrumentation_entry(void*);
static inline const void* GetQuickInstrumentationEntryPoint() {
  return reinterpret_cast<const void*>(art_quick_instrumentation_entry);
}

// Stub to deoptimize from compiled code.
extern "C" void art_quick_deoptimize_from_compiled_code();

// The return_pc of instrumentation exit stub.
extern "C" void art_quick_instrumentation_exit();
static inline const void* GetQuickInstrumentationExitPc() {
  return reinterpret_cast<const void*>(art_quick_instrumentation_exit);
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_RUNTIME_ASM_ENTRYPOINTS_H_
