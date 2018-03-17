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

#ifndef ART_RUNTIME_ASM_SUPPORT_H_
#define ART_RUNTIME_ASM_SUPPORT_H_

#if defined(__cplusplus)
#include "art_method.h"
#include "lock_word.h"
#include "mirror/class.h"
#include "mirror/string.h"
#include "runtime.h"
#include "thread.h"
#endif

#include "read_barrier_c.h"

#if defined(__arm__) || defined(__mips__)
// In quick code for ARM and MIPS we make poor use of registers and perform frequent suspend
// checks in the event of loop back edges. The SUSPEND_CHECK_INTERVAL constant is loaded into a
// register at the point of an up-call or after handling a suspend check. It reduces the number of
// loads of the TLS suspend check value by the given amount (turning it into a decrement and compare
// of a register). This increases the time for a thread to respond to requests from GC and the
// debugger, damaging GC performance and creating other unwanted artifacts. For example, this count
// has the effect of making loops and Java code look cold in profilers, where the count is reset
// impacts where samples will occur. Reducing the count as much as possible improves profiler
// accuracy in tools like traceview.
// TODO: get a compiler that can do a proper job of loop optimization and remove this.
#define SUSPEND_CHECK_INTERVAL 96
#endif

#if defined(__cplusplus)

#ifndef ADD_TEST_EQ  // Allow #include-r to replace with their own.
#define ADD_TEST_EQ(x, y) CHECK_EQ(x, y);
#endif

