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

#ifndef ART_RUNTIME_GC_ACCOUNTING_READ_BARRIER_TABLE_H_
#define ART_RUNTIME_GC_ACCOUNTING_READ_BARRIER_TABLE_H_

#include "base/bit_utils.h"
#include "base/mutex.h"
#include "gc/space/space.h"
#include "globals.h"
#include "mem_map.h"

namespace art {
namespace gc {
namespace accounting {

// Used to decide whether to take the read barrier fast/slow paths for
// kUseTableLookupReadBarrier. If an entry is set, take the read
// barrier slow path. There's an entry per region.
class ReadBarrierTable {
 public:
  ReadBarrierTable() {
    size_t capacity = static_cast<size_t>(kHeapCapacity / kRegionSize);
    DCHECK_EQ(kHeapCapacity / kRegionSize,
              static_cast<uint64_t>(static_cast<size_t>(kHeapCapacity / kRegionSize)));
    std::string error_msg;
    MemMap* mem_map = MemMap::MapAnonymous("read barrier table", nullptr, capacity,
                                           PROT_READ | PROT_WRITE, false, false, &error_msg);
    CHECK(mem_map != nullptr && mem_map->Begin() != nullptr)
        << "couldn't allocate read barrier table: " << error_msg;
    mem_map_.reset(mem_map);
  }
  void ClearForSpace(space::ContinuousSpace* space) {
    uint8_t* entry_start = EntryFromAddr(space->Begin());
    uint8_t* entry_end = EntryFromAddr(space->Limit());
    memset(reinterpret_cast<void*>(entry_start), 0, entry_end - entry_start);
  }
  void Clear(uint8_t* start_addr, uint8_t* end_addr) {
    DCHECK(IsValidHeapAddr(start_addr)) << start_addr;
    DCHECK(IsValidHeapAddr(end_addr)) << end_addr;
    DCHECK_ALIGNED(start_addr, kRegionSize);
    DCHECK_ALIGNED(end_addr, kRegionSize);
    uint8_t* entry_start = EntryFromAddr(start_addr);
    uint8_t* entry_end = EntryFromAddr(end_addr);
    memset(reinterpret_cast<void*>(entry_start), 0, entry_end - entry_start);
  }
  bool IsSet(const void* heap_addr) const {
    DCHECK(IsValidHeapAddr(heap_addr)) << heap_addr;
    uint8_t entry_value = *EntryFromAddr(heap_addr);
    DCHECK(entry_value == 0 || entry_value == kSetEntryValue);
    return entry_value == kSetEntryValue;
  }
  void ClearAll() {
    mem_map_->MadviseDontNeedAndZero();
  }
  void SetAll() {
    memset(mem_map_->Begin(), kSetEntryValue, mem_map_->Size());
  }
  bool IsAllCleared() const {
    for (uint32_t* p = reinterpret_cast<uint32_t*>(mem_map_->Begin());
         p < reinterpret_cast<uint32_t*>(mem_map_->End()); ++p) {
      if (*p != 0) {
        return false;
      }
    }
    return true;
  }

  // This should match RegionSpace::kRegionSize. static_assert'ed in concurrent_copying.h.
  static constexpr size_t kRegionSize = 1 * MB;

 private:
  static constexpr uint64_t kHeapCapacity = 4ULL * GB;  // low 4gb.
  static constexpr uint8_t kSetEntryValue = 0x01;

  uint8_t* EntryFromAddr(const void* heap_addr) const {
    DCHECK(IsValidHeapAddr(heap_addr)) << heap_addr;
    uint8_t* entry_addr = mem_map_->Begin() + reinterpret_cast<uintptr_t>(heap_addr) / kRegionSize;
    DCHECK(IsValidEntry(entry_addr)) << "heap_addr: " << heap_addr
                                     << " entry_addr: " << reinterpret_cast<void*>(entry_addr);
    return entry_addr;
  }

  bool IsValidHeapAddr(const void* heap_addr) const {
#ifdef __LP64__
    return reinterpret_cast<uint64_t>(heap_addr) < kHeapCapacity;
#else
    UNUSED(heap_addr);
    return true;
#endif
  }

  bool IsValidEntry(const uint8_t* entry_addr) const {
    uint8_t* begin = mem_map_->Begin();
    uint8_t* end = mem_map_->End();
    return entry_addr >= begin && entry_addr < end;
  }

  std::unique_ptr<MemMap> mem_map_;
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_READ_BARRIER_TABLE_H_
