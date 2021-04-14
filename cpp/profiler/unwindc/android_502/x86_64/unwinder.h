// @nolint
// @generated
struct OatMethod {
  uintptr_t begin_uintptr;
  uintptr_t offset_uintptr;
  bool success_b;
};
struct OatClass {
  uintptr_t oat_file_uintptr;
  intptr_t status_intptr;
  uintptr_t type_uintptr;
  uintptr_t bitmap_size_uintptr;
  uintptr_t bitmap_ptr_uintptr;
  uintptr_t methods_ptr_uintptr;
  bool success_b;
};
struct ArraySlice {
  uintptr_t array_uintptr;
  uintptr_t size_uintptr;
  uintptr_t element_size_uintptr;
};
auto get_runtime_from_thread(uintptr_t thread) {
  uint64_t jni_env = Read8(AccessField(AccessField(thread, 120UL), 64UL));
  uint64_t java_vm = Read8(AccessField(jni_env, 16UL));
  uint64_t runtime = Read8(AccessField(java_vm, 8UL));
  return runtime;
}

auto get_runtime() {
  return get_runtime_from_thread(get_art_thread());
}

auto get_class_dexfile(uintptr_t cls) {
  uintptr_t dexcache_heap_ref = AccessField(cls, 16UL);
  uint32_t dexcache_ptr = Read4(AccessField(dexcache_heap_ref, 0UL));
  uint32_t dexcache = dexcache_ptr;
  uint64_t dexfile = Read8(AccessField(dexcache, 32UL));
  return dexfile;
}

auto get_dexfile_string_by_idx(uintptr_t dexfile, uintptr_t idx) {
  idx = idx;
  uintptr_t id = AccessArrayItem(Read8(AccessField(dexfile, 72UL)), idx, 4UL);
  uint64_t begin = Read8(AccessField(dexfile, 8UL));
  uint32_t string_data_off = Read4(AccessField(id, 0UL));
  uintptr_t ptr = AdvancePointer(begin, (string_data_off * 1UL));
  uintptr_t val = ptr;
  uintptr_t length = 0UL;
  uintptr_t index = 0UL;
  bool proceed = true;
  while (proceed) {
    uint8_t byte = Read1(AccessArrayItem(val, index, 1UL));
    length = (length | ((byte & 127UL) << (index * 7UL)));
    proceed = ((byte & 128UL) != 0UL);
    index = (index + 1UL);
  }
  string_t result =
      String(AdvancePointer(ptr, (index * 1UL)), "ascii", "ignore", length);
  return String(result);
}

auto get_declaring_class(uintptr_t method) {
  uintptr_t declaring_class_ref = AccessField(method, 8UL);
  uint32_t declaring_class_ptr = Read4(AccessField(declaring_class_ref, 0UL));
  uint32_t declaring_class = declaring_class_ptr;
  return declaring_class;
}

auto get_method_trace_id(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uintptr_t signature = AccessField(Read8(AccessField(dexfile, 64UL)), 12UL);
  uint32_t dex_id = Read4(signature);
  dex_id = dex_id;
  uint32_t method_id = Read4(AccessField(method, 64UL));
  return GetMethodTraceId(dex_id, method_id);
}

auto get_method_name(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 64UL));
  uintptr_t method_id =
      AccessArrayItem(Read8(AccessField(dexfile, 96UL)), dex_method_index, 8UL);
  uint32_t name_idx = Read4(AccessField(method_id, 4UL));
  return get_dexfile_string_by_idx(dexfile, name_idx);
}

auto get_class_descriptor(uintptr_t cls) {
  auto dexfile = get_class_dexfile(cls);
  uint32_t typeidx = Read4(AccessField(cls, 76UL));
  uintptr_t typeid_ =
      AccessArrayItem(Read8(AccessField(dexfile, 80UL)), typeidx, 4UL);
  uint32_t descriptor_idx = Read4(AccessField(typeid_, 0UL));
  return get_dexfile_string_by_idx(dexfile, descriptor_idx);
}

auto get_method_shorty(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 64UL));
  uintptr_t method_id =
      AccessArrayItem(Read8(AccessField(dexfile, 96UL)), dex_method_index, 8UL);
  uint16_t proto_idx = Read2(AccessField(method_id, 2UL));
  uintptr_t method_proto_id =
      AccessArrayItem(Read8(AccessField(dexfile, 104UL)), proto_idx, 12UL);
  uint32_t shorty_id = Read4(AccessField(method_proto_id, 0UL));
  return get_dexfile_string_by_idx(dexfile, shorty_id);
}

