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
  uint32_t jni_env = Read4(AccessField(AccessField(thread, 152U), 28U));
  uint32_t java_vm = Read4(AccessField(jni_env, 8U));
  uint32_t runtime = Read4(AccessField(java_vm, 4U));
  return runtime;
}

auto get_runtime() {
  return get_runtime_from_thread(get_art_thread());
}

auto get_class_dexfile(uintptr_t cls) {
  uintptr_t dexcache_heap_ref = AccessField(cls, 16U);
  uintptr_t dexcache_ptr = AccessField(dexcache_heap_ref, 0U);
  dexcache_ptr = Read4(AccessField(dexcache_ptr, 0U));
  uintptr_t dexcache = dexcache_ptr;
  uint64_t dexfile = Read8(AccessField(dexcache, 16U));
  return dexfile;
}

auto get_dexfile_string_by_idx(uintptr_t dexfile, uintptr_t idx) {
  idx = idx;
  uintptr_t id = AccessArrayItem(Read4(AccessField(dexfile, 40U)), idx, 4U);
  uint32_t begin = Read4(AccessField(dexfile, 12U));
  uint32_t string_data_off = Read4(AccessField(id, 0U));
  uintptr_t ptr = AdvancePointer(begin, (string_data_off * 1U));
  uintptr_t val = ptr;
  uintptr_t length = 0U;
  uintptr_t index = 0U;
  bool proceed = true;
  while (proceed) {
    uint8_t byte = Read1(AccessArrayItem(val, index, 1U));
    length = (length | ((byte & 127U) << (index * 7U)));
    proceed = ((byte & 128U) != 0U);
    index = (index + 1U);
  }
  string_t result =
      String(AdvancePointer(ptr, (index * 1U)), "ascii", "ignore", length);
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
  uintptr_t signature = AccessField(Read4(AccessField(dexfile, 36U)), 12U);
  uint32_t dex_id = Read4(signature);
  dex_id = dex_id;
  uint32_t method_id = Read4(AccessField(method, 12U));
  return GetMethodTraceId(dex_id, method_id);
}

auto get_method_name(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 12U));
  uintptr_t method_id =
      AccessArrayItem(Read4(AccessField(dexfile, 52U)), dex_method_index, 8U);
  uintptr_t name_idx = AccessField(method_id, 4U);
  name_idx = Read4(AccessField(name_idx, 0U));
  return get_dexfile_string_by_idx(dexfile, name_idx);
}

auto get_class_descriptor(uintptr_t cls) {
  auto dexfile = get_class_dexfile(cls);
  uint32_t typeidx = Read4(AccessField(cls, 84U));
  uintptr_t typeid_ =
      AccessArrayItem(Read4(AccessField(dexfile, 44U)), typeidx, 4U);
  uintptr_t descriptor_idx = AccessField(typeid_, 0U);
  descriptor_idx = Read4(AccessField(descriptor_idx, 0U));
  return get_dexfile_string_by_idx(dexfile, descriptor_idx);
}

auto get_method_shorty(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 12U));
  uintptr_t method_id =
      AccessArrayItem(Read4(AccessField(dexfile, 52U)), dex_method_index, 8U);
  uint16_t proto_idx = Read2(AccessField(method_id, 2U));
  uintptr_t method_proto_id =
      AccessArrayItem(Read4(AccessField(dexfile, 56U)), proto_idx, 12U);
  uintptr_t shorty_id = AccessField(method_proto_id, 0U);
  shorty_id = Read4(AccessField(shorty_id, 0U));
  return get_dexfile_string_by_idx(dexfile, shorty_id);
}

auto get_number_of_refs_without_receiver(uintptr_t method) {
  auto shorty = get_method_shorty(method);
  return CountShortyRefs(shorty);
}

auto get_method_access_flags(uintptr_t method) {
  uintptr_t access_flags = AccessField(method, 4U);
  access_flags = Read4(AccessField(access_flags, 0U));
  return access_flags;
}

auto is_runtime_method(uintptr_t method) {
  uint32_t dex_method_index = Read4(AccessField(method, 12U));
  bool is_runtime_method = (dex_method_index == 4294967295U);
  return is_runtime_method;
}

auto is_proxy_method(uintptr_t method) {
  auto declaring_class = get_declaring_class(method);
  uint32_t class_access_flags = Read4(AccessField(declaring_class, 64U));
  uintptr_t kAccClassIsProxy = 262144U;
  if ((class_access_flags & kAccClassIsProxy)) {
    return true;
  } else {
    return false;
  }
}

