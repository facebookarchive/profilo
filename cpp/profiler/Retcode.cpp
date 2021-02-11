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

#include "Retcode.h"

#include <profilo/ExternalApi.h>
#include <profilo/LogEntry.h>

namespace facebook {
namespace profilo {
namespace profiler {

namespace {
EntryType errorToTraceEntry(StackCollectionRetcode error) {
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wswitch"

  switch (error) {
    case StackCollectionRetcode::EMPTY_STACK:
      return EntryType::STKERR_EMPTYSTACK;

    case StackCollectionRetcode::STACK_OVERFLOW:
      return EntryType::STKERR_STACKOVERFLOW;

    case StackCollectionRetcode::NO_STACK_FOR_THREAD:
      return EntryType::STKERR_NOSTACKFORTHREAD;

    case StackCollectionRetcode::SIGNAL_INTERRUPT:
      return EntryType::STKERR_SIGNALINTERRUPT;

    case StackCollectionRetcode::NESTED_UNWIND:
      return EntryType::STKERR_NESTEDUNWIND;

    case StackCollectionRetcode::STACK_COPY_FAILED:
      return EntryType::STKERR_STACKCOPYFAILED;

    case StackCollectionRetcode::UNWINDER_QUEUE_OVERFLOW:
      return EntryType::STKERR_QUEUEOVERFLOW;

    case StackCollectionRetcode::PARTIAL_STACK:
      return EntryType::STKERR_PARTIALSTACK;

    case StackCollectionRetcode::TRACER_DISABLED:
    case StackCollectionRetcode::SUCCESS:
    case StackCollectionRetcode::IGNORE:
    case StackCollectionRetcode::MAXVAL:
      return EntryType::UNKNOWN_TYPE;
  }

#pragma clang diagnostic pop
}
} // namespace

void StackCollectionEntryConverter::logRetcode(
    MultiBufferLogger& logger,
    uint32_t retcode,
    int32_t tid,
    int64_t time,
    uint32_t profiler_type) {
  logger.write(StandardEntry{
      .type = errorToTraceEntry(static_cast<StackCollectionRetcode>(retcode)),
      .timestamp = time,
      .tid = tid,
      .extra = profiler_type});
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
