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
#ifndef ART_RUNTIME_LAMBDA_SHORTY_FIELD_TYPE_H_
#define ART_RUNTIME_LAMBDA_SHORTY_FIELD_TYPE_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/value_object.h"
#include "globals.h"
#include "runtime/primitive.h"

#include <ostream>

namespace art {

namespace mirror {
class Object;  // forward declaration
}  // namespace mirror

namespace lambda {

struct Closure;  // forward declaration

// TODO: Refactor together with primitive.h

// The short form of a field type descriptor. Corresponds to ShortyFieldType in dex specification.
// Only types usable by a field (and locals) are allowed (i.e. no void type).
// Note that arrays and objects are treated both as 'L'.
//
// This is effectively a 'char' enum-like zero-cost type-safe wrapper with extra helper functions.
struct ShortyFieldType : ValueObject {
  // Use as if this was an enum class, e.g. 'ShortyFieldType::kBoolean'.
  enum : char {
    // Primitives (Narrow):
    kBoolean = 'Z',
    kByte = 'B',
    kChar = 'C',
    kShort = 'S',
    kInt = 'I',
    kFloat = 'F',
    // Primitives (Wide):
    kLong = 'J',
    kDouble = 'D',
    // Managed types:
    kObject = 'L',  // This can also be an array (which is otherwise '[' in a non-shorty).
    kLambda = '\\',
  };  // NOTE: This is an anonymous enum so we can get exhaustive switch checking from the compiler.

  // Implicitly construct from the enum above. Value must be one of the enum list members above.
  // Always safe to use, does not do any DCHECKs.
  inline constexpr ShortyFieldType(decltype(kByte) c) : value_(c) {
  }

  // Default constructor. The initial value is undefined. Initialize before calling methods.
  // This is very unsafe but exists as a convenience to having undefined values.
  explicit ShortyFieldType() : value_(StaticCastValue(0)) {
  }

  // Explicitly construct from a char. Value must be one of the enum list members above.
  // Conversion is potentially unsafe, so DCHECKing is performed.
  explicit inline ShortyFieldType(char c) : value_(StaticCastValue(c)) {
    if (kIsDebugBuild) {
      // Verify at debug-time that our conversion is safe.
      ShortyFieldType ignored;
      DCHECK(MaybeCreate(c, &ignored)) << "unknown shorty field type '" << c << "'";
    }
  }

  // Attempts to parse the character in 'shorty_field_type' into its strongly typed version.
  // Returns false if the character was out of range of the grammar.
  static bool MaybeCreate(char shorty_field_type, ShortyFieldType* out) {
    DCHECK(out != nullptr);
    switch (shorty_field_type) {
      case kBoolean:
      case kByte:
      case kChar:
      case kShort:
      case kInt:
      case kFloat:
      case kLong:
      case kDouble:
      case kObject:
      case kLambda:
        *out = ShortyFieldType(static_cast<decltype(kByte)>(shorty_field_type));
        return true;
      default:
        break;
    }

    return false;
  }

  // Convert the first type in a field type descriptor string into a shorty.
  // Arrays are converted into objects.
  // Does not work for 'void' types (as they are illegal in a field type descriptor).
  static ShortyFieldType CreateFromFieldTypeDescriptor(const char* field_type_descriptor) {
    DCHECK(field_type_descriptor != nullptr);
    char c = *field_type_descriptor;
    if (UNLIKELY(c == kArray)) {  // Arrays are treated as object references.
      c = kObject;
    }
    return ShortyFieldType{c};  // NOLINT [readability/braces] [4]
  }