auto is_static_method(uintptr_t method) {
  uintptr_t kAccStatic = 8U;
  if ((get_method_access_flags(method) & kAccStatic)) {
    return true;
  } else {
    return false;
  }
}

auto is_direct_method(uintptr_t method) {
  uintptr_t kAccStatic = 8U;
  uintptr_t kAccPrivate = 2U;
  uintptr_t kAccConstructor = 65536U;
  if ((get_method_access_flags(method) &
       ((kAccStatic | kAccPrivate) | kAccConstructor))) {
    return true;
  } else {
    return false;
  }
}

auto is_native_method(uintptr_t method) {
  uintptr_t kAccNative = 256U;
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
  uint32_t class_linker = Read4(AccessField(runtime, 280U));
  uintptr_t entry_points = AccessField(AccessField(thread, 152U), 156U);
  return (
      (Read4(AccessField(class_linker, 168U)) == entry_point) ||
      (Read4(AccessField(entry_points, 392U)) ==
       entry_point)); // GetQuickResolutionStub
}

auto is_quick_to_interpreter_bridge(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint32_t class_linker = Read4(AccessField(runtime, 280U));
  uintptr_t entry_points = AccessField(AccessField(thread, 152U), 156U);
  return (
      (Read4(AccessField(class_linker, 180U)) == entry_point) ||
      (Read4(AccessField(entry_points, 396U)) ==
       entry_point)); // GetQuickToInterpreterBridge
}

auto is_quick_generic_jni_stub(
    uintptr_t entry_point,
    uintptr_t runtime,
    uintptr_t thread) {
  uint32_t class_linker = Read4(AccessField(runtime, 280U));
  uintptr_t entry_points = AccessField(AccessField(thread, 152U), 156U);
  return (
      (Read4(AccessField(class_linker, 176U)) == entry_point) ||
      (Read4(AccessField(entry_points, 216U)) ==
       entry_point)); // GetQuickGenericJniStub
}

auto get_quick_entry_point_from_compiled_code(uintptr_t method) {
  uintptr_t ptr_fields = AccessField(method, 20U);
  uint32_t entry_point = Read4(AccessField(ptr_fields, 4U));
  entry_point = entry_point;
  return entry_point;
}

auto get_oat_method_header_from_entry_point(uintptr_t entry_point) {
  entry_point = entry_point;
  entry_point = (entry_point & (~1U));
  uintptr_t header_offset = 8U;
  uintptr_t oat_method_header = (entry_point - header_offset);
  return oat_method_header;
}

auto get_quick_frame_info_from_entry_point(uintptr_t entry_point) {
  auto oat_method_header = get_oat_method_header_from_entry_point(entry_point);
  return AccessField(oat_method_header, 8U);
}

auto method_header_contains(uintptr_t method_header, uintptr_t pc) {
  uintptr_t code = AccessField(method_header, 8U);
  uint32_t code_size = Read4(AccessField(method_header, 4U));
  uintptr_t kCodeSizeMask = (~2147483648U);
  code_size = (code_size & kCodeSizeMask);
  return ((code <= pc) && (pc <= (code + code_size)));
}

auto is_resolved(uintptr_t cls) {
  uint32_t status = Read4(AccessField(cls, 112U));
  status = (status >> (32U - 4U));
  uintptr_t kStatusResolved = 7U;
  uintptr_t kStatusErrorResolved = 2U;
  return ((status >= 4U) || (status == kStatusErrorResolved));
}

