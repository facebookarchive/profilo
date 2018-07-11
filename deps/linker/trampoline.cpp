/**
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

#include <linker/trampoline.h>
#include <linker/locks.h>

#include <cstring>
#include <list>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <sys/mman.h>
#include <system_error>
#include <vector>

#if defined(__arm__)
extern "C" {
extern void (*trampoline_template_thumb)();
extern void (*trampoline_template_arm)();
extern void* trampoline_data_thumb;
extern void* trampoline_data_arm;
}
#endif

namespace facebook { namespace linker {

namespace {

class allocator_block {
public:
  allocator_block()
    : map_(mmap(
        nullptr,
        kSize,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, /* invalid fd */
        0)
      ) {
    if (map_ == MAP_FAILED) {
      throw std::system_error(errno, std::system_category());
    }
    top_ = reinterpret_cast<uint8_t*>(map_);
  }

  allocator_block(allocator_block const&) = delete;
  allocator_block(allocator_block&&) = delete;

  allocator_block& operator=(allocator_block const&) = delete;
  allocator_block& operator=(allocator_block&&) = delete;

  size_t remaining() const {
    return kSize - (top_ - reinterpret_cast<uint8_t* const>(map_));
  }

  void* allocate(size_t sz) {
    if (remaining() < sz) {
      throw std::bad_alloc();
    }
    void* old = top_;
    top_ += sz;
    return old;
  }

private:
  static constexpr size_t kPageSize = 4096;
  static constexpr size_t kPagesPerBlock = 1;
  static constexpr size_t kSize = kPageSize * kPagesPerBlock;

  void* const map_;
  uint8_t* top_;
};

static void* allocate(size_t sz) {
  static constexpr size_t kAlignment = 4;
  static_assert((kAlignment & (kAlignment - 1)) == 0, "kAlignment must be power of 2");
  static std::list<allocator_block> blocks_;
  static pthread_rwlock_t lock_ = PTHREAD_RWLOCK_INITIALIZER;

  // round sz up to nearest kAlignment-bytes boundary
  sz = (sz + (kAlignment - 1)) & ~(kAlignment - 1);

  WriterLock wl(&lock_);
  if (blocks_.empty() || blocks_.back().remaining() < sz) {
    blocks_.emplace_back();
  }
  return blocks_.back().allocate(sz);
}

struct trampoline_stack_entry {
  void* const chained;
  void* const lr;
  void* const ip;
};

static pthread_key_t get_hook_stack_key() {
  static pthread_key_t tls_key_ = ({
    pthread_key_t key;
    if (pthread_key_create(&key, +[](void* obj) {
          auto vec = reinterpret_cast<std::vector<trampoline_stack_entry>*>(obj);
          delete vec;
        }) != 0) {
      log_assert("failed to create trampoline TLS key");
    }
    key;
  });
  return tls_key_;
}

static std::vector<trampoline_stack_entry>& get_hook_stack() {
  auto key = get_hook_stack_key();
  auto vec = reinterpret_cast<std::vector<trampoline_stack_entry>*>(
    pthread_getspecific(key));
  if (vec == nullptr) {
    vec = new std::vector<trampoline_stack_entry>();
    pthread_setspecific(key, vec);
  }
  return *vec;
}

#if defined (__arm__)
static void delete_hook_stack() {
  auto key = get_hook_stack_key();
  auto vec = reinterpret_cast<std::vector<trampoline_stack_entry>*>(
      pthread_getspecific(key));
  if (vec == nullptr) {
    return;
  }
  delete vec;
  pthread_setspecific(key, nullptr);
}

void push_hook_stack(void* chained, void* lr, void* ip) {
  trampoline_stack_entry entry = { chained, lr, ip };
  get_hook_stack().push_back(entry);
}

