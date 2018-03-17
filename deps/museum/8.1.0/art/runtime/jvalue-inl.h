/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_JVALUE_INL_H_
#define ART_RUNTIME_JVALUE_INL_H_

#include "jvalue.h"

#include "obj_ptr.h"

namespace art {

inline void JValue::SetL(ObjPtr<mirror::Object> new_l) {
  l = new_l.Ptr();
}

#define DEFINE_FROM(type, chr) \
    template <> inline JValue JValue::FromPrimitive(type v) { \
      JValue res; \
      res.Set ## chr(v); \
      return res; \
    }

DEFINE_FROM(uint8_t, Z);
DEFINE_FROM(int8_t, B);
DEFINE_FROM(uint16_t, C);
DEFINE_FROM(int16_t, S);
DEFINE_FROM(int32_t, I);
DEFINE_FROM(int64_t, J);
DEFINE_FROM(float, F);
DEFINE_FROM(double, D);

#undef DEFINE_FROM

}  // namespace art

#endif  // ART_RUNTIME_JVALUE_INL_H_