auto get_oat_class(uintptr_t oat_dex_file, uintptr_t class_def_idx) {
  uint32_t oat_class_offsets_pointer = Read4(AccessField(oat_dex_file, 52U));
  uintptr_t oat_class_offset =
      AdvancePointer(oat_class_offsets_pointer, (class_def_idx * 4U));
  oat_class_offset = Read4(oat_class_offset);
  uint32_t oat_file = Read4(AccessField(oat_dex_file, 0U));
  uint32_t oat_file_begin = Read4(AccessField(oat_file, 20U));
  uintptr_t oat_class_pointer =
      AdvancePointer(oat_file_begin, (oat_class_offset * 1U));

  uintptr_t status_pointer = oat_class_pointer;
  uint16_t status = Read2(status_pointer);
  uintptr_t kStatusMax = 15U;

  uintptr_t type_pointer = AdvancePointer(status_pointer, (2U * 1U));

  uint16_t oat_type = Read2(type_pointer);
  uintptr_t kOatClassMax = 3U;

  uintptr_t after_type_pointer = AdvancePointer(type_pointer, (2U * 1U));

  uintptr_t bitmap_size = 0U;
  uintptr_t bitmap_pointer = 0U;
  uintptr_t methods_pointer = 0U;
  uintptr_t kOatClassNoneCompiled = 2U;
  if ((oat_type != kOatClassNoneCompiled)) {
    uintptr_t kOatClassSomeCompiled = 1U;
    if ((oat_type == kOatClassSomeCompiled)) {
      bitmap_size = Read4(after_type_pointer);
      bitmap_pointer = AdvancePointer(after_type_pointer, (4U * 1U));

      methods_pointer = AdvancePointer(bitmap_pointer, (bitmap_size * 1U));
    } else {
      methods_pointer = after_type_pointer;
    }
  }
  return OatClass{
      .oat_file_uintptr = oat_file,
      .status_intptr = status,
      .type_uintptr = oat_type,
      .bitmap_size_uintptr = bitmap_size,
      .bitmap_ptr_uintptr = bitmap_pointer,
      .methods_ptr_uintptr = methods_pointer,
      .success_b = true,
  };
}

auto find_oat_class(uintptr_t cls) {
  auto dex_file = get_class_dexfile(cls);
  uint32_t class_def_idx = Read4(AccessField(cls, 80U));
  uintptr_t kDexNoIndex16 = 65535U;

  uint32_t oat_dex_file = Read4(AccessField(dex_file, 84U));
  if (((oat_dex_file == 0U) || (Read4(AccessField(oat_dex_file, 0U)) == 0U))) {
    return OatClass{
        .oat_file_uintptr = 0,
        .status_intptr = -1,
        .type_uintptr = 2,
        .bitmap_size_uintptr = 0,
        .bitmap_ptr_uintptr = 0,
        .methods_ptr_uintptr = 0,
        .success_b = false,
    };
  } else {
    return get_oat_class(oat_dex_file, class_def_idx);
  }
}

auto count_bits_in_word(uintptr_t word) {
  uintptr_t count = 0U;
  while ((word > 0U)) {
    if ((word & 1U)) {
      count = (count + 1U);
    }
    word = (word >> 1U);
  }
  return count;
}

auto get_oat_method_offsets(
    struct OatClass const& struct_OatClass,
    uintptr_t method_index) {
  uintptr_t methods_ptr = struct_OatClass.methods_ptr_uintptr;
  uintptr_t oc_type = struct_OatClass.type_uintptr;
  uintptr_t bitmap_ptr = struct_OatClass.bitmap_ptr_uintptr;
  if ((methods_ptr == 0U)) {
    uintptr_t kOatClassNoneCompiled = 2U;

    return methods_ptr;
  }
  uintptr_t methods_pointer_index = 0U;
  if ((bitmap_ptr == 0U)) {
    uintptr_t kOatClassAllCompiled = 0U;

    methods_pointer_index = method_index;
  } else {
    uintptr_t kOatClassSomeCompiled = 1U;

    uintptr_t word_index = (method_index >> 5U);
    uintptr_t bit_mask = (1U << (method_index & 31U));
    uintptr_t is_bit_set = AdvancePointer(bitmap_ptr, (word_index * 4U));
    is_bit_set = Read4(is_bit_set);
    is_bit_set = ((is_bit_set & bit_mask) != 0U);
    if ((!is_bit_set)) {
      return is_bit_set;
    }
    uintptr_t word_end = (method_index >> 5U);
    uintptr_t partial_word_bits = (method_index & 31U);
    uintptr_t count = 0U;
    uintptr_t word = 0U;
    uintptr_t elem = 0U;
    while ((word < word_end)) {
      elem = AdvancePointer(bitmap_ptr, (word * 4U));
      elem = Read4(elem);
      count = (count + count_bits_in_word(elem));
      word = (word + 1U);
    }
    if ((partial_word_bits != 0U)) {
      elem = AdvancePointer(bitmap_ptr, (word_end * 4U));
      elem = Read4(elem);
      uint32_t shifted = 4294967295U;
      count =
          (count +
           count_bits_in_word((elem & (~(shifted << partial_word_bits)))));
    }
    methods_pointer_index = count;
  }
  uintptr_t ret = AdvancePointer(methods_ptr, (methods_pointer_index * 4U));
  return ret;
}

