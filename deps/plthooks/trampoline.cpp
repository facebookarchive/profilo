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

#include <plthooks/trampoline.h>
#include <plthooks/hooks.h>
#include <linker/locks.h>

#include <sys/mman.h>
#include <algorithm>
#include <cstring>
#include <list>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <system_error>
#include <vector>

#include <abort_with_reason.h>

#ifdef ANDROID
#include <sys/prctl.h>
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0
#endif // PR_SET_VMA
#endif // ANDROID

namespace facebook {
namespace plthooks {

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
            0)) {
    if (map_ == MAP_FAILED) {
      throw std::system_error(errno, std::system_category());
    }

#ifdef ANDROID
    // Older Linux kernels may not have the PR_SET_VMA call implementation.
    // That's okay we just ignore errors if this call fails.
    prctl(
        PR_SET_VMA,
        PR_SET_VMA_ANON_NAME,
        map_,
        kSize,
        "plthooks plt trampolines");
#endif // ANDROID
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
  static_assert(
      (kAlignment & (kAlignment - 1)) == 0, "kAlignment must be power of 2");
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

struct trampoline_hook_info {
  HookId id;
  void* const return_address;
  trampoline_hook_info* previous;

  std::vector<void*> run_list;
};

static pthread_key_t get_hook_info_key() {
  static pthread_key_t tls_key_ = ({
    pthread_key_t key;
    if (pthread_key_create(&key, +[](void* obj) {
          auto info = reinterpret_cast<trampoline_hook_info*>(obj);
          delete info;
        }) != 0) {
      log_assert("failed to create trampoline TLS key");
    }
    key;
  });
  return tls_key_;
}

static trampoline_hook_info* get_hook_info() {
  auto key = get_hook_info_key();
  return reinterpret_cast<trampoline_hook_info*>(pthread_getspecific(key));
}

#ifdef LINKER_TRAMPOLINE_SUPPORTED_ARCH
void* push_hook_stack(HookId hook, void* return_address) {
  auto run_list = hooks::get_run_list(hook);
  if (run_list.size() == 0) {
    // No run list
    abortWithReason("Run list for trampoline is empty");
  }
  auto* info = new trampoline_hook_info{.id = hook,
                                        .return_address = return_address,
                                        .previous = get_hook_info()};
  info->run_list = std::move(run_list);

  pthread_setspecific(get_hook_info_key(), info);

  // return the last entry
  return info->run_list.back();
}

void* pop_hook_stack() {
  auto info = get_hook_info();
  void* ret = info->return_address;
  pthread_setspecific(get_hook_info_key(), info->previous);
  delete info;
  return ret;
}
#endif

} // namespace

namespace trampoline {

class trampoline {
 public:
  explicit trampoline(HookId id)
      : code_size_(
            reinterpret_cast<uintptr_t>(trampoline_data_pointer()) -
            reinterpret_cast<uintptr_t>(trampoline_template_pointer())),
        code_(allocate(code_size_ + trampoline_data_size())) {
#ifdef LINKER_TRAMPOLINE_SUPPORTED_ARCH
    std::memcpy(code_, trampoline_template_pointer(), code_size_);

    void** data = reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(code_) + code_size_);

    *data++ = reinterpret_cast<void*>(push_hook_stack);
    *data++ = reinterpret_cast<void*>(pop_hook_stack);
    *data++ = reinterpret_cast<void*>(id);

     __builtin___clear_cache((char*) code_, (char*) code_ + code_size_ + trampoline_data_size());
#endif
  }

  trampoline(trampoline const&) = delete;
  trampoline(trampoline&&) = delete;

  trampoline& operator=(trampoline const&) = delete;
  trampoline& operator=(trampoline&&) = delete;

  void* code() const {
    return reinterpret_cast<void*>(code_);
  }

 private:
  size_t const code_size_; // does NOT include data
  void* const code_;
};

} // namespace trampoline

void* create_trampoline(HookId id) {
#ifdef LINKER_TRAMPOLINE_SUPPORTED_ARCH
  static std::list<trampoline::trampoline> trampolines_;
  static pthread_rwlock_t lock_ = PTHREAD_RWLOCK_INITIALIZER;

  WriterLock wl(&lock_);
  trampolines_.emplace_back(id);
  return trampolines_.back().code();
#else
  throw std::runtime_error("unsupported architecture");
#endif
}

} // namespace plthooks
} // namespace facebook

extern "C" {

void* get_previous_from_hook(void* hook) {
  auto info = facebook::plthooks::get_hook_info();
  if (info == nullptr) {
    // Not in a hook!
    abortWithReason("CALL_PREV call outside of an active hook");
  }
  auto iter = std::find(info->run_list.begin(), info->run_list.end(), hook);
  if (iter == info->run_list.begin()) {
    abortWithReason("CALL_PREV call by original function?!");
  }
  if (iter == info->run_list.end()) {
    abortWithReason("CALL_PREV call by an unknown hook? How did we get here?");
  }
  // Decrement means go towards the original function.
  iter--;
  return *iter;
}

} // extern "C"
