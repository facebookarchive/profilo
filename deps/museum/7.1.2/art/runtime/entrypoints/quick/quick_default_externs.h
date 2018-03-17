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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_DEFAULT_EXTERNS_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_DEFAULT_EXTERNS_H_

#include <cstdint>

namespace art {
namespace mirror {
class Array;
class Class;
class Object;
}  // namespace mirror
class ArtMethod;
}  // namespace art

// These are extern declarations of assembly stubs with common names.

// Cast entrypoints.
extern "C" void art_quick_check_cast(const art::mirror::Class*, const art::mirror::Class*);

// DexCache entrypoints.
extern "C" void* art_quick_initialize_static_storage(uint32_t);
extern "C" void* art_quick_initialize_type(uint32_t);
extern "C" void* art_quick_initialize_type_and_verify_access(uint32_t);
extern "C" void* art_quick_resolve_string(uint32_t);

// Field entrypoints.
extern "C" int art_quick_set8_instance(uint32_t, void*, int8_t);
extern "C" int art_quick_set8_static(uint32_t, int8_t);
extern "C" int art_quick_set16_instance(uint32_t, void*, int16_t);
extern "C" int art_quick_set16_static(uint32_t, int16_t);
extern "C" int art_quick_set32_instance(uint32_t, void*, int32_t);
extern "C" int art_quick_set32_static(uint32_t, int32_t);
extern "C" int art_quick_set64_instance(uint32_t, void*, int64_t);
extern "C" int art_quick_set64_static(uint32_t, int64_t);
extern "C" int art_quick_set_obj_instance(uint32_t, void*, void*);
extern "C" int art_quick_set_obj_static(uint32_t, void*);
extern "C" int8_t art_quick_get_byte_instance(uint32_t, void*);
extern "C" uint8_t art_quick_get_boolean_instance(uint32_t, void*);
extern "C" int8_t art_quick_get_byte_static(uint32_t);
extern "C" uint8_t art_quick_get_boolean_static(uint32_t);
extern "C" int16_t art_quick_get_short_instance(uint32_t, void*);
extern "C" uint16_t art_quick_get_char_instance(uint32_t, void*);
extern "C" int16_t art_quick_get_short_static(uint32_t);
extern "C" uint16_t art_quick_get_char_static(uint32_t);
extern "C" int32_t art_quick_get32_instance(uint32_t, void*);
extern "C" int32_t art_quick_get32_static(uint32_t);
extern "C" int64_t art_quick_get64_instance(uint32_t, void*);
extern "C" int64_t art_quick_get64_static(uint32_t);
extern "C" void* art_quick_get_obj_instance(uint32_t, void*);
extern "C" void* art_quick_get_obj_static(uint32_t);

// Array entrypoints.
extern "C" void art_quick_aput_obj_with_null_and_bound_check(art::mirror::Array*, int32_t,
                                                             art::mirror::Object*);
extern "C" void art_quick_aput_obj_with_bound_check(art::mirror::Array*, int32_t,
                                                    art::mirror::Object*);
extern "C" void art_quick_aput_obj(art::mirror::Array*, int32_t, art::mirror::Object*);
extern "C" void art_quick_handle_fill_data(void*, void*);

// Lock entrypoints.
extern "C" void art_quick_lock_object(art::mirror::Object*);
extern "C" void art_quick_unlock_object(art::mirror::Object*);

// Lock entrypoints that do not inline any behavior (e.g., thin-locks).
extern "C" void art_quick_lock_object_no_inline(art::mirror::Object*);
extern "C" void art_quick_unlock_object_no_inline(art::mirror::Object*);

// Math entrypoints.
extern "C" int64_t art_quick_d2l(double);
extern "C" int64_t art_quick_f2l(float);
extern "C" float art_quick_l2f(int64_t);
extern "C" int64_t art_quick_ldiv(int64_t, int64_t);
extern "C" int64_t art_quick_lmod(int64_t, int64_t);
extern "C" int64_t art_quick_lmul(int64_t, int64_t);
extern "C" uint64_t art_quick_lshl(uint64_t, uint32_t);
extern "C" uint64_t art_quick_lshr(uint64_t, uint32_t);
extern "C" uint64_t art_quick_lushr(uint64_t, uint32_t);
extern "C" int64_t art_quick_mul_long(int64_t, int64_t);
extern "C" uint64_t art_quick_shl_long(uint64_t, uint32_t);
extern "C" uint64_t art_quick_shr_long(uint64_t, uint32_t);
extern "C" uint64_t art_quick_ushr_long(uint64_t, uint32_t);

// Intrinsic entrypoints.
extern "C" int32_t art_quick_indexof(void*, uint32_t, uint32_t);
extern "C" int32_t art_quick_string_compareto(void*, void*);
extern "C" void* art_quick_memcpy(void*, const void*, size_t);

// Invoke entrypoints.
extern "C" void art_quick_imt_conflict_trampoline(art::ArtMethod*);
extern "C" void art_quick_resolution_trampoline(art::ArtMethod*);
extern "C" void art_quick_to_interpreter_bridge(art::ArtMethod*);
extern "C" void art_quick_invoke_direct_trampoline_with_access_check(uint32_t, void*);
extern "C" void art_quick_invoke_interface_trampoline_with_access_check(uint32_t, void*);
extern "C" void art_quick_invoke_static_trampoline_with_access_check(uint32_t, void*);
extern "C" void art_quick_invoke_super_trampoline_with_access_check(uint32_t, void*);
extern "C" void art_quick_invoke_virtual_trampoline_with_access_check(uint32_t, void*);

// Thread entrypoints.
extern "C" void art_quick_test_suspend();

// Throw entrypoints.
extern "C" void art_quick_deliver_exception(art::mirror::Object*);
extern "C" void art_quick_throw_array_bounds(int32_t index, int32_t limit);
extern "C" void art_quick_throw_div_zero();
extern "C" void art_quick_throw_no_such_method(int32_t method_idx);
extern "C" void art_quick_throw_null_pointer_exception();
extern "C" void art_quick_throw_stack_overflow(void*);

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_DEFAULT_EXTERNS_H_
