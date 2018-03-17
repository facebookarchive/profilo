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

#ifndef ART_RUNTIME_JVALUE_H_
#define ART_RUNTIME_JVALUE_H_

#include "base/macros.h"
#include "base/mutex.h"

#include <stdint.h>

#include "obj_ptr.h"

namespace art {
namespace mirror {
class Object;
}  // namespace mirror

union PACKED(alignof(mirror::Object*)) JValue {
  // We default initialize JValue instances to all-zeros.
  JValue() : j(0) {}

  template<typename T> static JValue FromPrimitive(T v);

  int8_t GetB() const { return b; }
  void SetB(int8_t new_b) {
    j = ((static_cast<int64_t>(new_b) << 56) >> 56);  // Sign-extend to 64 bits.
  }

  uint16_t GetC() const { return c; }
  void SetC(uint16_t new_c) {
    j = static_cast<int64_t>(new_c);  // Zero-extend to 64 bits.
  }

  double GetD() const { return d; }
  void SetD(double new_d) { d = new_d; }

  float GetF() const { return f; }
  void SetF(float new_f) { f = new_f; }

  int32_t GetI() const { return i; }
  void SetI(int32_t new_i) {
    j = ((static_cast<int64_t>(new_i) << 32) >> 32);  // Sign-extend to 64 bits.
  }

  int64_t GetJ() const { return j; }
  void SetJ(int64_t new_j) { j = new_j; }

  mirror::Object* GetL() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return l;
  }
  void SetL(ObjPtr<mirror::Object> new_l) REQUIRES_SHARED(Locks::mutator_lock_);

  int16_t GetS() const { return s; }
  void SetS(int16_t new_s) {
    j = ((static_cast<int64_t>(new_s) << 48) >> 48);  // Sign-extend to 64 bits.
  }

  uint8_t GetZ() const { return z; }
  void SetZ(uint8_t new_z) {
    j = static_cast<int64_t>(new_z);  // Zero-extend to 64 bits.
  }

  mirror::Object** GetGCRoot() { return &l; }

 private:
  uint8_t z;
  int8_t b;
  uint16_t c;
  int16_t s;
  int32_t i;
  int64_t j;
  float f;
  double d;
  mirror::Object* l;
};

}  // namespace art

#endif  // ART_RUNTIME_JVALUE_H_
