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

#ifndef ART_RUNTIME_UTF_INL_H_
#define ART_RUNTIME_UTF_INL_H_

#include "utf.h"

namespace art {

inline uint16_t GetUtf16FromUtf8(const char** utf8_data_in) {
  uint8_t one = *(*utf8_data_in)++;
  if ((one & 0x80) == 0) {
    // one-byte encoding
    return one;
  }
  // two- or three-byte encoding
  uint8_t two = *(*utf8_data_in)++;
  if ((one & 0x20) == 0) {
    // two-byte encoding
    return ((one & 0x1f) << 6) | (two & 0x3f);
  }
  // three-byte encoding
  uint8_t three = *(*utf8_data_in)++;
  return ((one & 0x0f) << 12) | ((two & 0x3f) << 6) | (three & 0x3f);
}

inline int CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(const char* utf8_1,
                                                                   const char* utf8_2) {
  uint16_t c1, c2;
  do {
    c1 = *utf8_1;
    c2 = *utf8_2;
    // Did we reach a terminating character?
    if (c1 == 0) {
      return (c2 == 0) ? 0 : -1;
    } else if (c2 == 0) {
      return 1;
    }
    // Assume 1-byte value and handle all cases first.
    utf8_1++;
    utf8_2++;
    if ((c1 & 0x80) == 0) {
      if (c1 == c2) {
        // Matching 1-byte values.
        continue;
      } else {
        // Non-matching values.
        if ((c2 & 0x80) == 0) {
          // 1-byte value, do nothing.
        } else if ((c2 & 0x20) == 0) {
          // 2-byte value.
          c2 = ((c2 & 0x1f) << 6) | (*utf8_2 & 0x3f);
        } else {
          // 3-byte value.
          c2 = ((c2 & 0x0f) << 12) | ((utf8_2[0] & 0x3f) << 6) | (utf8_2[1] & 0x3f);
        }
        return static_cast<int>(c1) - static_cast<int>(c2);
      }
    }
    // Non-matching or multi-byte values.
    if ((c1 & 0x20) == 0) {
      // 2-byte value.
      c1 = ((c1 & 0x1f) << 6) | (*utf8_1 & 0x3f);
      utf8_1++;
    } else {
      // 3-byte value.
      c1 = ((c1 & 0x0f) << 12) | ((utf8_1[0] & 0x3f) << 6) | (utf8_1[1] & 0x3f);
      utf8_1 += 2;
    }
    if ((c2 & 0x80) == 0) {
      // 1-byte value, do nothing.
    } else if ((c2 & 0x20) == 0) {
      // 2-byte value.
      c2 = ((c2 & 0x1f) << 6) | (*utf8_2 & 0x3f);
      utf8_2++;
    } else {
      // 3-byte value.
      c2 = ((c2 & 0x0f) << 12) | ((utf8_2[0] & 0x3f) << 6) | (utf8_2[1] & 0x3f);
      utf8_2 += 2;
    }
  } while (c1 == c2);
  return static_cast<int>(c1) - static_cast<int>(c2);
}

}  // namespace art

#endif  // ART_RUNTIME_UTF_INL_H_