static inline void CheckAsmSupportOffsetsAndSizes() {
#else
#define ADD_TEST_EQ(x, y)
#endif

// Size of references to the heap on the stack.
#define STACK_REFERENCE_SIZE 4
ADD_TEST_EQ(static_cast<size_t>(STACK_REFERENCE_SIZE), sizeof(art::StackReference<art::mirror::Object>))

// Size of heap references
#define COMPRESSED_REFERENCE_SIZE 4
ADD_TEST_EQ(static_cast<size_t>(COMPRESSED_REFERENCE_SIZE),
            sizeof(art::mirror::CompressedReference<art::mirror::Object>))

// Note: these callee save methods loads require read barriers.
// Offset of field Runtime::callee_save_methods_[kSaveAll]
#define RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET 0
ADD_TEST_EQ(static_cast<size_t>(RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET),
            art::Runtime::GetCalleeSaveMethodOffset(art::Runtime::kSaveAll))

// Offset of field Runtime::callee_save_methods_[kRefsOnly]
#define RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET 8
ADD_TEST_EQ(static_cast<size_t>(RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET),
            art::Runtime::GetCalleeSaveMethodOffset(art::Runtime::kRefsOnly))

// Offset of field Runtime::callee_save_methods_[kRefsAndArgs]
#define RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET (2 * 8)
ADD_TEST_EQ(static_cast<size_t>(RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET),
            art::Runtime::GetCalleeSaveMethodOffset(art::Runtime::kRefsAndArgs))

// Offset of field Thread::tls32_.state_and_flags.
#define THREAD_FLAGS_OFFSET 0
ADD_TEST_EQ(THREAD_FLAGS_OFFSET,
            art::Thread::ThreadFlagsOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tls32_.thin_lock_thread_id.
#define THREAD_ID_OFFSET 12
ADD_TEST_EQ(THREAD_ID_OFFSET,
            art::Thread::ThinLockIdOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.card_table.
#define THREAD_CARD_TABLE_OFFSET 128
ADD_TEST_EQ(THREAD_CARD_TABLE_OFFSET,
            art::Thread::CardTableOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.exception.
#define THREAD_EXCEPTION_OFFSET (THREAD_CARD_TABLE_OFFSET + __SIZEOF_POINTER__)
ADD_TEST_EQ(THREAD_EXCEPTION_OFFSET,
            art::Thread::ExceptionOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.managed_stack.top_quick_frame_.
#define THREAD_TOP_QUICK_FRAME_OFFSET (THREAD_CARD_TABLE_OFFSET + (3 * __SIZEOF_POINTER__))
ADD_TEST_EQ(THREAD_TOP_QUICK_FRAME_OFFSET,
            art::Thread::TopOfManagedStackOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.managed_stack.top_quick_frame_.
#define THREAD_SELF_OFFSET (THREAD_CARD_TABLE_OFFSET + (9 * __SIZEOF_POINTER__))
ADD_TEST_EQ(THREAD_SELF_OFFSET,
            art::Thread::SelfOffset<__SIZEOF_POINTER__>().Int32Value())

#define THREAD_LOCAL_POS_OFFSET (THREAD_CARD_TABLE_OFFSET + 147 * __SIZEOF_POINTER__)
ADD_TEST_EQ(THREAD_LOCAL_POS_OFFSET,
            art::Thread::ThreadLocalPosOffset<__SIZEOF_POINTER__>().Int32Value())
#define THREAD_LOCAL_END_OFFSET (THREAD_LOCAL_POS_OFFSET + __SIZEOF_POINTER__)
ADD_TEST_EQ(THREAD_LOCAL_END_OFFSET,
            art::Thread::ThreadLocalEndOffset<__SIZEOF_POINTER__>().Int32Value())
#define THREAD_LOCAL_OBJECTS_OFFSET (THREAD_LOCAL_POS_OFFSET + 2 * __SIZEOF_POINTER__)
ADD_TEST_EQ(THREAD_LOCAL_OBJECTS_OFFSET,
            art::Thread::ThreadLocalObjectsOffset<__SIZEOF_POINTER__>().Int32Value())

// Offsets within java.lang.Object.
#define MIRROR_OBJECT_CLASS_OFFSET 0
ADD_TEST_EQ(MIRROR_OBJECT_CLASS_OFFSET, art::mirror::Object::ClassOffset().Int32Value())
#define MIRROR_OBJECT_LOCK_WORD_OFFSET 4
ADD_TEST_EQ(MIRROR_OBJECT_LOCK_WORD_OFFSET, art::mirror::Object::MonitorOffset().Int32Value())

#if defined(USE_BROOKS_READ_BARRIER)
#define MIRROR_OBJECT_HEADER_SIZE 16
#else
#define MIRROR_OBJECT_HEADER_SIZE 8
#endif
ADD_TEST_EQ(size_t(MIRROR_OBJECT_HEADER_SIZE), sizeof(art::mirror::Object))

// Offsets within java.lang.Class.
#define MIRROR_CLASS_COMPONENT_TYPE_OFFSET (4 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_CLASS_COMPONENT_TYPE_OFFSET,
            art::mirror::Class::ComponentTypeOffset().Int32Value())
#define MIRROR_CLASS_ACCESS_FLAGS_OFFSET (36 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_CLASS_ACCESS_FLAGS_OFFSET,
            art::mirror::Class::AccessFlagsOffset().Int32Value())
#define MIRROR_CLASS_OBJECT_SIZE_OFFSET (112 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_CLASS_OBJECT_SIZE_OFFSET,
            art::mirror::Class::ObjectSizeOffset().Int32Value())
#define MIRROR_CLASS_STATUS_OFFSET (124 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_CLASS_STATUS_OFFSET,
            art::mirror::Class::StatusOffset().Int32Value())

#define MIRROR_CLASS_STATUS_INITIALIZED 10
ADD_TEST_EQ(static_cast<uint32_t>(MIRROR_CLASS_STATUS_INITIALIZED),
            static_cast<uint32_t>(art::mirror::Class::kStatusInitialized))
#define ACCESS_FLAGS_CLASS_IS_FINALIZABLE 0x80000000
ADD_TEST_EQ(static_cast<uint32_t>(ACCESS_FLAGS_CLASS_IS_FINALIZABLE),
            static_cast<uint32_t>(kAccClassIsFinalizable))

// Array offsets.
#define MIRROR_ARRAY_LENGTH_OFFSET      MIRROR_OBJECT_HEADER_SIZE
ADD_TEST_EQ(MIRROR_ARRAY_LENGTH_OFFSET, art::mirror::Array::LengthOffset().Int32Value())

#define MIRROR_CHAR_ARRAY_DATA_OFFSET   (4 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_CHAR_ARRAY_DATA_OFFSET,
            art::mirror::Array::DataOffset(sizeof(uint16_t)).Int32Value())

#define MIRROR_OBJECT_ARRAY_DATA_OFFSET (4 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_OBJECT_ARRAY_DATA_OFFSET,
    art::mirror::Array::DataOffset(
        sizeof(art::mirror::HeapReference<art::mirror::Object>)).Int32Value())