  // Parse the first type in the field type descriptor string into a shorty.
  // See CreateFromFieldTypeDescriptor for more details.
  //
  // Returns the pointer offset into the middle of the field_type_descriptor
  // that would either point to the next shorty type, or to null if there are
  // no more types.
  //
  // DCHECKs that each of the nested types is a valid shorty field type. This
  // means the type descriptor must be already valid.
  static const char* ParseFromFieldTypeDescriptor(const char* field_type_descriptor,
                                                  ShortyFieldType* out_type) {
    DCHECK(field_type_descriptor != nullptr);

    if (UNLIKELY(field_type_descriptor[0] == '\0')) {
      // Handle empty strings by immediately returning null.
      return nullptr;
    }

    // All non-empty strings must be a valid list of field type descriptors, otherwise
    // the DCHECKs will kick in and the program will crash.
    const char shorter_type = *field_type_descriptor;

    ShortyFieldType safe_type;
    bool type_set = MaybeCreate(shorter_type, &safe_type);

    // Lambda that keeps skipping characters until it sees ';'.
    // Stops one character -after- the ';'.
    auto skip_until_semicolon = [&field_type_descriptor]() {
      while (*field_type_descriptor != ';' && *field_type_descriptor != '\0') {
        ++field_type_descriptor;
      }
      DCHECK_NE(*field_type_descriptor, '\0')
          << " type descriptor terminated too early: " << field_type_descriptor;
      ++field_type_descriptor;  // Skip the ';'
    };

    ++field_type_descriptor;
    switch (shorter_type) {
      case kObject:
        skip_until_semicolon();

        DCHECK(type_set);
        DCHECK(safe_type == kObject);
        break;
      case kArray:
        // Strip out all of the leading [[[[[s, we don't care if it's a multi-dimensional array.
        while (*field_type_descriptor == '[' && *field_type_descriptor != '\0') {
          ++field_type_descriptor;
        }
        DCHECK_NE(*field_type_descriptor, '\0')
            << " type descriptor terminated too early: " << field_type_descriptor;
        // Either a primitive, object, or closure left. No more arrays.
        {
          // Now skip all the characters that form the array's interior-most element type
          // (which itself is guaranteed not to be an array).
          ShortyFieldType array_interior_type;
          type_set = MaybeCreate(*field_type_descriptor, &array_interior_type);
          DCHECK(type_set) << " invalid remaining type descriptor " << field_type_descriptor;

          // Handle array-of-objects case like [[[[[LObject; and array-of-closures like [[[[[\Foo;
          if (*field_type_descriptor == kObject || *field_type_descriptor == kLambda) {
            skip_until_semicolon();
          } else {
            // Handle primitives which are exactly one character we can skip.
            DCHECK(array_interior_type.IsPrimitive());
            ++field_type_descriptor;
          }
        }

        safe_type = kObject;
        type_set = true;
        break;
      case kLambda:
        skip_until_semicolon();

        DCHECK(safe_type == kLambda);
        DCHECK(type_set);
        break;
      default:
        DCHECK_NE(kVoid, shorter_type) << "cannot make a ShortyFieldType from a void type";
        break;
    }

    DCHECK(type_set) << "invalid shorty type descriptor " << shorter_type;

    *out_type = safe_type;
    return type_set ? field_type_descriptor : nullptr;
  }

  // Explicitly convert to a char.
  inline explicit operator char() const {
    return value_;
  }

  // Is this a primitive?
  inline bool IsPrimitive() const {
    return IsPrimitiveNarrow() || IsPrimitiveWide();
  }

  // Is this a narrow primitive (i.e. can fit into 1 virtual register)?
  inline bool IsPrimitiveNarrow() const {
    switch (value_) {
      case kBoolean:
      case kByte:
      case kChar:
      case kShort:
      case kInt:
      case kFloat:
        return true;
      default:
        return false;
    }
  }

  // Is this a wide primitive (i.e. needs exactly 2 virtual registers)?
  inline bool IsPrimitiveWide() const {
    switch (value_) {
      case kLong:
      case kDouble:
        return true;
      default:
        return false;
    }
  }

  // Is this an object reference (which can also be an array)?
  inline bool IsObject() const {
    return value_ == kObject;
  }

  // Is this a lambda?
  inline bool IsLambda() const {
    return value_ == kLambda;
  }

  // Is the size of this (to store inline as a field) always known at compile-time?
  inline bool IsStaticSize() const {
    return !IsLambda();
  }

  // Get the compile-time size (to be able to store it inline as a field or on stack).
  // Dynamically-sized values such as lambdas return the guaranteed lower bound.
  inline size_t GetStaticSize() const {
    switch (value_) {
      case kBoolean:
        return sizeof(bool);
      case kByte:
        return sizeof(uint8_t);
      case kChar:
        return sizeof(int16_t);
      case kShort:
        return sizeof(uint16_t);
      case kInt:
        return sizeof(int32_t);
      case kLong:
        return sizeof(int64_t);
      case kFloat:
        return sizeof(float);
      case kDouble:
        return sizeof(double);
      case kObject:
        return kObjectReferenceSize;
      case kLambda:
        return sizeof(void*);  // Large enough to store the ArtLambdaMethod
      default:
        DCHECK(false) << "unknown shorty field type '" << static_cast<char>(value_) << "'";
        UNREACHABLE();
    }
  }

