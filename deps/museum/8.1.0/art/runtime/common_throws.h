/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_COMMON_THROWS_H_
#define ART_RUNTIME_COMMON_THROWS_H_

#include "base/mutex.h"
#include "invoke_type.h"
#include "obj_ptr.h"

namespace art {
namespace mirror {
  class Class;
  class Object;
  class MethodType;
}  // namespace mirror
class ArtField;
class ArtMethod;
class DexFile;
class Signature;
class StringPiece;

// AbstractMethodError

void ThrowAbstractMethodError(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowAbstractMethodError(uint32_t method_idx, const DexFile& dex_file)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// ArithmeticException

void ThrowArithmeticExceptionDivideByZero() REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// ArrayIndexOutOfBoundsException

void ThrowArrayIndexOutOfBoundsException(int index, int length)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// ArrayStoreException

void ThrowArrayStoreException(ObjPtr<mirror::Class> element_class,
                              ObjPtr<mirror::Class> array_class)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// BootstrapMethodError

void ThrowBootstrapMethodError(const char* fmt, ...)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowWrappedBootstrapMethodError(const char* fmt, ...)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// ClassCircularityError

void ThrowClassCircularityError(ObjPtr<mirror::Class> c)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowClassCircularityError(ObjPtr<mirror::Class> c, const char* fmt, ...)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// ClassCastException

void ThrowClassCastException(ObjPtr<mirror::Class> dest_type, ObjPtr<mirror::Class> src_type)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowClassCastException(const char* msg)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// ClassFormatError

void ThrowClassFormatError(ObjPtr<mirror::Class> referrer, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// IllegalAccessError

void ThrowIllegalAccessErrorClass(ObjPtr<mirror::Class> referrer, ObjPtr<mirror::Class> accessed)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIllegalAccessErrorClassForMethodDispatch(ObjPtr<mirror::Class> referrer,
                                                   ObjPtr<mirror::Class> accessed,
                                                   ArtMethod* called,
                                                   InvokeType type)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIllegalAccessErrorMethod(ObjPtr<mirror::Class> referrer, ArtMethod* accessed)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIllegalAccessErrorField(ObjPtr<mirror::Class> referrer, ArtField* accessed)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIllegalAccessErrorFinalField(ArtMethod* referrer, ArtField* accessed)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIllegalAccessError(ObjPtr<mirror::Class> referrer, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// IllegalAccessException

void ThrowIllegalAccessException(const char* msg)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// IllegalArgumentException

void ThrowIllegalArgumentException(const char* msg)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// IncompatibleClassChangeError

void ThrowIncompatibleClassChangeError(InvokeType expected_type,
                                       InvokeType found_type,
                                       ArtMethod* method,
                                       ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIncompatibleClassChangeErrorClassForInterfaceSuper(ArtMethod* method,
                                                             ObjPtr<mirror::Class> target_class,
                                                             ObjPtr<mirror::Object> this_object,
                                                             ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIncompatibleClassChangeErrorClassForInterfaceDispatch(ArtMethod* interface_method,
                                                                ObjPtr<mirror::Object> this_object,
                                                                ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIncompatibleClassChangeErrorField(ArtField* resolved_field,
                                            bool is_static,
                                            ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIncompatibleClassChangeError(ObjPtr<mirror::Class> referrer, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowIncompatibleClassChangeErrorForMethodConflict(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// InternalError

void ThrowInternalError(const char* fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// IOException

void ThrowIOException(const char* fmt, ...) __attribute__((__format__(__printf__, 1, 2)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowWrappedIOException(const char* fmt, ...) __attribute__((__format__(__printf__, 1, 2)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// LinkageError

void ThrowLinkageError(ObjPtr<mirror::Class> referrer, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowWrappedLinkageError(ObjPtr<mirror::Class> referrer, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// NegativeArraySizeException

void ThrowNegativeArraySizeException(int size)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowNegativeArraySizeException(const char* msg)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;


// NoSuchFieldError

void ThrowNoSuchFieldError(const StringPiece& scope,
                           ObjPtr<mirror::Class> c,
                           const StringPiece& type,
                           const StringPiece& name)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowNoSuchFieldException(ObjPtr<mirror::Class> c, const StringPiece& name)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// NoSuchMethodError

void ThrowNoSuchMethodError(InvokeType type,
                            ObjPtr<mirror::Class> c,
                            const StringPiece& name,
                            const Signature& signature)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// NullPointerException

void ThrowNullPointerExceptionForFieldAccess(ArtField* field,
                                             bool is_read)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowNullPointerExceptionForMethodAccess(uint32_t method_idx,
                                              InvokeType type)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowNullPointerExceptionForMethodAccess(ArtMethod* method,
                                              InvokeType type)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowNullPointerExceptionFromDexPC(bool check_address = false, uintptr_t addr = 0)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void ThrowNullPointerException(const char* msg)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// RuntimeException

void ThrowRuntimeException(const char* fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// SecurityException

void ThrowSecurityException(const char* fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// Stack overflow.

void ThrowStackOverflowError(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// StringIndexOutOfBoundsException

void ThrowStringIndexOutOfBoundsException(int index, int length)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// VerifyError

void ThrowVerifyError(ObjPtr<mirror::Class> referrer, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

// WrongMethodTypeException
void ThrowWrongMethodTypeException(mirror::MethodType* callee_type,
                                   mirror::MethodType* callsite_type)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

}  // namespace art

#endif  // ART_RUNTIME_COMMON_THROWS_H_
