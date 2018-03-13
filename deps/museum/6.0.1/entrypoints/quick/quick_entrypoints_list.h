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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_

// All quick entrypoints. Format is name, return type, argument types.

#define QUICK_ENTRYPOINT_LIST(V) \
  V(AllocArray, void*, uint32_t, int32_t, ArtMethod*) \
  V(AllocArrayResolved, void*, mirror::Class*, int32_t, ArtMethod*) \
  V(AllocArrayWithAccessCheck, void*, uint32_t, int32_t, ArtMethod*) \
  V(AllocObject, void*, uint32_t, ArtMethod*) \
  V(AllocObjectResolved, void*, mirror::Class*, ArtMethod*) \
  V(AllocObjectInitialized, void*, mirror::Class*, ArtMethod*) \
  V(AllocObjectWithAccessCheck, void*, uint32_t, ArtMethod*) \
  V(CheckAndAllocArray, void*, uint32_t, int32_t, ArtMethod*) \
  V(CheckAndAllocArrayWithAccessCheck, void*, uint32_t, int32_t, ArtMethod*) \
  V(AllocStringFromBytes, void*, void*, int32_t, int32_t, int32_t) \
  V(AllocStringFromChars, void*, int32_t, int32_t, void*) \
  V(AllocStringFromString, void*, void*) \
\
  V(InstanceofNonTrivial, uint32_t, const mirror::Class*, const mirror::Class*) \
  V(CheckCast, void, const mirror::Class*, const mirror::Class*) \
\
  V(InitializeStaticStorage, void*, uint32_t) \
  V(InitializeTypeAndVerifyAccess, void*, uint32_t) \
  V(InitializeType, void*, uint32_t) \
  V(ResolveString, void*, uint32_t) \
\
  V(Set8Instance, int, uint32_t, void*, int8_t) \
  V(Set8Static, int, uint32_t, int8_t) \
  V(Set16Instance, int, uint32_t, void*, int16_t) \
  V(Set16Static, int, uint32_t, int16_t) \
  V(Set32Instance, int, uint32_t, void*, int32_t) \
  V(Set32Static, int, uint32_t, int32_t) \
  V(Set64Instance, int, uint32_t, void*, int64_t) \
  V(Set64Static, int, uint32_t, int64_t) \
  V(SetObjInstance, int, uint32_t, void*, void*) \
  V(SetObjStatic, int, uint32_t, void*) \
  V(GetByteInstance, int8_t, uint32_t, void*) \
  V(GetBooleanInstance, uint8_t, uint32_t, void*) \
  V(GetByteStatic, int8_t, uint32_t) \
  V(GetBooleanStatic, uint8_t, uint32_t) \
  V(GetShortInstance, int16_t, uint32_t, void*) \
  V(GetCharInstance, uint16_t, uint32_t, void*) \
  V(GetShortStatic, int16_t, uint32_t) \
  V(GetCharStatic, uint16_t, uint32_t) \
  V(Get32Instance, int32_t, uint32_t, void*) \
  V(Get32Static, int32_t, uint32_t) \
  V(Get64Instance, int64_t, uint32_t, void*) \
  V(Get64Static, int64_t, uint32_t) \
  V(GetObjInstance, void*, uint32_t, void*) \
  V(GetObjStatic, void*, uint32_t) \
\
  V(AputObjectWithNullAndBoundCheck, void, mirror::Array*, int32_t, mirror::Object*) \
  V(AputObjectWithBoundCheck, void, mirror::Array*, int32_t, mirror::Object*) \
  V(AputObject, void, mirror::Array*, int32_t, mirror::Object*) \
  V(HandleFillArrayData, void, void*, void*) \