#define MIRROR_OBJECT_ARRAY_COMPONENT_SIZE 4
ADD_TEST_EQ(static_cast<size_t>(MIRROR_OBJECT_ARRAY_COMPONENT_SIZE),
            sizeof(art::mirror::HeapReference<art::mirror::Object>))

#define MIRROR_LONG_ARRAY_DATA_OFFSET (8 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_LONG_ARRAY_DATA_OFFSET,
            art::mirror::Array::DataOffset(sizeof(uint64_t)).Int32Value())

// Offsets within java.lang.String.
#define MIRROR_STRING_COUNT_OFFSET  MIRROR_OBJECT_HEADER_SIZE
ADD_TEST_EQ(MIRROR_STRING_COUNT_OFFSET, art::mirror::String::CountOffset().Int32Value())

#define MIRROR_STRING_VALUE_OFFSET (8 + MIRROR_OBJECT_HEADER_SIZE)
ADD_TEST_EQ(MIRROR_STRING_VALUE_OFFSET, art::mirror::String::ValueOffset().Int32Value())

// Offsets within java.lang.reflect.ArtMethod.
#define ART_METHOD_DEX_CACHE_METHODS_OFFSET 4
ADD_TEST_EQ(ART_METHOD_DEX_CACHE_METHODS_OFFSET,
            art::ArtMethod::DexCacheResolvedMethodsOffset().Int32Value())

#define ART_METHOD_DEX_CACHE_TYPES_OFFSET 8
ADD_TEST_EQ(ART_METHOD_DEX_CACHE_TYPES_OFFSET,
            art::ArtMethod::DexCacheResolvedTypesOffset().Int32Value())

#define ART_METHOD_QUICK_CODE_OFFSET_32 36
ADD_TEST_EQ(ART_METHOD_QUICK_CODE_OFFSET_32,
            art::ArtMethod::EntryPointFromQuickCompiledCodeOffset(4).Int32Value())

#define ART_METHOD_QUICK_CODE_OFFSET_64 48
ADD_TEST_EQ(ART_METHOD_QUICK_CODE_OFFSET_64,
            art::ArtMethod::EntryPointFromQuickCompiledCodeOffset(8).Int32Value())

#define LOCK_WORD_STATE_SHIFT 30
ADD_TEST_EQ(LOCK_WORD_STATE_SHIFT, static_cast<int32_t>(art::LockWord::kStateShift))

#define LOCK_WORD_STATE_MASK 0xC0000000
ADD_TEST_EQ(LOCK_WORD_STATE_MASK, static_cast<uint32_t>(art::LockWord::kStateMaskShifted))

#define LOCK_WORD_READ_BARRIER_STATE_SHIFT 28
ADD_TEST_EQ(LOCK_WORD_READ_BARRIER_STATE_SHIFT,
            static_cast<int32_t>(art::LockWord::kReadBarrierStateShift))

#define LOCK_WORD_READ_BARRIER_STATE_MASK 0x30000000
ADD_TEST_EQ(LOCK_WORD_READ_BARRIER_STATE_MASK,
            static_cast<int32_t>(art::LockWord::kReadBarrierStateMaskShifted))

#define LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED 0xCFFFFFFF
ADD_TEST_EQ(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED,
            static_cast<uint32_t>(art::LockWord::kReadBarrierStateMaskShiftedToggled))

#define LOCK_WORD_THIN_LOCK_COUNT_ONE 65536
ADD_TEST_EQ(LOCK_WORD_THIN_LOCK_COUNT_ONE, static_cast<int32_t>(art::LockWord::kThinLockCountOne))

#define OBJECT_ALIGNMENT_MASK 7
ADD_TEST_EQ(static_cast<size_t>(OBJECT_ALIGNMENT_MASK), art::kObjectAlignment - 1)

#define OBJECT_ALIGNMENT_MASK_TOGGLED 0xFFFFFFF8
ADD_TEST_EQ(static_cast<uint32_t>(OBJECT_ALIGNMENT_MASK_TOGGLED),
            ~static_cast<uint32_t>(art::kObjectAlignment - 1))

#if defined(__cplusplus)
}  // End of CheckAsmSupportOffsets.
#endif

#endif  // ART_RUNTIME_ASM_SUPPORT_H_