auto runtime_is_aot_compiler(uintptr_t runtime, uintptr_t instance) {
  uint32_t jit =
      Read4(AccessField(AccessField(AccessField(runtime, 292U), 0U), 0U));
  bool use_jit_compilation =
      ((jit != 0U) && Read1(AccessFieldDesc(jit, 96416U)));
  uint32_t compiler_callbacks = Read4(AccessField(runtime, 84U));
  return ((!use_jit_compilation) && (compiler_callbacks != 0U));
}

auto get_oat_method(
    uintptr_t runtime_obj,
    struct OatClass const& struct_OatClass,
    uintptr_t oat_method_index) {
  auto oat_method_offsets =
      get_oat_method_offsets(struct_OatClass, oat_method_index);
  if ((oat_method_offsets == 0U)) {
    return OatMethod{
        .begin_uintptr = 0,
        .offset_uintptr = 0,
        .success_b = true,
    };
  }
  uintptr_t runtime_current = get_runtime();
  uintptr_t begin_uintptr = 0U;
  uintptr_t oat_file = struct_OatClass.oat_file_uintptr;
  if ((Read1(AccessField(oat_file, 52U)) ||
       ((runtime_current == 0U) ||
        runtime_is_aot_compiler(runtime_current, runtime_current)))) {
    begin_uintptr = Read4(AccessField(oat_file, 20U));
    uint32_t offset_uintptr = Read4(AccessField(oat_method_offsets, 0U));
    return OatMethod{
        .begin_uintptr = begin_uintptr,
        .offset_uintptr = offset_uintptr,
        .success_b = true,
    };
  }
  begin_uintptr = Read4(AccessField(oat_file, 20U));
  return OatMethod{
      .begin_uintptr = begin_uintptr,
      .offset_uintptr = 0,
      .success_b = true,
  };
}

auto round_up(uintptr_t x, uintptr_t n) {
  uintptr_t arg1 = ((x + n) - 1U);
  uintptr_t arg2 = n;
  return (arg1 & (-arg2));
}

auto length_prefixed_array_at(
    uintptr_t array,
    uintptr_t idx,
    uintptr_t element_size,
    uintptr_t alignment) {
  uintptr_t ptr = array;

  uintptr_t data_offset = 4U;
  auto element_offset = round_up(data_offset, alignment);
  element_offset = (element_offset + (idx * element_size));
  uintptr_t ret = (ptr + element_offset);
  return ret;
}

auto get_virtual_methods(
    uintptr_t method,
    uintptr_t cls,
    uintptr_t start_offset) {
  uintptr_t ptr_size = 4U;
  uintptr_t num_methods = 0U;
  uint64_t methods_ptr = Read8(AccessField(cls, 48U));
  if ((methods_ptr == 0U)) {
    num_methods = 0U;
  } else {
    num_methods = Read4(AccessField(methods_ptr, 0U));
  }
  uintptr_t end_offset = num_methods;

  uintptr_t size = (end_offset - start_offset);
  if ((size == 0U)) {
    return ArraySlice{
        .array_uintptr = 0,
        .size_uintptr = 0,
        .element_size_uintptr = 0,
    };
  }

  uintptr_t method_size = 20U;
  method_size = round_up(method_size, ptr_size);
  method_size = (method_size + 8U);
  uintptr_t method_alignment = ptr_size;
  auto array_method =
      length_prefixed_array_at(methods_ptr, 0U, method_size, method_alignment);
  uint32_t size_uintptr = Read4(AccessField(methods_ptr, 0U));
  auto array_slice = ArraySlice{
      .array_uintptr = array_method,
      .size_uintptr = size_uintptr,
      .element_size_uintptr = method_size,
  };

  uintptr_t tmp = array_slice.array_uintptr;
  tmp = (tmp + (start_offset * array_slice.element_size_uintptr));
  return ArraySlice{
      .array_uintptr = tmp,
      .size_uintptr = size,
      .element_size_uintptr = array_slice.element_size_uintptr,
  };
}

auto find_oat_method_for(uintptr_t method, uintptr_t runtime_obj) {
  uintptr_t oat_method_index = 0U;
  auto cls = get_declaring_class(method); // done
  if ((is_static_method(method) || is_direct_method(method))) {
    oat_method_index = Read2(AccessField(method, 16U));
  } else {
    oat_method_index = Read2(AccessField(cls, 118U));
    auto virtual_methods = get_virtual_methods(method, cls, oat_method_index);
    uintptr_t iterator = virtual_methods.array_uintptr;
    uintptr_t end =
        (iterator +
         (virtual_methods.size_uintptr * virtual_methods.element_size_uintptr));
    bool found_virtual = false;
    while ((iterator != end)) {
      uintptr_t art_method = iterator;
      if ((Read4(AccessField(art_method, 12U)) ==
           Read4(AccessField(method, 12U)))) {
        found_virtual = true;
        break;
      }
      oat_method_index = (oat_method_index + 1U);
      iterator = (iterator + virtual_methods.element_size_uintptr);
    }
  }
  auto oat_class = find_oat_class(cls);
  if ((!oat_class.success_b)) {
    return OatMethod{
        .begin_uintptr = 0,
        .offset_uintptr = 0,
        .success_b = false,
    };
  }
  return get_oat_method(runtime_obj, oat_class, oat_method_index);
}