  // Implicitly convert to the anonymous nested inner type. Used for exhaustive switch detection.
  inline operator decltype(kByte)() const {
    return value_;
  }

  // Returns a read-only static string representing the enum name, useful for printing/debug only.
  inline const char* ToString() const {
    switch (value_) {
      case kBoolean:
        return "kBoolean";
      case kByte:
        return "kByte";
      case kChar:
        return "kChar";
      case kShort:
        return "kShort";
      case kInt:
        return "kInt";
      case kLong:
        return "kLong";
      case kFloat:
        return "kFloat";
      case kDouble:
        return "kDouble";
      case kObject:
        return "kObject";
      case kLambda:
        return "kLambda";
      default:
        // Undefined behavior if we get this far. Pray the compiler gods are merciful.
        return "<undefined>";
    }
  }

 private:
  static constexpr const char kArray = '[';
  static constexpr const char kVoid  = 'V';

  // Helper to statically cast anything into our nested anonymous enum type.
  template <typename T>
  inline static decltype(kByte) StaticCastValue(const T& anything) {
    return static_cast<decltype(value_)>(anything);
  }

  // The only field in this struct.
  decltype(kByte) value_;
};


  // Print to an output stream.
inline std::ostream& operator<<(std::ostream& ostream, ShortyFieldType shorty) {
  return ostream << shorty.ToString();
}

static_assert(sizeof(ShortyFieldType) == sizeof(char),
              "ShortyFieldType must be lightweight just like a char");

// Compile-time trait information regarding the ShortyFieldType.
// Used by static_asserts to verify that the templates are correctly used at compile-time.
//
// For example,
//     ShortyFieldTypeTraits::IsPrimitiveNarrowType<int64_t>() == true
//     ShortyFieldTypeTraits::IsObjectType<mirror::Object*>() == true
struct ShortyFieldTypeTraits {
  // A type guaranteed to be large enough to holds any of the shorty field types.
  using MaxType = uint64_t;

  // Type traits: Returns true if 'T' is a valid type that can be represented by a shorty field type.
  template <typename T>
  static inline constexpr bool IsType() {
    return IsPrimitiveType<T>() || IsObjectType<T>() || IsLambdaType<T>();
  }

  // Returns true if 'T' is a primitive type (i.e. a built-in without nested references).
  template <typename T>
  static inline constexpr bool IsPrimitiveType() {
    return IsPrimitiveNarrowType<T>() || IsPrimitiveWideType<T>();
  }

  // Returns true if 'T' is a primitive type that is narrow (i.e. can be stored into 1 vreg).
  template <typename T>
  static inline constexpr bool IsPrimitiveNarrowType() {
    return IsPrimitiveNarrowTypeImpl(static_cast<T* const>(nullptr));
  }

  // Returns true if 'T' is a primitive type that is wide (i.e. needs 2 vregs for storage).
  template <typename T>
  static inline constexpr bool IsPrimitiveWideType() {
    return IsPrimitiveWideTypeImpl(static_cast<T* const>(nullptr));
  }

  // Returns true if 'T' is an object (i.e. it is a managed GC reference).
  // Note: This is equivalent to std::base_of<mirror::Object*, T>::value
  template <typename T>
  static inline constexpr bool IsObjectType() {
    return IsObjectTypeImpl(static_cast<T* const>(nullptr));
  }

  // Returns true if 'T' is a lambda (i.e. it is a closure with unknown static data);
  template <typename T>
  static inline constexpr bool IsLambdaType() {
    return IsLambdaTypeImpl(static_cast<T* const>(nullptr));
  }

 private:
#define IS_VALID_TYPE_SPECIALIZATION(type, name) \
  static inline constexpr bool Is ## name ## TypeImpl(type* const  = 0) { \
    return true; \
  } \
  \
  static_assert(sizeof(MaxType) >= sizeof(type), "MaxType too small")

