// @nolint

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
  uint32_t begin = Read4(AccessField(dexfile, 4U));
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
  uintptr_t declaring_class_ref = AccessField(method, 8U);
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
  uint32_t method_id = Read4(AccessField(method, 64U));
  return GetMethodTraceId(dex_id, method_id);
}

auto get_method_name(uintptr_t method) {
  auto cls = get_declaring_class(method);
  auto dexfile = get_class_dexfile(cls);
  uint32_t dex_method_index = Read4(AccessField(method, 64U));
  uintptr_t method_id =
      AccessArrayItem(Read4(AccessField(dexfile, 48U)), dex_method_index, 8U);
  uint32_t name_idx = Read4(AccessField(method_id, 4U));
  return get_dexfile_string_by_idx(dexfile, name_idx);
}

auto get_class_descriptor(uintptr_t cls) {
  auto dexfile = get_class_dexfile(cls);
  uint32_t typeidx = Read4(AccessField(cls, 76U));
  uintptr_t typeid_ =
      AccessArrayItem(Read4(AccessField(dexfile, 40U)), typeidx, 4U);
  uint32_t descriptor_idx = Read4(AccessField(typeid_, 0U));
  return get_dexfile_string_by_idx(dexfile, descriptor_idx);
}

auto unwind(unwind_callback_t __unwind_callback, void* __unwind_data) {
  return false;
}
