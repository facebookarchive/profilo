/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_SPACE_SPACE_TEST_H_
#define ART_RUNTIME_GC_SPACE_SPACE_TEST_H_

#include <stdint.h>
#include <memory>

#include "common_runtime_test.h"
#include "globals.h"
#include "mirror/array-inl.h"
#include "mirror/object-inl.h"
#include "scoped_thread_state_change.h"
#include "zygote_space.h"

namespace art {
namespace gc {
namespace space {

class SpaceTest : public CommonRuntimeTest {
 public:
  jobject byte_array_class_;

  SpaceTest() : byte_array_class_(nullptr) {
  }

  void AddSpace(ContinuousSpace* space, bool revoke = true) {
    Heap* heap = Runtime::Current()->GetHeap();
    if (revoke) {
      heap->RevokeAllThreadLocalBuffers();
    }
    heap->AddSpace(space);
    heap->SetSpaceAsDefault(space);
  }

  mirror::Class* GetByteArrayClass(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    StackHandleScope<1> hs(self);
    auto null_loader(hs.NewHandle<mirror::ClassLoader>(nullptr));
    if (byte_array_class_ == nullptr) {
      mirror::Class* byte_array_class =
          Runtime::Current()->GetClassLinker()->FindClass(self, "[B", null_loader);
      EXPECT_TRUE(byte_array_class != nullptr);
      byte_array_class_ = self->GetJniEnv()->NewLocalRef(byte_array_class);
      EXPECT_TRUE(byte_array_class_ != nullptr);
    }
    return reinterpret_cast<mirror::Class*>(self->DecodeJObject(byte_array_class_));
  }

  mirror::Object* Alloc(space::MallocSpace* alloc_space, Thread* self, size_t bytes,
                        size_t* bytes_allocated, size_t* usable_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> byte_array_class(hs.NewHandle(GetByteArrayClass(self)));
    mirror::Object* obj = alloc_space->Alloc(self, bytes, bytes_allocated, usable_size);
    if (obj != nullptr) {
      InstallClass(obj, byte_array_class.Get(), bytes);
    }
    return obj;
  }

  mirror::Object* AllocWithGrowth(space::MallocSpace* alloc_space, Thread* self, size_t bytes,
                                  size_t* bytes_allocated, size_t* usable_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> byte_array_class(hs.NewHandle(GetByteArrayClass(self)));
    mirror::Object* obj = alloc_space->AllocWithGrowth(self, bytes, bytes_allocated, usable_size);
    if (obj != nullptr) {
      InstallClass(obj, byte_array_class.Get(), bytes);
    }
    return obj;
  }

  void InstallClass(mirror::Object* o, mirror::Class* byte_array_class, size_t size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Note the minimum size, which is the size of a zero-length byte array.
    EXPECT_GE(size, SizeOfZeroLengthByteArray());
    EXPECT_TRUE(byte_array_class != nullptr);
    o->SetClass(byte_array_class);
    if (kUseBakerOrBrooksReadBarrier) {
      // Like the proper heap object allocation, install and verify
      // the correct read barrier pointer.
      if (kUseBrooksReadBarrier) {
        o->SetReadBarrierPointer(o);
      }
      o->AssertReadBarrierPointer();
    }
    mirror::Array* arr = o->AsArray<kVerifyNone>();
    size_t header_size = SizeOfZeroLengthByteArray();
    int32_t length = size - header_size;
    arr->SetLength(length);
    EXPECT_EQ(arr->SizeOf<kVerifyNone>(), size);
  }

  static size_t SizeOfZeroLengthByteArray() {
    return mirror::Array::DataOffset(Primitive::ComponentSize(Primitive::kPrimByte)).Uint32Value();
  }

  typedef MallocSpace* (*CreateSpaceFn)(const std::string& name, size_t initial_size, size_t growth_limit,
                                        size_t capacity, byte* requested_begin);
  void InitTestBody(CreateSpaceFn create_space);
  void ZygoteSpaceTestBody(CreateSpaceFn create_space);
  void AllocAndFreeTestBody(CreateSpaceFn create_space);
  void AllocAndFreeListTestBody(CreateSpaceFn create_space);

