/*
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// @nolint
// @generated
auto get_runtime_from_thread(uintptr_t thread) {
  uint32_t jni_env = Read4(AccessField(AccessField(thread, 128U), 28U));
  uint32_t java_vm = Read4(AccessField(jni_env, 8U));
  uint32_t runtime = Read4(AccessField(java_vm, 4U));
  return runtime;
}

auto get_runtime() {
  return get_runtime_from_thread(get_art_thread());
}

auto get_class_dexfile(uintptr_t cls) {
  uintptr_t dexcache_heap_ref = AccessField(cls, 16U);
  uint32_t dexcache_ptr = Read4(AccessField(dexcache_heap_ref, 0U));
  uint32_t dexcache = dexcache_ptr;
  uint64_t dexfile = Read8(AccessField(dexcache, 32U));
  return dexfile;
}

auto get_dexfile_string_by_idx(uintptr_t dexfile, uintptr_t idx) {
  idx = idx;
  uintptr_t id = AccessArrayItem(Read4(AccessField(dexfile, 36U)), idx, 4U);
  uint32_t ptr = (Read4(AccessField(dexfile, 4U)) + Read4(AccessField(id, 0U)));
  uint32_t val = ptr;
  uint32_t length = 0U;
  uint32_t index = 0U;
  bool proceed = true;
  while (proceed) {
    uint8_t byte = Read1(AccessArrayItem(val, index, 1U));
    length = (length | ((byte & 127U) << (index * 7U)));
    proceed = ((byte & 128U) != 0U);
    index = (index + 1U);
  }
  string_t result = String((ptr + index), "ascii", "ignore", length);
  return String(result);
}

auto get_declaring_class(uintptr_t method) {
  uintptr_t declaring_class_gc_root = AccessField(method, 0U);
  uintptr_t declaring_class_ref = AccessField(declaring_class_gc_root, 0U);
  uint32_t declaring_class_ptr = Read4(AccessField(declaring_class_ref, 0U));
  uint32_t declaring_class = declaring_class_ptr;
  return declaring_class;
}

auto get_method_trace_id(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uintptr_t signature = AccessField(Read4(AccessField(dexfile, 32U)), 12U);
  uint32_t dex_id = Read4(signature);
  dex_id = dex_id;
  uint32_t method_id = Read4(AccessField(method, 20U));
  return GetMethodTraceId(dex_id, method_id);
}

auto get_method_name(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 20U));
  uintptr_t method_id =
      AccessArrayItem(Read4(AccessField(dexfile, 48U)), dex_method_index, 8U);
  uint32_t name_idx = Read4(AccessField(method_id, 4U));
  return get_dexfile_string_by_idx(dexfile, name_idx);
}

auto get_class_descriptor(uintptr_t cls) {
  auto dexfile = get_class_dexfile(cls);
  uint32_t typeidx = Read4(AccessField(cls, 92U));
  uintptr_t typeid_ =
      AccessArrayItem(Read4(AccessField(dexfile, 40U)), typeidx, 4U);
  uint32_t descriptor_idx = Read4(AccessField(typeid_, 0U));
  return get_dexfile_string_by_idx(dexfile, descriptor_idx);
}

auto get_method_shorty(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 20U));
  uintptr_t method_id =
      AccessArrayItem(Read4(AccessField(dexfile, 48U)), dex_method_index, 8U);
  uint16_t proto_idx = Read2(AccessField(method_id, 2U));
  uintptr_t method_proto_id =
      AccessArrayItem(Read4(AccessField(dexfile, 52U)), proto_idx, 12U);
  return get_dexfile_string_by_idx(
      dexfile, Read4(AccessField(method_proto_id, 0U)));
}

auto get_number_of_refs_without_receiver(uintptr_t method) {
  auto shorty = get_method_shorty(method);
  return CountShortyRefs(shorty);
}

auto is_runtime_method(uintptr_t method) {
  uint32_t dex_method_index = Read4(AccessField(method, 20U));
  bool is_runtime_method = (dex_method_index == 4294967295U);
  return is_runtime_method;
}

auto is_proxy_method(uintptr_t method) {
  auto declaring_class = get_declaring_class(method);
  uint32_t class_access_flags = Read4(AccessField(declaring_class, 44U));
  uint32_t kAccClassIsProxy = 262144U;
  if ((class_access_flags & kAccClassIsProxy)) {
    return true;
  } else {
    return false;
  }
}

auto is_static_method(uintptr_t method) {
  uint32_t access_flags = Read4(AccessField(method, 12U));
  uint32_t kAccStatic = 8U;
  if ((access_flags & kAccStatic)) {
    return true;
  } else {
    return false;
  }
}

auto is_direct_method(uintptr_t method) {
  uint32_t access_flags = Read4(AccessField(method, 12U));
  uint32_t kAccStatic = 8U;
  uint32_t kAccPrivate = 2U;
  uint32_t kAccConstructor = 65536U;
  if ((access_flags & ((kAccStatic | kAccPrivate) | kAccConstructor))) {
    return true;
  } else {
    return false;
  }
}

auto is_native_method(uintptr_t method) {
  uint32_t access_flags = Read4(AccessField(method, 12U));
  uint32_t kAccNative = 256U;
  if ((access_flags & kAccNative)) {
    return true;
  } else {
    return false;
  }
}

auto is_quick_resolution_stub(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint32_t class_linker = Read4(AccessField(runtime, 236U));
  uintptr_t entry_points = AccessField(AccessField(thread, 128U), 136U);
  return (
      (Read4(AccessField(class_linker, 288U)) == entry_point) ||
      (Read4(AccessField(entry_points, 312U)) == entry_point));
}

auto is_quick_to_interpreter_bridge(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint32_t class_linker = Read4(AccessField(runtime, 236U));
  uintptr_t entry_points = AccessField(AccessField(thread, 128U), 136U);
  return (
      (Read4(AccessField(class_linker, 300U)) == entry_point) ||
      (Read4(AccessField(entry_points, 316U)) == entry_point));
}

auto is_quick_generic_jni_stub(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint32_t class_linker = Read4(AccessField(runtime, 236U));
  uintptr_t entry_points = AccessField(AccessField(thread, 128U), 136U);
  return (
      (Read4(AccessField(class_linker, 296U)) == entry_point) ||
      (Read4(AccessField(entry_points, 208U)) == entry_point));
}

auto get_quick_entry_point_from_compiled_code(uintptr_t method) {
  uintptr_t ptr_fields = AccessField(method, 28U);
  uint32_t entry_point = Read4(AccessField(ptr_fields, 8U));
  entry_point = entry_point;
  return entry_point;
}

auto get_quick_frame_info_from_entry_point(uintptr_t entry_point) {
  entry_point = entry_point;
  entry_point = (entry_point & (~1U));
  uint32_t header_offset = 28U;
  uintptr_t oat_method = (entry_point - header_offset);
  return AccessField(oat_method, 12U);
}

auto get_frame_size(
    uintptr_t frameptr,
    uintptr_t runtime_obj,
    uintptr_t thread_obj) {
  uintptr_t method = frameptr;
  auto entry_point = get_quick_entry_point_from_compiled_code(method);
  uint32_t size = 0U;
  uintptr_t callee_save_methods = AccessField(runtime_obj, 0U);
  uintptr_t callee_save_infos = AccessField(runtime_obj, 52U);
  uint32_t kSaveAll = 0U;
  uint32_t kRefsOnly = 1U;
  uint32_t kRefsAndArgs = 2U;
  uint32_t method_info = 0U;
  uint32_t access_flags = Read4(AccessField(method, 12U));
  if ((access_flags & 1024U)) {
    method_info = AccessArrayItem(callee_save_infos, kRefsAndArgs, 12U);
    size = Read4(AccessField(method_info, 0U));
    return size;
  }
  if (is_runtime_method(method)) {
    if ((frameptr ==
         Read8(AccessArrayItem(callee_save_methods, kRefsAndArgs, 8U)))) {
      method_info = AccessArrayItem(callee_save_infos, kRefsAndArgs, 12U);
    } else {
      if ((frameptr ==
           Read8(AccessArrayItem(callee_save_methods, kSaveAll, 8U)))) {
        method_info = AccessArrayItem(callee_save_infos, kSaveAll, 12U);
      } else {
        method_info = AccessArrayItem(callee_save_infos, kRefsOnly, 12U);
      }
    }
    size = Read4(AccessField(method_info, 0U));
    return size;
  }
  if (is_proxy_method(method)) {
    if (is_direct_method(method)) {
      auto info = get_quick_frame_info_from_entry_point(entry_point);
      size = Read4(AccessField(info, 0U));
      return size;
    } else {
      method_info = AccessArrayItem(callee_save_infos, kRefsAndArgs, 12U);
      size = Read4(AccessField(method_info, 0U));
      return size;
    }
  }
  uint32_t code = 0U;
  bool is_native = false;
  if ((is_quick_resolution_stub(entry_point, runtime_obj, thread_obj) ||
       is_quick_to_interpreter_bridge(entry_point, runtime_obj, thread_obj))) {
    if (is_native_method(method)) {
      is_native = true;
    } else {
      ;
    }
  }
  code = entry_point;
  if ((is_native || is_quick_generic_jni_stub(code, runtime_obj, thread_obj))) {
    uintptr_t callee_info =
        AccessArrayItem(callee_save_infos, kRefsAndArgs, 12U);
    uint32_t callee_info_size = Read4(AccessField(callee_info, 0U));
    uint32_t voidptr_size = 4U;
    uint32_t artmethodptr_size = 4U;
    auto num_refs = (get_number_of_refs_without_receiver(method) + 1U);
    uint32_t handle_scope_size = (8U + (4U * num_refs));
    size =
        (((callee_info_size - voidptr_size) + artmethodptr_size) +
         handle_scope_size);
    uint32_t kStackAlignment = 16U;
    size = (size + (kStackAlignment - (size % kStackAlignment)));
    return size;
  }
  auto frame_info = get_quick_frame_info_from_entry_point(code);
  size = Read4(AccessField(frame_info, 0U));
  return size;
}

auto unwind(unwind_callback_t __unwind_callback, void* __unwind_data) {
  uintptr_t thread = get_art_thread();
  if ((thread == 0U)) {
    return true;
  }
  uintptr_t runtime = get_runtime();
  uintptr_t thread_obj = thread;
  uintptr_t runtime_obj = runtime;
  uintptr_t tls = AccessField(thread_obj, 128U);
  uintptr_t mstack = AccessField(tls, 12U);
  uint32_t generic_jni_trampoline =
      Read4(AccessField(AccessField(tls, 136U), 208U));
  while ((mstack != 0U)) {
    uint32_t quick_frame = Read4(AccessField(mstack, 0U));
    quick_frame = quick_frame;
    uint32_t shadow_frame = Read4(AccessField(mstack, 8U));
    shadow_frame = shadow_frame;
    uint32_t kMaxFrames = 1024U;
    uint32_t depth = 0U;
    if ((quick_frame != 0U)) {
      while (((quick_frame != 0U) && (depth < kMaxFrames))) {
        uint32_t frameptr = Read4(quick_frame);
        if ((frameptr == 0U)) {
          break;
        }
        uint32_t frame = frameptr;
        auto size = get_frame_size(frameptr, runtime_obj, thread_obj);
        if ((!is_runtime_method(frame))) {
          if ((!__unwind_callback(frame, __unwind_data))) {
            return false;
          }
        }
        quick_frame = (quick_frame + size);
        depth = (depth + 1U);
      }
    } else {
      if ((shadow_frame != 0U)) {
        while (((shadow_frame != 0U) && (depth < kMaxFrames))) {
          uint32_t frame_obj = shadow_frame;
          uint32_t artmethodptr = Read4(AccessField(frame_obj, 8U));
          uint32_t artmethod = artmethodptr;
          if ((!is_runtime_method(artmethod))) {
            if ((!__unwind_callback(artmethod, __unwind_data))) {
              return false;
            }
          }
          shadow_frame = Read4(AccessField(frame_obj, 4U));
          depth = (depth + 1U);
        }
      }
    }
    uint32_t link = Read4(AccessField(mstack, 4U));
    mstack = link;
  }
  return true;
}
