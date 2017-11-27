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
#ifndef ART_RUNTIME_LAMBDA_CLOSURE_H_
#define ART_RUNTIME_LAMBDA_CLOSURE_H_

#include "base/macros.h"
#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "lambda/shorty_field_type.h"

#include <stdint.h>

namespace art {
class ArtMethod;  // forward declaration

namespace mirror {
class Object;  // forward declaration
}  // namespace mirror

namespace lambda {
class ArtLambdaMethod;  // forward declaration
class ClosureBuilder;   // forward declaration

// Inline representation of a lambda closure.
// Contains the target method and the set of packed captured variables as a copy.
//
// The closure itself is logically immutable, although in practice any object references
// it (recursively) contains can be moved and updated by the GC.
struct PACKED(sizeof(ArtLambdaMethod*)) Closure {
  // Get the size of the Closure in bytes.
  // This is necessary in order to allocate a large enough area to copy the Closure into.
  // Do *not* copy the closure with memcpy, since references also need to get moved.
  size_t GetSize() const;

  // Copy this closure into the target, whose memory size is specified by target_size.
  // Any object references are fixed up during the copy (if there was a read barrier).
  // The target_size must be at least as large as GetSize().
  void CopyTo(void* target, size_t target_size) const;

  // Get the target method, i.e. the method that will be dispatched into with invoke-lambda.
  ArtMethod* GetTargetMethod() const;

  // Calculates the hash code. Value is recomputed each time.
  uint32_t GetHashCode() const SHARED_REQUIRES(Locks::mutator_lock_);

  // Is this the same closure as other? e.g. same target method, same variables captured.
  //
  // Determines whether the two Closures are interchangeable instances.
  // Does *not* call Object#equals recursively. If two Closures compare ReferenceEquals true that
  // means that they are interchangeable values (usually for the purpose of boxing/unboxing).
  bool ReferenceEquals(const Closure* other) const SHARED_REQUIRES(Locks::mutator_lock_);

  // How many variables were captured?
  size_t GetNumberOfCapturedVariables() const;

  // Returns a type descriptor string that represents each captured variable.
  // e.g. "Ljava/lang/Object;ZB" would mean a capture tuple of (Object, boolean, byte)
  const char* GetCapturedVariablesTypeDescriptor() const;

  // Returns the short type for the captured variable at index.
  // Index must be less than the number of captured variables.
  ShortyFieldType GetCapturedShortyType(size_t index) const;

  // Returns the 32-bit representation of a non-wide primitive at the captured variable index.
  // Smaller types are zero extended.
  // Index must be less than the number of captured variables.
  uint32_t GetCapturedPrimitiveNarrow(size_t index) const;
  // Returns the 64-bit representation of a wide primitive at the captured variable index.
  // Smaller types are zero extended.
  // Index must be less than the number of captured variables.
  uint64_t GetCapturedPrimitiveWide(size_t index) const;
  // Returns the object reference at the captured variable index.
  // The type at the index *must* be an object reference or a CHECK failure will occur.
  // Index must be less than the number of captured variables.
  mirror::Object* GetCapturedObject(size_t index) const SHARED_REQUIRES(Locks::mutator_lock_);

  // Gets the size of a nested capture closure in bytes, at the captured variable index.
  // The type at the index *must* be a lambda closure or a CHECK failure will occur.
  size_t GetCapturedClosureSize(size_t index) const;

  // Copies a nested lambda closure at the captured variable index.
  // The destination must have enough room for the closure (see GetCapturedClosureSize).
  void CopyCapturedClosure(size_t index, void* destination, size_t destination_room) const;

 private:
  // Read out any non-lambda value as a copy.
  template <typename T>
  T GetCapturedVariable(size_t index) const;

  // Reconstruct the closure's captured variable info at runtime.
  struct VariableInfo {
    size_t index_;
    ShortyFieldType variable_type_;
    size_t offset_;
    size_t count_;

    enum Flags {
      kIndex = 0x1,
      kVariableType = 0x2,
      kOffset = 0x4,
      kCount = 0x8,
    };

    // Traverse to the end of the type descriptor list instead of stopping at some particular index.
    static constexpr size_t kUpToIndexMax = static_cast<size_t>(-1);
  };

  // Parse a type descriptor, stopping at index "upto_index".
  // Returns only the information requested in flags. All other fields are indeterminate.
  template <VariableInfo::Flags flags>
  inline VariableInfo ALWAYS_INLINE ParseTypeDescriptor(const char* type_descriptor,
                                                        size_t upto_index) const;

  // Convenience function to call ParseTypeDescriptor with just the type and offset.
  void GetCapturedVariableTypeAndOffset(size_t index,
                                        ShortyFieldType* out_type,
                                        size_t* out_offset) const;

  // How many bytes do the captured variables take up? Runtime sizeof(captured_variables).
  size_t GetCapturedVariablesSize() const;
  // Get the size in bytes of the variable_type which is potentially stored at offset.
  size_t GetCapturedVariableSize(ShortyFieldType variable_type, size_t offset) const;
  // Get the starting offset (in bytes) for the 0th captured variable.
  // All offsets are relative to 'captured_'.
  size_t GetStartingOffset() const;
  // Get the offset for this index.
  // All offsets are relative to 'captuerd_'.
  size_t GetCapturedVariableOffset(size_t index) const;

  // Cast the data at '(char*)captured_[offset]' into T, returning its address.
  // This value should not be de-referenced directly since its unaligned.
  template <typename T>
  inline const uint8_t* GetUnsafeAtOffset(size_t offset) const;

  // Copy the data at the offset into the destination. DCHECKs that
  // the destination_room is large enough (in bytes) to fit the data.
  template <typename T>
  inline void CopyUnsafeAtOffset(size_t offset,
                                 void* destination,
                                 size_t src_size = sizeof(T),
                                 size_t destination_room = sizeof(T)) const;

  // Get the closure size from an unaligned (i.e. interior) closure pointer.
  static size_t GetClosureSize(const uint8_t* closure);

  ///////////////////////////////////////////////////////////////////////////////////

  // Compile-time known lambda information such as the type descriptor and size.
  ArtLambdaMethod* lambda_info_;

  // A contiguous list of captured variables, and possibly the closure size.
  // The runtime size can always be determined through GetSize().
  union {
    // Read from here if the closure size is static (ArtLambdaMethod::IsStatic)
    uint8_t static_variables_[0];
    struct {
      // Read from here if the closure size is dynamic (ArtLambdaMethod::IsDynamic)
      size_t size_;  // The lambda_info_ and the size_ itself is also included as part of the size.
      uint8_t variables_[0];
    } dynamic_;
  } captured_[0];
  // captured_ will always consist of one array element at runtime.
  // Set to [0] so that 'size_' is not counted in sizeof(Closure).

  friend class ClosureBuilder;
  friend class ClosureTest;
};

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_CLOSURE_H_