  void SizeFootPrintGrowthLimitAndTrimBody(MallocSpace* space, intptr_t object_size,
                                           int round, size_t growth_limit);
  void SizeFootPrintGrowthLimitAndTrimDriver(size_t object_size, CreateSpaceFn create_space);
};

static inline size_t test_rand(size_t* seed) {
  *seed = *seed * 1103515245 + 12345;
  return *seed;
}

void SpaceTest::InitTestBody(CreateSpaceFn create_space) {
  {
    // Init < max == growth
    std::unique_ptr<Space> space(create_space("test", 16 * MB, 32 * MB, 32 * MB, nullptr));
    EXPECT_TRUE(space.get() != nullptr);
  }
  {
    // Init == max == growth
    std::unique_ptr<Space> space(create_space("test", 16 * MB, 16 * MB, 16 * MB, nullptr));
    EXPECT_TRUE(space.get() != nullptr);
  }
  {
    // Init > max == growth
    std::unique_ptr<Space> space(create_space("test", 32 * MB, 16 * MB, 16 * MB, nullptr));
    EXPECT_TRUE(space.get() == nullptr);
  }
  {
    // Growth == init < max
    std::unique_ptr<Space> space(create_space("test", 16 * MB, 16 * MB, 32 * MB, nullptr));
    EXPECT_TRUE(space.get() != nullptr);
  }
  {
    // Growth < init < max
    std::unique_ptr<Space> space(create_space("test", 16 * MB, 8 * MB, 32 * MB, nullptr));
    EXPECT_TRUE(space.get() == nullptr);
  }
  {
    // Init < growth < max
    std::unique_ptr<Space> space(create_space("test", 8 * MB, 16 * MB, 32 * MB, nullptr));
    EXPECT_TRUE(space.get() != nullptr);
  }
  {
    // Init < max < growth
    std::unique_ptr<Space> space(create_space("test", 8 * MB, 32 * MB, 16 * MB, nullptr));
    EXPECT_TRUE(space.get() == nullptr);
  }
}

// TODO: This test is not very good, we should improve it.
// The test should do more allocations before the creation of the ZygoteSpace, and then do
// allocations after the ZygoteSpace is created. The test should also do some GCs to ensure that
// the GC works with the ZygoteSpace.
void SpaceTest::ZygoteSpaceTestBody(CreateSpaceFn create_space) {
  size_t dummy;
  MallocSpace* space(create_space("test", 4 * MB, 16 * MB, 16 * MB, nullptr));
  ASSERT_TRUE(space != nullptr);

  // Make space findable to the heap, will also delete space when runtime is cleaned up
  AddSpace(space);
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  // Succeeds, fits without adjusting the footprint limit.
  size_t ptr1_bytes_allocated, ptr1_usable_size;
  StackHandleScope<3> hs(soa.Self());
  Handle<mirror::Object> ptr1(
      hs.NewHandle(Alloc(space, self, 1 * MB, &ptr1_bytes_allocated, &ptr1_usable_size)));
  EXPECT_TRUE(ptr1.Get() != nullptr);
  EXPECT_LE(1U * MB, ptr1_bytes_allocated);
  EXPECT_LE(1U * MB, ptr1_usable_size);
  EXPECT_LE(ptr1_usable_size, ptr1_bytes_allocated);

  // Fails, requires a higher footprint limit.
  mirror::Object* ptr2 = Alloc(space, self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr2 == nullptr);

  // Succeeds, adjusts the footprint.
  size_t ptr3_bytes_allocated, ptr3_usable_size;
  Handle<mirror::Object> ptr3(
      hs.NewHandle(AllocWithGrowth(space, self, 8 * MB, &ptr3_bytes_allocated, &ptr3_usable_size)));
  EXPECT_TRUE(ptr3.Get() != nullptr);
  EXPECT_LE(8U * MB, ptr3_bytes_allocated);
  EXPECT_LE(8U * MB, ptr3_usable_size);
  EXPECT_LE(ptr3_usable_size, ptr3_bytes_allocated);

  // Fails, requires a higher footprint limit.
  mirror::Object* ptr4 = space->Alloc(self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr4 == nullptr);

  // Also fails, requires a higher allowed footprint.
  mirror::Object* ptr5 = space->AllocWithGrowth(self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr5 == nullptr);

  // Release some memory.
  size_t free3 = space->AllocationSize(ptr3.Get(), nullptr);
  EXPECT_EQ(free3, ptr3_bytes_allocated);
  EXPECT_EQ(free3, space->Free(self, ptr3.Assign(nullptr)));
  EXPECT_LE(8U * MB, free3);

  // Succeeds, now that memory has been freed.
  size_t ptr6_bytes_allocated, ptr6_usable_size;
  Handle<mirror::Object> ptr6(
      hs.NewHandle(AllocWithGrowth(space, self, 9 * MB, &ptr6_bytes_allocated, &ptr6_usable_size)));
  EXPECT_TRUE(ptr6.Get() != nullptr);
  EXPECT_LE(9U * MB, ptr6_bytes_allocated);
  EXPECT_LE(9U * MB, ptr6_usable_size);
  EXPECT_LE(ptr6_usable_size, ptr6_bytes_allocated);

  // Final clean up.
  size_t free1 = space->AllocationSize(ptr1.Get(), nullptr);
  space->Free(self, ptr1.Assign(nullptr));
  EXPECT_LE(1U * MB, free1);

  // Make sure that the zygote space isn't directly at the start of the space.
  EXPECT_TRUE(space->Alloc(self, 1U * MB, &dummy, nullptr) != nullptr);

  gc::Heap* heap = Runtime::Current()->GetHeap();
  space::Space* old_space = space;
  heap->RemoveSpace(old_space);
  heap->RevokeAllThreadLocalBuffers();
  space::ZygoteSpace* zygote_space = space->CreateZygoteSpace("alloc space",
                                                              heap->IsLowMemoryMode(),
                                                              &space);
  delete old_space;
  // Add the zygote space.
  AddSpace(zygote_space, false);

  // Make space findable to the heap, will also delete space when runtime is cleaned up
  AddSpace(space, false);

  // Succeeds, fits without adjusting the footprint limit.
  ptr1.Assign(Alloc(space, self, 1 * MB, &ptr1_bytes_allocated, &ptr1_usable_size));
  EXPECT_TRUE(ptr1.Get() != nullptr);
  EXPECT_LE(1U * MB, ptr1_bytes_allocated);
  EXPECT_LE(1U * MB, ptr1_usable_size);
  EXPECT_LE(ptr1_usable_size, ptr1_bytes_allocated);

  // Fails, requires a higher footprint limit.
  ptr2 = Alloc(space, self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr2 == nullptr);

  // Succeeds, adjusts the footprint.
  ptr3.Assign(AllocWithGrowth(space, self, 2 * MB, &ptr3_bytes_allocated, &ptr3_usable_size));
  EXPECT_TRUE(ptr3.Get() != nullptr);
  EXPECT_LE(2U * MB, ptr3_bytes_allocated);
  EXPECT_LE(2U * MB, ptr3_usable_size);
  EXPECT_LE(ptr3_usable_size, ptr3_bytes_allocated);
  space->Free(self, ptr3.Assign(nullptr));

  // Final clean up.
  free1 = space->AllocationSize(ptr1.Get(), nullptr);
  space->Free(self, ptr1.Assign(nullptr));
  EXPECT_LE(1U * MB, free1);
}

void SpaceTest::AllocAndFreeTestBody(CreateSpaceFn create_space) {
  size_t dummy = 0;
  MallocSpace* space(create_space("test", 4 * MB, 16 * MB, 16 * MB, nullptr));
  ASSERT_TRUE(space != nullptr);
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  // Make space findable to the heap, will also delete space when runtime is cleaned up
  AddSpace(space);

  // Succeeds, fits without adjusting the footprint limit.
  size_t ptr1_bytes_allocated, ptr1_usable_size;
  StackHandleScope<3> hs(soa.Self());
  Handle<mirror::Object> ptr1(
      hs.NewHandle(Alloc(space, self, 1 * MB, &ptr1_bytes_allocated, &ptr1_usable_size)));
  EXPECT_TRUE(ptr1.Get() != nullptr);
  EXPECT_LE(1U * MB, ptr1_bytes_allocated);
  EXPECT_LE(1U * MB, ptr1_usable_size);
  EXPECT_LE(ptr1_usable_size, ptr1_bytes_allocated);

  // Fails, requires a higher footprint limit.
  mirror::Object* ptr2 = Alloc(space, self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr2 == nullptr);

  // Succeeds, adjusts the footprint.
  size_t ptr3_bytes_allocated, ptr3_usable_size;
  Handle<mirror::Object> ptr3(
      hs.NewHandle(AllocWithGrowth(space, self, 8 * MB, &ptr3_bytes_allocated, &ptr3_usable_size)));
  EXPECT_TRUE(ptr3.Get() != nullptr);
  EXPECT_LE(8U * MB, ptr3_bytes_allocated);
  EXPECT_LE(8U * MB, ptr3_usable_size);
  EXPECT_LE(ptr3_usable_size, ptr3_bytes_allocated);

  // Fails, requires a higher footprint limit.
  mirror::Object* ptr4 = Alloc(space, self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr4 == nullptr);

  // Also fails, requires a higher allowed footprint.
  mirror::Object* ptr5 = AllocWithGrowth(space, self, 8 * MB, &dummy, nullptr);
  EXPECT_TRUE(ptr5 == nullptr);

  // Release some memory.
  size_t free3 = space->AllocationSize(ptr3.Get(), nullptr);
  EXPECT_EQ(free3, ptr3_bytes_allocated);
  space->Free(self, ptr3.Assign(nullptr));
  EXPECT_LE(8U * MB, free3);

  // Succeeds, now that memory has been freed.
  size_t ptr6_bytes_allocated, ptr6_usable_size;
  Handle<mirror::Object> ptr6(
      hs.NewHandle(AllocWithGrowth(space, self, 9 * MB, &ptr6_bytes_allocated, &ptr6_usable_size)));
  EXPECT_TRUE(ptr6.Get() != nullptr);
  EXPECT_LE(9U * MB, ptr6_bytes_allocated);
  EXPECT_LE(9U * MB, ptr6_usable_size);
  EXPECT_LE(ptr6_usable_size, ptr6_bytes_allocated);

  // Final clean up.
  size_t free1 = space->AllocationSize(ptr1.Get(), nullptr);
  space->Free(self, ptr1.Assign(nullptr));
  EXPECT_LE(1U * MB, free1);
}

void SpaceTest::AllocAndFreeListTestBody(CreateSpaceFn create_space) {
  MallocSpace* space(create_space("test", 4 * MB, 16 * MB, 16 * MB, nullptr));
  ASSERT_TRUE(space != nullptr);

  // Make space findable to the heap, will also delete space when runtime is cleaned up
  AddSpace(space);
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  // Succeeds, fits without adjusting the max allowed footprint.
  mirror::Object* lots_of_objects[1024];
  for (size_t i = 0; i < arraysize(lots_of_objects); i++) {
    size_t allocation_size, usable_size;
    size_t size_of_zero_length_byte_array = SizeOfZeroLengthByteArray();
    lots_of_objects[i] = Alloc(space, self, size_of_zero_length_byte_array, &allocation_size,
                               &usable_size);
    EXPECT_TRUE(lots_of_objects[i] != nullptr);
    size_t computed_usable_size;
    EXPECT_EQ(allocation_size, space->AllocationSize(lots_of_objects[i], &computed_usable_size));
    EXPECT_EQ(usable_size, computed_usable_size);
  }

  // Release memory.
  space->FreeList(self, arraysize(lots_of_objects), lots_of_objects);

  // Succeeds, fits by adjusting the max allowed footprint.
  for (size_t i = 0; i < arraysize(lots_of_objects); i++) {
    size_t allocation_size, usable_size;
    lots_of_objects[i] = AllocWithGrowth(space, self, 1024, &allocation_size, &usable_size);
    EXPECT_TRUE(lots_of_objects[i] != nullptr);
    size_t computed_usable_size;
    EXPECT_EQ(allocation_size, space->AllocationSize(lots_of_objects[i], &computed_usable_size));
    EXPECT_EQ(usable_size, computed_usable_size);
  }

  // Release memory.
  space->FreeList(self, arraysize(lots_of_objects), lots_of_objects);
}

void SpaceTest::SizeFootPrintGrowthLimitAndTrimBody(MallocSpace* space, intptr_t object_size,
                                                    int round, size_t growth_limit) {
  if (((object_size > 0 && object_size >= static_cast<intptr_t>(growth_limit))) ||
      ((object_size < 0 && -object_size >= static_cast<intptr_t>(growth_limit)))) {
    // No allocation can succeed
    return;
  }

  // The space's footprint equals amount of resources requested from system
  size_t footprint = space->GetFootprint();

  // The space must at least have its book keeping allocated
  EXPECT_GT(footprint, 0u);

  // But it shouldn't exceed the initial size
  EXPECT_LE(footprint, growth_limit);

  // space's size shouldn't exceed the initial size
  EXPECT_LE(space->Size(), growth_limit);

  // this invariant should always hold or else the space has grown to be larger than what the
  // space believes its size is (which will break invariants)
  EXPECT_GE(space->Size(), footprint);

  // Fill the space with lots of small objects up to the growth limit
  size_t max_objects = (growth_limit / (object_size > 0 ? object_size : 8)) + 1;
  std::unique_ptr<mirror::Object*[]> lots_of_objects(new mirror::Object*[max_objects]);
  size_t last_object = 0;  // last object for which allocation succeeded
  size_t amount_allocated = 0;  // amount of space allocated
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  size_t rand_seed = 123456789;
  for (size_t i = 0; i < max_objects; i++) {
    size_t alloc_fails = 0;  // number of failed allocations
    size_t max_fails = 30;  // number of times we fail allocation before giving up
    for (; alloc_fails < max_fails; alloc_fails++) {
      size_t alloc_size;
      if (object_size > 0) {
        alloc_size = object_size;
      } else {
        alloc_size = test_rand(&rand_seed) % static_cast<size_t>(-object_size);
        // Note the minimum size, which is the size of a zero-length byte array.
        size_t size_of_zero_length_byte_array = SizeOfZeroLengthByteArray();
        if (alloc_size < size_of_zero_length_byte_array) {
          alloc_size = size_of_zero_length_byte_array;
        }
      }
      StackHandleScope<1> hs(soa.Self());
      auto object(hs.NewHandle<mirror::Object>(nullptr));
      size_t bytes_allocated = 0;
      if (round <= 1) {
        object.Assign(Alloc(space, self, alloc_size, &bytes_allocated, nullptr));
      } else {
        object.Assign(AllocWithGrowth(space, self, alloc_size, &bytes_allocated, nullptr));
      }
      footprint = space->GetFootprint();
      EXPECT_GE(space->Size(), footprint);  // invariant
      if (object.Get() != nullptr) {  // allocation succeeded
        lots_of_objects[i] = object.Get();
        size_t allocation_size = space->AllocationSize(object.Get(), nullptr);
        EXPECT_EQ(bytes_allocated, allocation_size);
        if (object_size > 0) {
          EXPECT_GE(allocation_size, static_cast<size_t>(object_size));
        } else {
          EXPECT_GE(allocation_size, 8u);
        }
        amount_allocated += allocation_size;
        break;
      }
    }
    if (alloc_fails == max_fails) {
      last_object = i;
      break;
    }
  }
  CHECK_NE(last_object, 0u);  // we should have filled the space
  EXPECT_GT(amount_allocated, 0u);

  // We shouldn't have gone past the growth_limit
  EXPECT_LE(amount_allocated, growth_limit);
  EXPECT_LE(footprint, growth_limit);
  EXPECT_LE(space->Size(), growth_limit);

  // footprint and size should agree with amount allocated
  EXPECT_GE(footprint, amount_allocated);
  EXPECT_GE(space->Size(), amount_allocated);

  // Release storage in a semi-adhoc manner
  size_t free_increment = 96;
  while (true) {
    {
      ScopedThreadStateChange tsc(self, kNative);
      // Give the space a haircut.
      space->Trim();
    }

    // Bounds sanity
    footprint = space->GetFootprint();
    EXPECT_LE(amount_allocated, growth_limit);
    EXPECT_GE(footprint, amount_allocated);
    EXPECT_LE(footprint, growth_limit);
    EXPECT_GE(space->Size(), amount_allocated);
    EXPECT_LE(space->Size(), growth_limit);

    if (free_increment == 0) {
      break;
    }

    // Free some objects
    for (size_t i = 0; i < last_object; i += free_increment) {
      mirror::Object* object = lots_of_objects.get()[i];
      if (object == nullptr) {
        continue;
      }
      size_t allocation_size = space->AllocationSize(object, nullptr);
      if (object_size > 0) {
        EXPECT_GE(allocation_size, static_cast<size_t>(object_size));
      } else {
        EXPECT_GE(allocation_size, 8u);
      }
      space->Free(self, object);
      lots_of_objects.get()[i] = nullptr;
      amount_allocated -= allocation_size;
      footprint = space->GetFootprint();
      EXPECT_GE(space->Size(), footprint);  // invariant
    }

    free_increment >>= 1;
  }

  // The space has become empty here before allocating a large object
  // below. For RosAlloc, revoke thread-local runs, which are kept
  // even when empty for a performance reason, so that they won't
  // cause the following large object allocation to fail due to
  // potential fragmentation. Note they are normally revoked at each
  // GC (but no GC here.)
  space->RevokeAllThreadLocalBuffers();

  // All memory was released, try a large allocation to check freed memory is being coalesced
  StackHandleScope<1> hs(soa.Self());
  auto large_object(hs.NewHandle<mirror::Object>(nullptr));
  size_t three_quarters_space = (growth_limit / 2) + (growth_limit / 4);
  size_t bytes_allocated = 0;
  if (round <= 1) {
    large_object.Assign(Alloc(space, self, three_quarters_space, &bytes_allocated, nullptr));
  } else {
    large_object.Assign(AllocWithGrowth(space, self, three_quarters_space, &bytes_allocated,
                                        nullptr));
  }
  EXPECT_TRUE(large_object.Get() != nullptr);

  // Sanity check footprint
  footprint = space->GetFootprint();
  EXPECT_LE(footprint, growth_limit);
  EXPECT_GE(space->Size(), footprint);
  EXPECT_LE(space->Size(), growth_limit);

  // Clean up
  space->Free(self, large_object.Assign(nullptr));

  // Sanity check footprint
  footprint = space->GetFootprint();
  EXPECT_LE(footprint, growth_limit);
  EXPECT_GE(space->Size(), footprint);
  EXPECT_LE(space->Size(), growth_limit);
}

void SpaceTest::SizeFootPrintGrowthLimitAndTrimDriver(size_t object_size, CreateSpaceFn create_space) {
  if (object_size < SizeOfZeroLengthByteArray()) {
    // Too small for the object layout/model.
    return;
  }
  size_t initial_size = 4 * MB;
  size_t growth_limit = 8 * MB;
  size_t capacity = 16 * MB;
  MallocSpace* space(create_space("test", initial_size, growth_limit, capacity, nullptr));
  ASSERT_TRUE(space != nullptr);

  // Basic sanity
  EXPECT_EQ(space->Capacity(), growth_limit);
  EXPECT_EQ(space->NonGrowthLimitCapacity(), capacity);

  // Make space findable to the heap, will also delete space when runtime is cleaned up
  AddSpace(space);

  // In this round we don't allocate with growth and therefore can't grow past the initial size.
  // This effectively makes the growth_limit the initial_size, so assert this.
  SizeFootPrintGrowthLimitAndTrimBody(space, object_size, 1, initial_size);
  SizeFootPrintGrowthLimitAndTrimBody(space, object_size, 2, growth_limit);
  // Remove growth limit
  space->ClearGrowthLimit();
  EXPECT_EQ(space->Capacity(), capacity);
  SizeFootPrintGrowthLimitAndTrimBody(space, object_size, 3, capacity);
}

#define TEST_SizeFootPrintGrowthLimitAndTrimStatic(name, spaceName, spaceFn, size) \
  TEST_F(spaceName##StaticTest, SizeFootPrintGrowthLimitAndTrim_AllocationsOf_##name) { \
    SizeFootPrintGrowthLimitAndTrimDriver(size, spaceFn); \
  }

#define TEST_SizeFootPrintGrowthLimitAndTrimRandom(name, spaceName, spaceFn, size) \
  TEST_F(spaceName##RandomTest, SizeFootPrintGrowthLimitAndTrim_RandomAllocationsWithMax_##name) { \
    SizeFootPrintGrowthLimitAndTrimDriver(-size, spaceFn); \
  }

#define TEST_SPACE_CREATE_FN_BASE(spaceName, spaceFn) \
  class spaceName##BaseTest : public SpaceTest { \
  }; \
  \
  TEST_F(spaceName##BaseTest, Init) { \
    InitTestBody(spaceFn); \
  } \
  TEST_F(spaceName##BaseTest, ZygoteSpace) { \
    ZygoteSpaceTestBody(spaceFn); \
  } \
  TEST_F(spaceName##BaseTest, AllocAndFree) { \
    AllocAndFreeTestBody(spaceFn); \
  } \
  TEST_F(spaceName##BaseTest, AllocAndFreeList) { \
    AllocAndFreeListTestBody(spaceFn); \
  }

#define TEST_SPACE_CREATE_FN_STATIC(spaceName, spaceFn) \
  class spaceName##StaticTest : public SpaceTest { \
  }; \
  \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(12B, spaceName, spaceFn, 12) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(16B, spaceName, spaceFn, 16) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(24B, spaceName, spaceFn, 24) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(32B, spaceName, spaceFn, 32) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(64B, spaceName, spaceFn, 64) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(128B, spaceName, spaceFn, 128) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(1KB, spaceName, spaceFn, 1 * KB) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(4KB, spaceName, spaceFn, 4 * KB) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(1MB, spaceName, spaceFn, 1 * MB) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(4MB, spaceName, spaceFn, 4 * MB) \
  TEST_SizeFootPrintGrowthLimitAndTrimStatic(8MB, spaceName, spaceFn, 8 * MB)

#define TEST_SPACE_CREATE_FN_RANDOM(spaceName, spaceFn) \
  class spaceName##RandomTest : public SpaceTest { \
  }; \
  \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(16B, spaceName, spaceFn, 16) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(24B, spaceName, spaceFn, 24) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(32B, spaceName, spaceFn, 32) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(64B, spaceName, spaceFn, 64) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(128B, spaceName, spaceFn, 128) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(1KB, spaceName, spaceFn, 1 * KB) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(4KB, spaceName, spaceFn, 4 * KB) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(1MB, spaceName, spaceFn, 1 * MB) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(4MB, spaceName, spaceFn, 4 * MB) \
  TEST_SizeFootPrintGrowthLimitAndTrimRandom(8MB, spaceName, spaceFn, 8 * MB)

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_SPACE_TEST_H_