auto get_number_of_refs_without_receiver(uintptr_t method) {
  auto shorty = get_method_shorty(method);
  return CountShortyRefs(shorty);
}

auto get_method_access_flags(uintptr_t method) {
  uint32_t access_flags = Read4(AccessField(method, 56UL));
  return access_flags;
}

auto is_runtime_method(uintptr_t method) {
  uint32_t dex_method_index = Read4(AccessField(method, 64UL));
  bool is_runtime_method = (dex_method_index == 4294967295UL);
  return is_runtime_method;
}

auto is_proxy_method(uintptr_t method) {
  auto declaring_class = get_declaring_class(method);
  uint32_t class_access_flags = Read4(AccessField(declaring_class, 60UL));
  uintptr_t kAccClassIsProxy = 262144UL;
  if ((class_access_flags & kAccClassIsProxy)) {
    return true;
  } else {
    return false;
  }
}

auto is_static_method(uintptr_t method) {
  uintptr_t kAccStatic = 8UL;
  if ((get_method_access_flags(method) & kAccStatic)) {
    return true;
  } else {
    return false;
  }
}

auto is_direct_method(uintptr_t method) {
  uintptr_t kAccStatic = 8UL;
  uintptr_t kAccPrivate = 2UL;
  uintptr_t kAccConstructor = 65536UL;
  if ((get_method_access_flags(method) &
       ((kAccStatic | kAccPrivate) | kAccConstructor))) {
    return true;
  } else {
    return false;
  }
}

auto is_native_method(uintptr_t method) {
  uintptr_t kAccNative = 256UL;
  if ((get_method_access_flags(method) & kAccNative)) {
    return true;
  } else {
    return false;
  }
}

auto is_quick_resolution_stub(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint64_t class_linker = Read8(AccessField(runtime, 368UL));
  uintptr_t entry_points = AccessField(AccessField(thread, 120UL), 320UL);
  return (
      (Read8(AccessField(class_linker, 376UL)) == entry_point) ||
      (Read8(AccessField(entry_points, 504UL)) == entry_point));
}

auto is_quick_to_interpreter_bridge(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint64_t class_linker = Read8(AccessField(runtime, 368UL));
  uintptr_t entry_points = AccessField(AccessField(thread, 120UL), 320UL);
  return (
      (Read8(AccessField(class_linker, 408UL)) == entry_point) ||
      (Read8(AccessField(entry_points, 512UL)) == entry_point));
}

auto is_quick_generic_jni_stub(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint64_t class_linker = Read8(AccessField(runtime, 368UL));
  uintptr_t entry_points = AccessField(AccessField(thread, 120UL), 320UL);
  return (
      (Read8(AccessField(class_linker, 400UL)) == entry_point) ||
      (Read8(AccessField(entry_points, 296UL)) == entry_point));
}

auto get_quick_entry_point_from_compiled_code(uintptr_t method) {
  uint64_t entry_point = Read8(AccessField(method, 40UL));
  entry_point = entry_point;
  return entry_point;
}

auto get_oat_method_header_from_entry_point(uintptr_t entry_point) {
  entry_point = entry_point;
  entry_point = (entry_point & (~1UL));
  uintptr_t header_offset = 24UL;
  uintptr_t oat_method_header = (entry_point - header_offset);
  return oat_method_header;
}

auto get_quick_frame_info_from_entry_point(uintptr_t entry_point) {
  auto oat_method_header = get_oat_method_header_from_entry_point(entry_point);
  return AccessField(oat_method_header, 8UL);
}

auto round_up(uintptr_t x, uintptr_t n) {
  uintptr_t arg1 = ((x + n) - 1UL);
  uintptr_t arg2 = n;
  return (arg1 & (-arg2));
}

auto is_abstract_method(uintptr_t method) {
  uintptr_t kAccAbstract = 1024UL;
  return (get_method_access_flags(method) & kAccAbstract);
}