uint64_t pop_hook_stack() {
  auto& stack = get_hook_stack();
  auto back = stack.back();
  stack.pop_back();
  if (stack.empty()) {
    delete_hook_stack();
  }
  // this bitshift-and-return is a bit wonky, but it's taking advantage of the ARM
  // procedure call standard for returning a 64-bit fundamental type as simply two
  // paired registers. if we were to return a struct of some sort, it would get
  // stored and fetched from memory: more complicated, and more slower.
  return reinterpret_cast<uint64_t>(back.ip) << 32 | reinterpret_cast<uint64_t>(back.lr);
}
#endif

class trampoline {
public:
  trampoline(void* hook, void* chained)
      : code_size_(reinterpret_cast<uintptr_t>(trampoline_data(chained)) -
                   reinterpret_cast<uintptr_t>(trampoline_template(chained))),
        code_(allocate(code_size_ + trampoline_data_size())) {
#if defined (__arm__)
    std::memcpy(code_, trampoline_template(chained), code_size_);

    auto* data = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(code_) + code_size_);

    *data++ = reinterpret_cast<uint32_t>(push_hook_stack);
    *data++ = reinterpret_cast<uint32_t>(pop_hook_stack);
    *data++ = reinterpret_cast<uint32_t>(hook);
    *data++ = reinterpret_cast<uint32_t>(chained);
#endif
  }

  trampoline(trampoline const&) = delete;
  trampoline(trampoline&&) = delete;

  trampoline& operator=(trampoline const&) = delete;
  trampoline& operator=(trampoline&&) = delete;

  trampoline(void* existing_trampoline)
    : code_size_(0), code_(existing_trampoline) { }

  void* chained() const {
    auto* data = reinterpret_cast<uint32_t*>(
      reinterpret_cast<uintptr_t>(code_) + code_size_);
    return reinterpret_cast<void*>(data[3]);
  }

  void* code() const {
    // when constructing this trampoline we chose either an ARM or a Thumb template
    // to build from, so when creating our function pointer, set the Thumb bit on
    // it accordingly.
    // whoever is calling us will alllllmost certainly be using an exchange-capable
    // instruction (bx, blx) - so matching trampoline ISA to chained-func ISA might
    // be overkill - but on the off chance that they didn't, we want to be compatible.
    return reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(code_) |
      (reinterpret_cast<uintptr_t>(chained()) & 1));
  }

private:
  static void* trampoline_template(void* chained) {
#if defined(__arm__)
    uintptr_t ret;
    if (reinterpret_cast<uintptr_t>(chained) & 0x1) {
      ret = reinterpret_cast<uintptr_t>(&trampoline_template_thumb);
    } else {
      ret = reinterpret_cast<uintptr_t>(&trampoline_template_arm);
    }
    asm("" : "=rm"(ret)); // force compiler to abandon its assumption that ret is aligned
    ret &= ~1;
    return reinterpret_cast<void*>(ret);
#else
    return 0;
#endif
  }

  static void* trampoline_data(void* chained) {
#if defined(__arm__)
    if (reinterpret_cast<uintptr_t>(chained) & 0x1) {
      return &trampoline_data_thumb;
    } else {
      return &trampoline_data_arm;
    }
#else
    return 0;
#endif
  }

  static size_t trampoline_data_size() {
    return sizeof(uint32_t) * 4;
  }

  size_t const code_size_; // does NOT include data
  void* const code_;
};

} // namespace (anonymous)

void* create_trampoline(void* hook, void* chained) {
#if defined(__arm__)
  static std::list<trampoline> trampolines_;
  static pthread_rwlock_t lock_ = PTHREAD_RWLOCK_INITIALIZER;

  WriterLock wl(&lock_);
  trampolines_.emplace_back(hook, chained);
  return trampolines_.back().code();
#else
  throw std::runtime_error("unsupported architecture");
#endif
}

} } // namespace facebook::linker

extern "C" {

void* get_chained_plt_method() {
  return facebook::linker::get_hook_stack().back().chained;
}

} // extern "C"