auto get_oat_pointer(
    struct OatMethod const& struct_OatMethod,
    uintptr_t offset) {
  if ((offset == 0U)) {
    return offset;
  }
  uintptr_t begin = struct_OatMethod.begin_uintptr;
  return AdvancePointer(begin, (offset * 1U));
}

auto get_code_offset(struct OatMethod const& struct_OatMethod) {
  uintptr_t oat_method_offset = struct_OatMethod.offset_uintptr;
  auto code =
      get_oat_pointer(struct_OatMethod, oat_method_offset); // GetOatPointer
  code = (code & (~1U)); // EntryPointToCodePointer
  if ((code == 0U)) {
    return 0U;
  }
  uintptr_t method_header_size = 8U;
  code = (code - method_header_size);
  uintptr_t kCodeSizeMask = (~2147483648U);
  uint32_t code_size = Read4(AccessField(code, 4U));
  code_size = (code_size & kCodeSizeMask);
  if ((code_size == 0U)) {
    return 0U;
  }
  return (unsigned int)oat_method_offset;
}

auto get_quick_code(struct OatMethod const& struct_OatMethod) {
  auto offset = get_code_offset(struct_OatMethod);
  auto quick_code = get_oat_pointer(struct_OatMethod, offset);
  return quick_code;
}

auto get_oat_quick_method_header(
    uintptr_t method,
    uintptr_t runtime_obj,
    uintptr_t thread_obj,
    uintptr_t pc) {
  if (is_runtime_method(method)) {
    return 0U;
  }
  auto existing_entry_point = get_quick_entry_point_from_compiled_code(method);
  uintptr_t method_header = 0U;
  if (((!is_quick_generic_jni_stub(
           existing_entry_point, runtime_obj, thread_obj)) &&
       (!is_quick_resolution_stub(
           existing_entry_point, runtime_obj, thread_obj)) &&
       (!is_quick_to_interpreter_bridge(
           existing_entry_point, runtime_obj, thread_obj)))) {
    auto entry_point_tmp = existing_entry_point;
    method_header = get_oat_method_header_from_entry_point(entry_point_tmp);
    bool ret = method_header_contains(method_header, pc);
    if (ret) {
      return (unsigned int)method_header;
    }
  }
  auto oat_method = find_oat_method_for(method, runtime_obj);
  if ((!oat_method.success_b)) {
    if (is_quick_resolution_stub(
            existing_entry_point, runtime_obj, thread_obj)) {
      return 0U;
    }
  }
  auto oat_entry_point = get_quick_code(oat_method);
  if (((oat_entry_point == 0U) ||
       is_quick_generic_jni_stub(oat_entry_point, runtime_obj, thread_obj))) {
    return 0U;
  }
  oat_entry_point = oat_entry_point;
  method_header = get_oat_method_header_from_entry_point(oat_entry_point);
  if ((pc == 0U)) {
    return (unsigned int)method_header;
  }
  return (unsigned int)method_header;
}

auto is_abstract_method(uintptr_t method) {
  uintptr_t kAccAbstract = 1024U;
  return (get_method_access_flags(method) & kAccAbstract);
}

uintptr_t read_variant_size(uintptr_t code_info_ptr) {
  uintptr_t tmp = code_info_ptr & 0x0F;
  uint32_t kVarintSmallValue = 11;
  uint32_t kali = 11;
  size_t kStackAlignment = 16;
  if (tmp > kVarintSmallValue) {
    return (code_info_ptr >> 4 & 0x0F) * kStackAlignment;
  } else {
    return tmp * kStackAlignment;
  }
}