auto get_frame_size(
    uintptr_t frameptr,
    uintptr_t runtime_obj,
    uintptr_t thread_obj,
    uintptr_t pc) {
  uintptr_t method = frameptr;
  auto entry_point = get_quick_entry_point_from_compiled_code(method);
  uint32_t size = 0U;
  uintptr_t callee_save_methods = AccessField(runtime_obj, 0UL);
  uintptr_t callee_save_infos = AccessField(runtime_obj, 68UL);
  uintptr_t kSaveAll = 0UL;
  uintptr_t kRefsOnly = 1UL;
  uintptr_t kRefsAndArgs = 2UL;
  uintptr_t method_info = 0UL;
  if (is_abstract_method(method)) {
    method_info = AccessArrayItem(callee_save_infos, kRefsAndArgs, 12UL);
    size = Read4(AccessField(method_info, 0UL));
    return size;
  }
  if (is_runtime_method(method)) {
    if ((frameptr == AccessArrayItem(callee_save_methods, kRefsAndArgs, 8UL))) {
      method_info = AccessArrayItem(callee_save_infos, kRefsAndArgs, 12UL);
    } else {
      if ((frameptr == AccessArrayItem(callee_save_methods, kSaveAll, 8UL))) {
        method_info = AccessArrayItem(callee_save_infos, kSaveAll, 12UL);
      } else {
        method_info = AccessArrayItem(callee_save_infos, kRefsOnly, 12UL);
      }
    }
    size = Read4(AccessField(method_info, 0UL));
    return size;
  }
  if (is_proxy_method(method)) {
    if (is_direct_method(method)) {
      auto info = get_quick_frame_info_from_entry_point(entry_point);
      size = Read4(AccessField(info, 0UL));
      return size;
    } else {
      method_info = AccessArrayItem(callee_save_infos, kRefsAndArgs, 12UL);
      size = Read4(AccessField(method_info, 0UL));
      return size;
    }
  }
  uintptr_t code = 0UL;
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
        AccessArrayItem(callee_save_infos, kRefsAndArgs, 12UL);
    uint32_t callee_info_size = Read4(AccessField(callee_info, 0UL));
    uintptr_t voidptr_size = 8UL;
    uintptr_t artmethodptr_size = 8UL;
    auto num_refs = (get_number_of_refs_without_receiver(method) + 1UL);
    uintptr_t handle_scope_size = (12UL + (4UL * num_refs));
    size =
        (((callee_info_size - voidptr_size) + artmethodptr_size) +
         handle_scope_size);
    uintptr_t kStackAlignment = 16UL;
    size = round_up(size, kStackAlignment);
    return size;
  }
  auto frame_info = get_quick_frame_info_from_entry_point(code);
  size = Read4(AccessField(frame_info, 0UL));
  return size;
}

auto unwind(unwind_callback_t __unwind_callback, void* __unwind_data) {
  uintptr_t thread = get_art_thread();
  if ((thread == 0UL)) {
    return true;
  }
  auto runtime = get_runtime_from_thread(thread);
  uintptr_t thread_obj = thread;
  auto runtime_obj = runtime;
  uintptr_t tls = AccessField(thread_obj, 120UL);
  uintptr_t mstack = AccessField(tls, 24UL);
  uint64_t generic_jni_trampoline =
      Read8(AccessField(AccessField(tls, 320UL), 296UL));
  while ((mstack != 0UL)) {
    uint64_t quick_frame = Read8(AccessField(mstack, 16UL));
    quick_frame = quick_frame;
    uint64_t shadow_frame = Read8(AccessField(mstack, 8UL));
    shadow_frame = shadow_frame;
    uintptr_t pc = 0UL;
    uintptr_t kMaxFrames = 1024UL;
    uintptr_t depth = 0UL;
    if ((quick_frame != 0UL)) {
      while (((quick_frame != 0UL) && (depth < kMaxFrames))) {
        uint64_t frameptr = Read8(quick_frame);
        if ((frameptr == 0UL)) {
          break;
        }
        uint64_t frame = frameptr;
        if ((!is_runtime_method(frame))) {
          if ((!__unwind_callback(frame, __unwind_data))) {
            return false;
          }
        }
        auto size = get_frame_size(frameptr, runtime_obj, thread_obj, pc);
        auto return_pc_offset = (size - 8UL);
        uint64_t return_pc_addr = (quick_frame + return_pc_offset);
        uint64_t return_pc = return_pc_addr;
        pc = Read8(return_pc);
        quick_frame = (quick_frame + size);
        depth = (depth + 1UL);
      }
    } else {
      if ((shadow_frame != 0UL)) {
        while (((shadow_frame != 0UL) && (depth < kMaxFrames))) {
          uint64_t frame_obj = shadow_frame;
          uint64_t artmethodptr = Read8(AccessField(frame_obj, 16UL));
          uint64_t artmethod = artmethodptr;
          if ((!is_runtime_method(artmethod))) {
            if ((!__unwind_callback(artmethod, __unwind_data))) {
              return false;
            }
          }
          shadow_frame = Read8(AccessField(frame_obj, 8UL));
          depth = (depth + 1UL);
        }
      }
    }
    uint64_t link = Read8(AccessField(mstack, 0UL));
    if ((link == 0UL)) {
      break;
    }
    mstack = link;
  }
  return true;
}
