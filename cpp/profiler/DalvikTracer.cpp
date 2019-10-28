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

#include "DalvikTracer.h"

#include <dlfcn.h>
#include <unistd.h>

#include "profilo/LogEntry.h"
#include "profilo/Logger.h"

#include "DalvikUtils.h"

#define STACK_SAVE_AREA_OFFSET 5
#define SAVEAREA_FROM_FP(fp) \
  ((StackSaveArea*)(((u4*)(fp)) - STACK_SAVE_AREA_OFFSET))

namespace facebook {
namespace profilo {
namespace profiler {

namespace {

void* getDvmThreadSelf() {
  struct lib {
    lib() : handle(dlopen("libdvm.so", RTLD_LOCAL)) {
      if (handle == nullptr) {
        throw std::runtime_error(dlerror());
      }
    }
    ~lib() {
      if (handle != nullptr) {
        dlclose(handle);
      }
    }

    void* handle;
  } libdvm;

  void* dvmThreadSelf = dlsym(libdvm.handle, "dvmThreadSelf");
  if (dvmThreadSelf == nullptr) {
    // Try C++-mangled name
    dvmThreadSelf = dlsym(libdvm.handle, "_Z13dvmThreadSelfv");
  }

  if (dvmThreadSelf == nullptr) {
    throw std::runtime_error(dlerror());
  }
  return dvmThreadSelf;
}

} // namespace

DalvikTracer::DalvikTracer()
    : dvmThreadSelf_(
          reinterpret_cast<decltype(dvmThreadSelf_)>(getDvmThreadSelf())) {}

StackCollectionRetcode DalvikTracer::collectJavaStack(
    ucontext_t*,
    int64_t* frames,
    char const** method_names,
    char const** class_descriptors,
    uint8_t& depth,
    uint8_t max_depth) {
  Thread* thread = dvmThreadSelf_();
  if (thread == nullptr) {
    return StackCollectionRetcode::NO_STACK_FOR_THREAD;
  }

  u4* fp = thread->interpSave.curFrame;
  depth = 0;

  while (fp != nullptr) {
    if (depth == max_depth) {
      return StackCollectionRetcode::STACK_OVERFLOW;
    }

    StackSaveArea* saveArea = SAVEAREA_FROM_FP(fp);
    const Method* method = saveArea->method;

    fp = saveArea->prevFrame;

    if (method == nullptr) {
      continue;
    }

    if (method_names != nullptr && class_descriptors != nullptr) {
      method_names[depth] = method->name;
      class_descriptors[depth] = method->clazz->descriptor;
    }

    frames[depth] = dalvikGetMethodIdForSymbolication(method);
    depth++;
  }

  if (depth == 0) {
    return StackCollectionRetcode::EMPTY_STACK;
  }
  return StackCollectionRetcode::SUCCESS;
}

StackCollectionRetcode DalvikTracer::collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint8_t& depth,
    uint8_t max_depth) {
      return collectJavaStack(ucontext, frames, nullptr, nullptr, depth, max_depth);
}

void DalvikTracer::flushStack(
    int64_t* frames,
    uint8_t depth,
    int tid,
    int64_t time_) {
  Logger::get().writeStackFrames(
      tid, static_cast<int64_t>(time_), frames, depth);
}

void DalvikTracer::prepare() {}

void DalvikTracer::startTracing() {}

void DalvikTracer::stopTracing() {}

} // namespace profiler
} // namespace profilo
} // namespace facebook
