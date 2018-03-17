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

#ifndef ART_RUNTIME_REFLECTION_INL_H_
#define ART_RUNTIME_REFLECTION_INL_H_

#include "reflection.h"

#include "base/stringprintf.h"
#include "common_throws.h"
#include "jvalue.h"
#include "primitive.h"
#include "utils.h"

namespace art {

inline bool ConvertPrimitiveValue(const ThrowLocation* throw_location, bool unbox_for_result,
                                  Primitive::Type srcType, Primitive::Type dstType,
                                  const JValue& src, JValue* dst) {
  DCHECK(srcType != Primitive::kPrimNot && dstType != Primitive::kPrimNot);
  if (LIKELY(srcType == dstType)) {
    dst->SetJ(src.GetJ());
    return true;
  }
  switch (dstType) {
  case Primitive::kPrimBoolean:  // Fall-through.
  case Primitive::kPrimChar:  // Fall-through.
  case Primitive::kPrimByte:
    // Only expect assignment with source and destination of identical type.
    break;
  case Primitive::kPrimShort:
    if (srcType == Primitive::kPrimByte) {
      dst->SetS(src.GetI());
      return true;
    }
    break;
  case Primitive::kPrimInt:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort) {
      dst->SetI(src.GetI());
      return true;
    }
    break;
  case Primitive::kPrimLong:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst->SetJ(src.GetI());
      return true;
    }
    break;
  case Primitive::kPrimFloat:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst->SetF(src.GetI());
      return true;
    } else if (srcType == Primitive::kPrimLong) {
      dst->SetF(src.GetJ());
      return true;
    }
    break;
  case Primitive::kPrimDouble:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst->SetD(src.GetI());
      return true;
    } else if (srcType == Primitive::kPrimLong) {
      dst->SetD(src.GetJ());
      return true;
    } else if (srcType == Primitive::kPrimFloat) {
      dst->SetD(src.GetF());
      return true;
    }
    break;
  default:
    break;
  }
  if (!unbox_for_result) {
    ThrowIllegalArgumentException(throw_location,
                                  StringPrintf("Invalid primitive conversion from %s to %s",
                                               PrettyDescriptor(srcType).c_str(),
                                               PrettyDescriptor(dstType).c_str()).c_str());
  } else {
    ThrowClassCastException(throw_location,
                            StringPrintf("Couldn't convert result of type %s to %s",
                                         PrettyDescriptor(srcType).c_str(),
                                         PrettyDescriptor(dstType).c_str()).c_str());
  }
  return false;
}

}  // namespace art

#endif  // ART_RUNTIME_REFLECTION_INL_H_