\
  V(JniMethodStart, uint32_t, Thread*) \
  V(JniMethodStartSynchronized, uint32_t, jobject, Thread*) \
  V(JniMethodEnd, void, uint32_t, Thread*) \
  V(JniMethodEndSynchronized, void, uint32_t, jobject, Thread*) \
  V(JniMethodEndWithReference, mirror::Object*, jobject, uint32_t, Thread*) \
  V(JniMethodEndWithReferenceSynchronized, mirror::Object*, jobject, uint32_t, jobject, Thread*) \
  V(QuickGenericJniTrampoline, void, ArtMethod*) \
\
  V(LockObject, void, mirror::Object*) \
  V(UnlockObject, void, mirror::Object*) \
\
  V(CmpgDouble, int32_t, double, double) \
  V(CmpgFloat, int32_t, float, float) \
  V(CmplDouble, int32_t, double, double) \
  V(CmplFloat, int32_t, float, float) \
  V(Fmod, double, double, double) \
  V(L2d, double, int64_t) \
  V(Fmodf, float, float, float) \
  V(L2f, float, int64_t) \
  V(D2iz, int32_t, double) \
  V(F2iz, int32_t, float) \
  V(Idivmod, int32_t, int32_t, int32_t) \
  V(D2l, int64_t, double) \
  V(F2l, int64_t, float) \
  V(Ldiv, int64_t, int64_t, int64_t) \
  V(Lmod, int64_t, int64_t, int64_t) \
  V(Lmul, int64_t, int64_t, int64_t) \
  V(ShlLong, uint64_t, uint64_t, uint32_t) \
  V(ShrLong, uint64_t, uint64_t, uint32_t) \
  V(UshrLong, uint64_t, uint64_t, uint32_t) \
\
  V(IndexOf, int32_t, void*, uint32_t, uint32_t, uint32_t) \
  V(StringCompareTo, int32_t, void*, void*) \
  V(Memcpy, void*, void*, const void*, size_t) \
\
  V(QuickImtConflictTrampoline, void, ArtMethod*) \
  V(QuickResolutionTrampoline, void, ArtMethod*) \
  V(QuickToInterpreterBridge, void, ArtMethod*) \
  V(InvokeDirectTrampolineWithAccessCheck, void, uint32_t, void*) \
  V(InvokeInterfaceTrampolineWithAccessCheck, void, uint32_t, void*) \
  V(InvokeStaticTrampolineWithAccessCheck, void, uint32_t, void*) \
  V(InvokeSuperTrampolineWithAccessCheck, void, uint32_t, void*) \
  V(InvokeVirtualTrampolineWithAccessCheck, void, uint32_t, void*) \
\
  V(TestSuspend, void, void) \
\
  V(DeliverException, void, mirror::Object*) \
  V(ThrowArrayBounds, void, int32_t, int32_t) \
  V(ThrowDivZero, void, void) \
  V(ThrowNoSuchMethod, void, int32_t) \
  V(ThrowNullPointer, void, void) \
  V(ThrowStackOverflow, void, void*) \
  V(Deoptimize, void, void) \
\
  V(A64Load, int64_t, volatile const int64_t *) \
  V(A64Store, void, volatile int64_t *, int64_t) \
\
  V(NewEmptyString, void) \
  V(NewStringFromBytes_B, void) \
  V(NewStringFromBytes_BI, void) \
  V(NewStringFromBytes_BII, void) \
  V(NewStringFromBytes_BIII, void) \
  V(NewStringFromBytes_BIIString, void) \
  V(NewStringFromBytes_BString, void) \
  V(NewStringFromBytes_BIICharset, void) \
  V(NewStringFromBytes_BCharset, void) \
  V(NewStringFromChars_C, void) \
  V(NewStringFromChars_CII, void) \
  V(NewStringFromChars_IIC, void) \
  V(NewStringFromCodePoints, void) \
  V(NewStringFromString, void) \
  V(NewStringFromStringBuffer, void) \
  V(NewStringFromStringBuilder, void) \
\
  V(ReadBarrierJni, void, mirror::CompressedReference<mirror::Object>*, Thread*)

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_
#undef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_   // #define is only for lint.