  IS_VALID_TYPE_SPECIALIZATION(bool, PrimitiveNarrow);
  IS_VALID_TYPE_SPECIALIZATION(int8_t, PrimitiveNarrow);
  IS_VALID_TYPE_SPECIALIZATION(uint8_t, PrimitiveNarrow);  // Not strictly true, but close enough.
  IS_VALID_TYPE_SPECIALIZATION(int16_t, PrimitiveNarrow);
  IS_VALID_TYPE_SPECIALIZATION(uint16_t, PrimitiveNarrow);  // Chars are unsigned.
  IS_VALID_TYPE_SPECIALIZATION(int32_t, PrimitiveNarrow);
  IS_VALID_TYPE_SPECIALIZATION(uint32_t, PrimitiveNarrow);  // Not strictly true, but close enough.
  IS_VALID_TYPE_SPECIALIZATION(float, PrimitiveNarrow);
  IS_VALID_TYPE_SPECIALIZATION(int64_t, PrimitiveWide);
  IS_VALID_TYPE_SPECIALIZATION(uint64_t, PrimitiveWide);  // Not strictly true, but close enough.
  IS_VALID_TYPE_SPECIALIZATION(double, PrimitiveWide);
  IS_VALID_TYPE_SPECIALIZATION(mirror::Object*, Object);
  IS_VALID_TYPE_SPECIALIZATION(Closure*, Lambda);
#undef IS_VALID_TYPE_SPECIALIZATION

#define IS_VALID_TYPE_SPECIALIZATION_IMPL(name) \
  template <typename T> \
  static inline constexpr bool Is ## name ## TypeImpl(T* const = 0) { \
    return false; \
  }

  IS_VALID_TYPE_SPECIALIZATION_IMPL(PrimitiveNarrow);
  IS_VALID_TYPE_SPECIALIZATION_IMPL(PrimitiveWide);
  IS_VALID_TYPE_SPECIALIZATION_IMPL(Object);
  IS_VALID_TYPE_SPECIALIZATION_IMPL(Lambda);

#undef IS_VALID_TYPE_SPECIALIZATION_IMPL
};

// Maps the ShortyFieldType enum into it's C++ type equivalent, into the "type" typedef.
// For example:
//     ShortyFieldTypeSelectType<ShortyFieldType::kBoolean>::type => bool
//     ShortyFieldTypeSelectType<ShortyFieldType::kLong>::type => int64_t
//
// Invalid enums will not have the type defined.
template <decltype(ShortyFieldType::kByte) Shorty>
struct ShortyFieldTypeSelectType {
};

// Maps the C++ type into it's ShortyFieldType enum equivalent, into the "value" constexpr.
// For example:
//     ShortyFieldTypeSelectEnum<bool>::value => ShortyFieldType::kBoolean
//     ShortyFieldTypeSelectEnum<int64_t>::value => ShortyFieldType::kLong
//
// Signed-ness must match for a valid select, e.g. uint64_t will not map to kLong, but int64_t will.
// Invalid types will not have the value defined (see e.g. ShortyFieldTypeTraits::IsType<T>())
template <typename T>
struct ShortyFieldTypeSelectEnum {
};

#define SHORTY_FIELD_TYPE_SELECT_IMPL(cpp_type, enum_element)      \
template <> \
struct ShortyFieldTypeSelectType<ShortyFieldType::enum_element> { \
  using type = cpp_type; \
}; \
\
template <> \
struct ShortyFieldTypeSelectEnum<cpp_type> { \
  static constexpr const auto value = ShortyFieldType::enum_element; \
}; \

SHORTY_FIELD_TYPE_SELECT_IMPL(bool, kBoolean);
SHORTY_FIELD_TYPE_SELECT_IMPL(int8_t, kByte);
SHORTY_FIELD_TYPE_SELECT_IMPL(int16_t, kShort);
SHORTY_FIELD_TYPE_SELECT_IMPL(uint16_t, kChar);
SHORTY_FIELD_TYPE_SELECT_IMPL(int32_t, kInt);
SHORTY_FIELD_TYPE_SELECT_IMPL(float, kFloat);
SHORTY_FIELD_TYPE_SELECT_IMPL(int64_t, kLong);
SHORTY_FIELD_TYPE_SELECT_IMPL(double, kDouble);
SHORTY_FIELD_TYPE_SELECT_IMPL(mirror::Object*, kObject);
SHORTY_FIELD_TYPE_SELECT_IMPL(Closure*, kLambda);

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_SHORTY_FIELD_TYPE_H_
