/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_LIST_H_
#define ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_LIST_H_

// Methods that intercept available libcore implementations.
#define UNSTARTED_RUNTIME_DIRECT_LIST(V)    \
  V(ClassForName, "java.lang.Class java.lang.Class.forName(java.lang.String)") \
  V(ClassForNameLong, "java.lang.Class java.lang.Class.forName(java.lang.String, boolean, java.lang.ClassLoader)") \
  V(ClassClassForName, "java.lang.Class java.lang.Class.classForName(java.lang.String, boolean, java.lang.ClassLoader)") \
  V(ClassNewInstance, "java.lang.Object java.lang.Class.newInstance()") \
  V(ClassGetDeclaredField, "java.lang.reflect.Field java.lang.Class.getDeclaredField(java.lang.String)") \
  V(VmClassLoaderFindLoadedClass, "java.lang.Class java.lang.VMClassLoader.findLoadedClass(java.lang.ClassLoader, java.lang.String)") \
  V(VoidLookupType, "java.lang.Class java.lang.Void.lookupType()") \
  V(SystemArraycopy, "void java.lang.System.arraycopy(java.lang.Object, int, java.lang.Object, int, int)") \
  V(SystemArraycopyChar, "void java.lang.System.arraycopy(char[], int, char[], int, int)") \
  V(SystemArraycopyInt, "void java.lang.System.arraycopy(int[], int, int[], int, int)") \
  V(ThreadLocalGet, "java.lang.Object java.lang.ThreadLocal.get()") \
  V(MathCeil, "double java.lang.Math.ceil(double)") \
  V(ObjectHashCode, "int java.lang.Object.hashCode()") \
  V(DoubleDoubleToRawLongBits, "long java.lang.Double.doubleToRawLongBits(double)") \
  V(DexCacheGetDexNative, "com.android.dex.Dex java.lang.DexCache.getDexNative()") \
  V(MemoryPeekByte, "byte libcore.io.Memory.peekByte(long)") \
  V(MemoryPeekShort, "short libcore.io.Memory.peekShortNative(long)") \
  V(MemoryPeekInt, "int libcore.io.Memory.peekIntNative(long)") \
  V(MemoryPeekLong, "long libcore.io.Memory.peekLongNative(long)") \
  V(MemoryPeekByteArray, "void libcore.io.Memory.peekByteArray(long, byte[], int, int)") \
  V(SecurityGetSecurityPropertiesReader, "java.io.Reader java.security.Security.getSecurityPropertiesReader()") \
  V(StringGetCharsNoCheck, "void java.lang.String.getCharsNoCheck(int, int, char[], int)") \
  V(StringCharAt, "char java.lang.String.charAt(int)") \
  V(StringSetCharAt, "void java.lang.String.setCharAt(int, char)") \
  V(StringFactoryNewStringFromChars, "java.lang.String java.lang.StringFactory.newStringFromChars(int, int, char[])") \
  V(StringFactoryNewStringFromString, "java.lang.String java.lang.StringFactory.newStringFromString(java.lang.String)") \
  V(StringFastSubstring, "java.lang.String java.lang.String.fastSubstring(int, int)") \
  V(StringToCharArray, "char[] java.lang.String.toCharArray()")

// Methods that are native.
#define UNSTARTED_RUNTIME_JNI_LIST(V)           \
  V(VMRuntimeNewUnpaddedArray, "java.lang.Object dalvik.system.VMRuntime.newUnpaddedArray(java.lang.Class, int)") \
  V(VMStackGetCallingClassLoader, "java.lang.ClassLoader dalvik.system.VMStack.getCallingClassLoader()") \
  V(VMStackGetStackClass2, "java.lang.Class dalvik.system.VMStack.getStackClass2()") \
  V(MathLog, "double java.lang.Math.log(double)") \
  V(MathExp, "double java.lang.Math.exp(double)") \
  V(ClassGetNameNative, "java.lang.String java.lang.Class.getNameNative()") \
  V(FloatFloatToRawIntBits, "int java.lang.Float.floatToRawIntBits(float)") \
  V(FloatIntBitsToFloat, "float java.lang.Float.intBitsToFloat(int)") \
  V(ObjectInternalClone, "java.lang.Object java.lang.Object.internalClone()") \
  V(ObjectNotifyAll, "void java.lang.Object.notifyAll()") \
  V(StringCompareTo, "int java.lang.String.compareTo(java.lang.String)") \
  V(StringIntern, "java.lang.String java.lang.String.intern()") \
  V(StringFastIndexOf, "int java.lang.String.fastIndexOf(int, int)") \
  V(ArrayCreateMultiArray, "java.lang.Object java.lang.reflect.Array.createMultiArray(java.lang.Class, int[])") \
  V(ArrayCreateObjectArray, "java.lang.Object java.lang.reflect.Array.createObjectArray(java.lang.Class, int)") \
  V(ThrowableNativeFillInStackTrace, "java.lang.Object java.lang.Throwable.nativeFillInStackTrace()") \
  V(SystemIdentityHashCode, "int java.lang.System.identityHashCode(java.lang.Object)") \
  V(ByteOrderIsLittleEndian, "boolean java.nio.ByteOrder.isLittleEndian()") \
  V(UnsafeCompareAndSwapInt, "boolean sun.misc.Unsafe.compareAndSwapInt(java.lang.Object, long, int, int)") \
  V(UnsafePutObject, "void sun.misc.Unsafe.putObject(java.lang.Object, long, java.lang.Object)") \
  V(UnsafeGetArrayBaseOffsetForComponentType, "int sun.misc.Unsafe.getArrayBaseOffsetForComponentType(java.lang.Class)") \
  V(UnsafeGetArrayIndexScaleForComponentType, "int sun.misc.Unsafe.getArrayIndexScaleForComponentType(java.lang.Class)")

#endif  // ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_LIST_H_
// the guard in this file is just for cpplint
#undef ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_LIST_H_
