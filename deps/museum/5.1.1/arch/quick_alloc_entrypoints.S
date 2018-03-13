/*
 * Copyright (C) 2013 The Android Open Source Project
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

.macro GENERATE_ALLOC_ENTRYPOINTS c_suffix, cxx_suffix
// Called by managed code to allocate an object.
TWO_ARG_DOWNCALL art_quick_alloc_object\c_suffix, artAllocObjectFromCode\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an object of a resolved class.
TWO_ARG_DOWNCALL art_quick_alloc_object_resolved\c_suffix, artAllocObjectFromCodeResolved\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an object of an initialized class.
TWO_ARG_DOWNCALL art_quick_alloc_object_initialized\c_suffix, artAllocObjectFromCodeInitialized\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an object when the caller doesn't know whether it has access
// to the created type.
TWO_ARG_DOWNCALL art_quick_alloc_object_with_access_check\c_suffix, artAllocObjectFromCodeWithAccessCheck\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an array.
THREE_ARG_DOWNCALL art_quick_alloc_array\c_suffix, artAllocArrayFromCode\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an array of a resolve class.
THREE_ARG_DOWNCALL art_quick_alloc_array_resolved\c_suffix, artAllocArrayFromCodeResolved\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an array when the caller doesn't know whether it has access
// to the created type.
THREE_ARG_DOWNCALL art_quick_alloc_array_with_access_check\c_suffix, artAllocArrayFromCodeWithAccessCheck\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an array in a special case for FILLED_NEW_ARRAY.
THREE_ARG_DOWNCALL art_quick_check_and_alloc_array\c_suffix, artCheckAndAllocArrayFromCode\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
// Called by managed code to allocate an array in a special case for FILLED_NEW_ARRAY.
THREE_ARG_DOWNCALL art_quick_check_and_alloc_array_with_access_check\c_suffix, artCheckAndAllocArrayFromCodeWithAccessCheck\cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
.endm

.macro GENERATE_ALL_ALLOC_ENTRYPOINTS
GENERATE_ALLOC_ENTRYPOINTS _dlmalloc, DlMalloc
GENERATE_ALLOC_ENTRYPOINTS _dlmalloc_instrumented, DlMallocInstrumented
GENERATE_ALLOC_ENTRYPOINTS _rosalloc, RosAlloc
GENERATE_ALLOC_ENTRYPOINTS _rosalloc_instrumented, RosAllocInstrumented
GENERATE_ALLOC_ENTRYPOINTS _bump_pointer, BumpPointer
GENERATE_ALLOC_ENTRYPOINTS _bump_pointer_instrumented, BumpPointerInstrumented
GENERATE_ALLOC_ENTRYPOINTS _tlab, TLAB
GENERATE_ALLOC_ENTRYPOINTS _tlab_instrumented, TLABInstrumented
.endm