auto get_frame_size(
    uintptr_t frameptr,
    uintptr_t runtime_obj,
    uintptr_t thread_obj,
    uintptr_t pc) {
  uintptr_t method = frameptr;
  auto entry_point = get_quick_entry_point_from_compiled_code(method);
  auto oat_quick_method_header =
      get_oat_quick_method_header(method, runtime_obj, thread_obj, pc);
  if ((oat_quick_method_header != 0U)) {
    uint32_t vmap_table_offset =
        Read4(AccessField(oat_quick_method_header, 0U));
    uintptr_t code_info_ptr = Read4(AccessFieldDesc(
        AccessField(oat_quick_method_header, 8U), vmap_table_offset));
    uintptr_t frame_size = read_variant_size(code_info_ptr);
    return (unsigned int)frame_size;
  }
  uint32_t size = 0U;
  uintptr_t callee_save_methods = AccessField(runtime_obj, 0U);
  uintptr_t kSaveAll = 0U;
  uintptr_t kRefsOnly = 1U;
  uintptr_t kRefsAndArgs = 2U;
  uintptr_t method_info = 0U;
  if (is_abstract_method(method)) {
    return 112U;
  }
  if (is_runtime_method(method)) {
    if ((frameptr ==
         Read8(AccessArrayItem(callee_save_methods, kRefsAndArgs, 8U)))) {
      return 112U;
    } else {
      if ((frameptr ==
           Read8(AccessArrayItem(callee_save_methods, kSaveAll, 8U)))) {
        return 88U;
      } else {
        return 48U;
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
      return 112U;
    }
  }
  uintptr_t code = 0U;
  bool is_native = false;
  if ((is_quick_resolution_stub(entry_point, runtime_obj, thread_obj) ||
       is_quick_to_interpreter_bridge(entry_point, runtime_obj, thread_obj))) {
    if (is_native_method(method)) {
      is_native = true;
    } else {
    }
  }
  code = entry_point;
  if ((is_native || is_quick_generic_jni_stub(code, runtime_obj, thread_obj))) {
    uint32_t callee_info_size = 112U;
    uintptr_t voidptr_size = 8U;
    uintptr_t artmethodptr_size = 8U;
    auto num_refs = (get_number_of_refs_without_receiver(method) + 1U);
    uintptr_t handle_scope_size = (12U + (4U * num_refs));
    size =
        (((callee_info_size - voidptr_size) + artmethodptr_size) +
         handle_scope_size);
    uintptr_t kStackAlignment = 16U;
    size = round_up(size, kStackAlignment);
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
  auto runtime = get_runtime_from_thread(thread);
  uintptr_t thread_obj = thread;
  auto runtime_obj = runtime;
  uintptr_t tls = AccessField(thread_obj, 152U);
  uintptr_t mstack = AccessField(tls, 12U);
  while ((mstack != 0U)) {
    uint32_t quick_frame =
        (Read4(AccessField(AccessField(mstack, 0U), 0U)) & (~1U));
    quick_frame = quick_frame;
    uint32_t shadow_frame = Read4(AccessField(mstack, 8U));
    shadow_frame = shadow_frame;
    uintptr_t pc = 0U;
    uintptr_t kMaxFrames = 1024U;
    uintptr_t depth = 0U;
    if ((quick_frame != 0U)) {
      while (((quick_frame != 0U) && (depth < kMaxFrames))) {
        uint32_t frameptr = Read4(quick_frame);
        if ((frameptr == 0U)) {
          break;
        }
        uint32_t frame = frameptr;
        if ((!is_runtime_method(frame))) {
          if ((!__unwind_callback(frame, __unwind_data))) {
            return false;
          }
        }
        auto size = get_frame_size(frameptr, runtime_obj, thread_obj, pc);
        auto return_pc_offset = (size - 4U);
        uint32_t return_pc_addr = (quick_frame + return_pc_offset);
        uint32_t return_pc = return_pc_addr;
        pc = Read4(return_pc);
        quick_frame = (quick_frame + size);
        depth = (depth + 1U);
      }
    } else {
      if ((shadow_frame != 0U)) {
        while (((shadow_frame != 0U) && (depth < kMaxFrames))) {
          uint32_t frame_obj = shadow_frame;
          uint32_t artmethodptr = Read4(AccessField(frame_obj, 4U));
          uint32_t artmethod = artmethodptr;
          if ((!is_runtime_method(artmethod))) {
            if ((!__unwind_callback(artmethod, __unwind_data))) {
              return false;
            }
          }
          shadow_frame = Read4(AccessField(frame_obj, 0U));
          depth = (depth + 1U);
        }
      }
    }
    uint32_t link = Read4(AccessField(mstack, 4U));
    if ((link == 0U)) {
      break;
    }
    mstack = link;
  }
  return true;
}