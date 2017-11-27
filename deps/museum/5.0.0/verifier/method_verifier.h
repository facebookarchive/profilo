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

#ifndef ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_
#define ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/casts.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "class_reference.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "instruction_flags.h"
#include "method_reference.h"
#include "reg_type.h"
#include "reg_type_cache.h"
#include "register_line.h"
#include "safe_map.h"

namespace art {

struct ReferenceMap2Visitor;
template<class T> class Handle;

namespace verifier {

class MethodVerifier;
class DexPcToReferenceMap;

/*
 * "Direct" and "virtual" methods are stored independently. The type of call used to invoke the
 * method determines which list we search, and whether we travel up into superclasses.
 *
 * (<clinit>, <init>, and methods declared "private" or "static" are stored in the "direct" list.
 * All others are stored in the "virtual" list.)
 */
enum MethodType {
  METHOD_UNKNOWN  = 0,
  METHOD_DIRECT,      // <init>, private
  METHOD_STATIC,      // static
  METHOD_VIRTUAL,     // virtual, super
  METHOD_INTERFACE    // interface
};
std::ostream& operator<<(std::ostream& os, const MethodType& rhs);

/*
 * An enumeration of problems that can turn up during verification.
 * Both VERIFY_ERROR_BAD_CLASS_SOFT and VERIFY_ERROR_BAD_CLASS_HARD denote failures that cause
 * the entire class to be rejected. However, VERIFY_ERROR_BAD_CLASS_SOFT denotes a soft failure
 * that can potentially be corrected, and the verifier will try again at runtime.
 * VERIFY_ERROR_BAD_CLASS_HARD denotes a hard failure that can't be corrected, and will cause
 * the class to remain uncompiled. Other errors denote verification errors that cause bytecode
 * to be rewritten to fail at runtime.
 */
enum VerifyError {
  VERIFY_ERROR_BAD_CLASS_HARD,  // VerifyError; hard error that skips compilation.
  VERIFY_ERROR_BAD_CLASS_SOFT,  // VerifyError; soft error that verifies again at runtime.

  VERIFY_ERROR_NO_CLASS,        // NoClassDefFoundError.
  VERIFY_ERROR_NO_FIELD,        // NoSuchFieldError.
  VERIFY_ERROR_NO_METHOD,       // NoSuchMethodError.
  VERIFY_ERROR_ACCESS_CLASS,    // IllegalAccessError.
  VERIFY_ERROR_ACCESS_FIELD,    // IllegalAccessError.
  VERIFY_ERROR_ACCESS_METHOD,   // IllegalAccessError.
  VERIFY_ERROR_CLASS_CHANGE,    // IncompatibleClassChangeError.
  VERIFY_ERROR_INSTANTIATION,   // InstantiationError.
};
std::ostream& operator<<(std::ostream& os, const VerifyError& rhs);

/*
 * Identifies the type of reference in the instruction that generated the verify error
 * (e.g. VERIFY_ERROR_ACCESS_CLASS could come from a method, field, or class reference).
 *
 * This must fit in two bits.
 */
enum VerifyErrorRefType {
  VERIFY_ERROR_REF_CLASS  = 0,
  VERIFY_ERROR_REF_FIELD  = 1,
  VERIFY_ERROR_REF_METHOD = 2,
};
const int kVerifyErrorRefTypeShift = 6;

// We don't need to store the register data for many instructions, because we either only need
// it at branch points (for verification) or GC points and branches (for verification +
// type-precise register analysis).
enum RegisterTrackingMode {
  kTrackRegsBranches,
  kTrackCompilerInterestPoints,
  kTrackRegsAll,
};

// A mapping from a dex pc to the register line statuses as they are immediately prior to the
// execution of that instruction.
class PcToRegisterLineTable {
 public:
  PcToRegisterLineTable() : size_(0) {}
  ~PcToRegisterLineTable();

  // Initialize the RegisterTable. Every instruction address can have a different set of information
  // about what's in which register, but for verification purposes we only need to store it at
  // branch target addresses (because we merge into that).
  void Init(RegisterTrackingMode mode, InstructionFlags* flags, uint32_t insns_size,
            uint16_t registers_size, MethodVerifier* verifier);

  RegisterLine* GetLine(size_t idx) {
    DCHECK_LT(idx, size_);
    return register_lines_[idx];
  }

 private:
  std::unique_ptr<RegisterLine*[]> register_lines_;
  size_t size_;
};

// The verifier
class MethodVerifier {
 public:
  enum FailureKind {
    kNoFailure,
    kSoftFailure,
    kHardFailure,
  };

  /* Verify a class. Returns "kNoFailure" on success. */
  static FailureKind VerifyClass(mirror::Class* klass, bool allow_soft_failures, std::string* error)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static FailureKind VerifyClass(const DexFile* dex_file, Handle<mirror::DexCache> dex_cache,
                                 Handle<mirror::ClassLoader> class_loader,
                                 const DexFile::ClassDef* class_def,
                                 bool allow_soft_failures, std::string* error)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MethodVerifier* VerifyMethodAndDump(std::ostream& os, uint32_t method_idx,
                                             const DexFile* dex_file,
                                             Handle<mirror::DexCache> dex_cache,
                                             Handle<mirror::ClassLoader> class_loader,
                                             const DexFile::ClassDef* class_def,
                                             const DexFile::CodeItem* code_item,
                                             mirror::ArtMethod* method,
                                             uint32_t method_access_flags)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint8_t EncodePcToReferenceMapData() const;

  uint32_t DexFileVersion() const {
    return dex_file_->GetVersion();
  }

  RegTypeCache* GetRegTypeCache() {
    return &reg_types_;
  }

  // Log a verification failure.
  std::ostream& Fail(VerifyError error);

  // Log for verification information.
  std::ostream& LogVerifyInfo();

  // Dump the failures encountered by the verifier.
  std::ostream& DumpFailures(std::ostream& os);

  // Dump the state of the verifier, namely each instruction, what flags are set on it, register
  // information
  void Dump(std::ostream& os) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Fills 'monitor_enter_dex_pcs' with the dex pcs of the monitor-enter instructions corresponding
  // to the locks held at 'dex_pc' in method 'm'.
  static void FindLocksAtDexPc(mirror::ArtMethod* m, uint32_t dex_pc,
                               std::vector<uint32_t>* monitor_enter_dex_pcs)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the accessed field corresponding to the quick instruction's field
  // offset at 'dex_pc' in method 'm'.
  static mirror::ArtField* FindAccessedFieldAtDexPc(mirror::ArtMethod* m, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the invoked method corresponding to the quick instruction's vtable
  // index at 'dex_pc' in method 'm'.
  static mirror::ArtMethod* FindInvokedMethodAtDexPc(mirror::ArtMethod* m, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void Init() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void Shutdown();

  bool CanLoadClasses() const {
    return can_load_classes_;
  }

  MethodVerifier(const DexFile* dex_file, Handle<mirror::DexCache>* dex_cache,
                 Handle<mirror::ClassLoader>* class_loader, const DexFile::ClassDef* class_def,
                 const DexFile::CodeItem* code_item, uint32_t method_idx, mirror::ArtMethod* method,
                 uint32_t access_flags, bool can_load_classes, bool allow_soft_failures,
                 bool need_precise_constants) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : MethodVerifier(dex_file, dex_cache, class_loader, class_def, code_item, method_idx, method,
                       access_flags, can_load_classes, allow_soft_failures, need_precise_constants,
                       false) {}

  ~MethodVerifier();

  // Run verification on the method. Returns true if verification completes and false if the input
  // has an irrecoverable corruption.
  bool Verify() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Describe VRegs at the given dex pc.
  std::vector<int32_t> DescribeVRegs(uint32_t dex_pc);

  static void VisitStaticRoots(RootCallback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void VisitRoots(RootCallback* callback, void* arg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Accessors used by the compiler via CompilerCallback
  const DexFile::CodeItem* CodeItem() const;
  RegisterLine* GetRegLine(uint32_t dex_pc);
  const InstructionFlags& GetInstructionFlags(size_t index) const;
  mirror::ClassLoader* GetClassLoader() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::DexCache* GetDexCache() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  MethodReference GetMethodReference() const;
  uint32_t GetAccessFlags() const;
  bool HasCheckCasts() const;
  bool HasVirtualOrInterfaceInvokes() const;
  bool HasFailures() const;
  RegType& ResolveCheckedClass(uint32_t class_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Private constructor for dumping.
  MethodVerifier(const DexFile* dex_file, Handle<mirror::DexCache>* dex_cache,
                 Handle<mirror::ClassLoader>* class_loader, const DexFile::ClassDef* class_def,
                 const DexFile::CodeItem* code_item, uint32_t method_idx, mirror::ArtMethod* method,
                 uint32_t access_flags, bool can_load_classes, bool allow_soft_failures,
                 bool need_precise_constants, bool verify_to_dump)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Adds the given string to the beginning of the last failure message.
  void PrependToLastFailMessage(std::string);

  // Adds the given string to the end of the last failure message.
  void AppendToLastFailMessage(std::string);

  /*
   * Perform verification on a single method.
   *
   * We do this in three passes:
   *  (1) Walk through all code units, determining instruction locations,
   *      widths, and other characteristics.
   *  (2) Walk through all code units, performing static checks on
   *      operands.
   *  (3) Iterate through the method, checking type safety and looking
   *      for code flow problems.
   */
  static FailureKind VerifyMethod(uint32_t method_idx, const DexFile* dex_file,
                                  Handle<mirror::DexCache> dex_cache,
                                  Handle<mirror::ClassLoader> class_loader,
                                  const DexFile::ClassDef* class_def_idx,
                                  const DexFile::CodeItem* code_item,
                                  mirror::ArtMethod* method, uint32_t method_access_flags,
                                  bool allow_soft_failures, bool need_precise_constants)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void FindLocksAtDexPc() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtField* FindAccessedFieldAtDexPc(uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* FindInvokedMethodAtDexPc(uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Compute the width of the instruction at each address in the instruction stream, and store it in
   * insn_flags_. Addresses that are in the middle of an instruction, or that are part of switch
   * table data, are not touched (so the caller should probably initialize "insn_flags" to zero).
   *
   * The "new_instance_count_" and "monitor_enter_count_" fields in vdata are also set.
   *
   * Performs some static checks, notably:
   * - opcode of first instruction begins at index 0
   * - only documented instructions may appear
   * - each instruction follows the last
   * - last byte of last instruction is at (code_length-1)
   *
   * Logs an error and returns "false" on failure.
   */
  bool ComputeWidthsAndCountOps();

  /*
   * Set the "in try" flags for all instructions protected by "try" statements. Also sets the
   * "branch target" flags for exception handlers.
   *
   * Call this after widths have been set in "insn_flags".
   *
   * Returns "false" if something in the exception table looks fishy, but we're expecting the
   * exception table to be somewhat sane.
   */
  bool ScanTryCatchBlocks() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Perform static verification on all instructions in a method.
   *
   * Walks through instructions in a method calling VerifyInstruction on each.
   */
  bool VerifyInstructions();

  /*
   * Perform static verification on an instruction.
   *
   * As a side effect, this sets the "branch target" flags in InsnFlags.
   *
   * "(CF)" items are handled during code-flow analysis.
   *
   * v3 4.10.1
   * - target of each jump and branch instruction must be valid
   * - targets of switch statements must be valid
   * - operands referencing constant pool entries must be valid
   * - (CF) operands of getfield, putfield, getstatic, putstatic must be valid
   * - (CF) operands of method invocation instructions must be valid
   * - (CF) only invoke-direct can call a method starting with '<'
   * - (CF) <clinit> must never be called explicitly
   * - operands of instanceof, checkcast, new (and variants) must be valid
   * - new-array[-type] limited to 255 dimensions
   * - can't use "new" on an array class
   * - (?) limit dimensions in multi-array creation
   * - local variable load/store register values must be in valid range
   *
   * v3 4.11.1.2
   * - branches must be within the bounds of the code array
   * - targets of all control-flow instructions are the start of an instruction
   * - register accesses fall within range of allocated registers
   * - (N/A) access to constant pool must be of appropriate type
   * - code does not end in the middle of an instruction
   * - execution cannot fall off the end of the code
   * - (earlier) for each exception handler, the "try" area must begin and
   *   end at the start of an instruction (end can be at the end of the code)
   * - (earlier) for each exception handler, the handler must start at a valid
   *   instruction
   */
  bool VerifyInstruction(const Instruction* inst, uint32_t code_offset);

  /* Ensure that the register index is valid for this code item. */
  bool CheckRegisterIndex(uint32_t idx);

  /* Ensure that the wide register index is valid for this code item. */
  bool CheckWideRegisterIndex(uint32_t idx);

  // Perform static checks on a field Get or set instruction. All we do here is ensure that the
  // field index is in the valid range.
  bool CheckFieldIndex(uint32_t idx);

  // Perform static checks on a method invocation instruction. All we do here is ensure that the
  // method index is in the valid range.
  bool CheckMethodIndex(uint32_t idx);

  // Perform static checks on a "new-instance" instruction. Specifically, make sure the class
  // reference isn't for an array class.
  bool CheckNewInstance(uint32_t idx);

  /* Ensure that the string index is in the valid range. */
  bool CheckStringIndex(uint32_t idx);

  // Perform static checks on an instruction that takes a class constant. Ensure that the class
  // index is in the valid range.
  bool CheckTypeIndex(uint32_t idx);

  // Perform static checks on a "new-array" instruction. Specifically, make sure they aren't
  // creating an array of arrays that causes the number of dimensions to exceed 255.
  bool CheckNewArray(uint32_t idx);

  // Verify an array data table. "cur_offset" is the offset of the fill-array-data instruction.
  bool CheckArrayData(uint32_t cur_offset);

  // Verify that the target of a branch instruction is valid. We don't expect code to jump directly
  // into an exception handler, but it's valid to do so as long as the target isn't a
  // "move-exception" instruction. We verify that in a later stage.
  // The dex format forbids certain instructions from branching to themselves.
  // Updates "insn_flags_", setting the "branch target" flag.
  bool CheckBranchTarget(uint32_t cur_offset);

  // Verify a switch table. "cur_offset" is the offset of the switch instruction.
  // Updates "insn_flags_", setting the "branch target" flag.
  bool CheckSwitchTargets(uint32_t cur_offset);

  // Check the register indices used in a "vararg" instruction, such as invoke-virtual or
  // filled-new-array.
  // - vA holds word count (0-5), args[] have values.
  // There are some tests we don't do here, e.g. we don't try to verify that invoking a method that
  // takes a double is done with consecutive registers. This requires parsing the target method
  // signature, which we will be doing later on during the code flow analysis.
  bool CheckVarArgRegs(uint32_t vA, uint32_t arg[]);

  // Check the register indices used in a "vararg/range" instruction, such as invoke-virtual/range
  // or filled-new-array/range.
  // - vA holds word count, vC holds index of first reg.
  bool CheckVarArgRangeRegs(uint32_t vA, uint32_t vC);

  // Extract the relative offset from a branch instruction.
  // Returns "false" on failure (e.g. this isn't a branch instruction).
  bool GetBranchOffset(uint32_t cur_offset, int32_t* pOffset, bool* pConditional,
                       bool* selfOkay);

  /* Perform detailed code-flow analysis on a single method. */
  bool VerifyCodeFlow() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Set the register types for the first instruction in the method based on the method signature.
  // This has the side-effect of validating the signature.
  bool SetTypesFromSignature() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Perform code flow on a method.
   *
   * The basic strategy is as outlined in v3 4.11.1.2: set the "changed" bit on the first
   * instruction, process it (setting additional "changed" bits), and repeat until there are no
   * more.
   *
   * v3 4.11.1.1
   * - (N/A) operand stack is always the same size
   * - operand stack [registers] contain the correct types of values
   * - local variables [registers] contain the correct types of values
   * - methods are invoked with the appropriate arguments
   * - fields are assigned using values of appropriate types
   * - opcodes have the correct type values in operand registers
   * - there is never an uninitialized class instance in a local variable in code protected by an
   *   exception handler (operand stack is okay, because the operand stack is discarded when an
   *   exception is thrown) [can't know what's a local var w/o the debug info -- should fall out of
   *   register typing]
   *
   * v3 4.11.1.2
   * - execution cannot fall off the end of the code
   *
   * (We also do many of the items described in the "static checks" sections, because it's easier to
   * do them here.)
   *
   * We need an array of RegType values, one per register, for every instruction. If the method uses
   * monitor-enter, we need extra data for every register, and a stack for every "interesting"
   * instruction. In theory this could become quite large -- up to several megabytes for a monster
   * function.
   *
   * NOTE:
   * The spec forbids backward branches when there's an uninitialized reference in a register. The
   * idea is to prevent something like this:
   *   loop:
   *     move r1, r0
   *     new-instance r0, MyClass
   *     ...
   *     if-eq rN, loop  // once
   *   initialize r0
   *
   * This leaves us with two different instances, both allocated by the same instruction, but only
   * one is initialized. The scheme outlined in v3 4.11.1.4 wouldn't catch this, so they work around
   * it by preventing backward branches. We achieve identical results without restricting code
   * reordering by specifying that you can't execute the new-instance instruction if a register
   * contains an uninitialized instance created by that same instruction.
   */
  bool CodeFlowVerifyMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Perform verification for a single instruction.
   *
   * This requires fully decoding the instruction to determine the effect it has on registers.
   *
   * Finds zero or more following instructions and sets the "changed" flag if execution at that
   * point needs to be (re-)evaluated. Register changes are merged into "reg_types_" at the target
   * addresses. Does not set or clear any other flags in "insn_flags_".
   */
  bool CodeFlowVerifyInstruction(uint32_t* start_guess)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of a new array instruction
  void VerifyNewArray(const Instruction* inst, bool is_filled, bool is_range)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Helper to perform verification on puts of primitive type.
  void VerifyPrimitivePut(RegType& target_type, RegType& insn_type,
                          const uint32_t vregA) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of an aget instruction. The destination register's type will be set to
  // be that of component type of the array unless the array type is unknown, in which case a
  // bottom type inferred from the type of instruction is used. is_primitive is false for an
  // aget-object.
  void VerifyAGet(const Instruction* inst, RegType& insn_type,
                  bool is_primitive) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of an aput instruction.
  void VerifyAPut(const Instruction* inst, RegType& insn_type,
                  bool is_primitive) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Lookup instance field and fail for resolution violations
  mirror::ArtField* GetInstanceField(RegType& obj_type, int field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Lookup static field and fail for resolution violations
  mirror::ArtField* GetStaticField(int field_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of an iget or sget instruction.
  void VerifyISGet(const Instruction* inst, RegType& insn_type,
                   bool is_primitive, bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of an iput or sput instruction.
  void VerifyISPut(const Instruction* inst, RegType& insn_type,
                   bool is_primitive, bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the access field of a quick field access (iget/iput-quick) or nullptr
  // if it cannot be found.
  mirror::ArtField* GetQuickFieldAccess(const Instruction* inst, RegisterLine* reg_line)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of an iget-quick instruction.
  void VerifyIGetQuick(const Instruction* inst, RegType& insn_type,
                       bool is_primitive)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Perform verification of an iput-quick instruction.
  void VerifyIPutQuick(const Instruction* inst, RegType& insn_type,
                       bool is_primitive)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolves a class based on an index and performs access checks to ensure the referrer can
  // access the resolved class.
  RegType& ResolveClassAndCheckAccess(uint32_t class_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * For the "move-exception" instruction at "work_insn_idx_", which must be at an exception handler
   * address, determine the Join of all exceptions that can land here. Fails if no matching
   * exception handler can be found or if the Join of exception types fails.
   */
  RegType& GetCaughtExceptionType()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Resolves a method based on an index and performs access checks to ensure
   * the referrer can access the resolved method.
   * Does not throw exceptions.
   */
  mirror::ArtMethod* ResolveMethodAndCheckAccess(uint32_t method_idx, MethodType method_type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Verify the arguments to a method. We're executing in "method", making
   * a call to the method reference in vB.
   *
   * If this is a "direct" invoke, we allow calls to <init>. For calls to
   * <init>, the first argument may be an uninitialized reference. Otherwise,
   * calls to anything starting with '<' will be rejected, as will any
   * uninitialized reference arguments.
   *
   * For non-static method calls, this will verify that the method call is
   * appropriate for the "this" argument.
   *
   * The method reference is in vBBBB. The "is_range" parameter determines
   * whether we use 0-4 "args" values or a range of registers defined by
   * vAA and vCCCC.
   *
   * Widening conversions on integers and references are allowed, but
   * narrowing conversions are not.
   *
   * Returns the resolved method on success, nullptr on failure (with *failure
   * set appropriately).
   */
  mirror::ArtMethod* VerifyInvocationArgs(const Instruction* inst,
                                          MethodType method_type,
                                          bool is_range, bool is_super)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Similar checks to the above, but on the proto. Will be used when the method cannot be
  // resolved.
  void VerifyInvocationArgsUnresolvedMethod(const Instruction* inst, MethodType method_type,
                                            bool is_range)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template <class T>
  mirror::ArtMethod* VerifyInvocationArgsFromIterator(T* it, const Instruction* inst,
                                                      MethodType method_type, bool is_range,
                                                      mirror::ArtMethod* res_method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* GetQuickInvokedMethod(const Instruction* inst,
                                           RegisterLine* reg_line,
                                           bool is_range)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* VerifyInvokeVirtualQuickArgs(const Instruction* inst, bool is_range)
  SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Verify that the target instruction is not "move-exception". It's important that the only way
   * to execute a move-exception is as the first instruction of an exception handler.
   * Returns "true" if all is well, "false" if the target instruction is move-exception.
   */
  bool CheckNotMoveException(const uint16_t* insns, int insn_idx);

  /*
  * Control can transfer to "next_insn". Merge the registers from merge_line into the table at
  * next_insn, and set the changed flag on the target address if any of the registers were changed.
  * In the case of fall-through, update the merge line on a change as its the working line for the
  * next instruction.
  * Returns "false" if an error is encountered.
  */
  bool UpdateRegisters(uint32_t next_insn, RegisterLine* merge_line, bool update_merge_line)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Is the method being verified a constructor?
  bool IsConstructor() const {
    return (method_access_flags_ & kAccConstructor) != 0;
  }

  // Is the method verified static?
  bool IsStatic() const {
    return (method_access_flags_ & kAccStatic) != 0;
  }

  // Return the register type for the method.
  RegType& GetMethodReturnType() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get a type representing the declaring class of the method.
  RegType& GetDeclaringClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  InstructionFlags* CurrentInsnFlags();

  RegType& DetermineCat1Constant(int32_t value, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  RegTypeCache reg_types_;

  PcToRegisterLineTable reg_table_;

  // Storage for the register status we're currently working on.
  std::unique_ptr<RegisterLine> work_line_;

  // The address of the instruction we're currently working on, note that this is in 2 byte
  // quantities
  uint32_t work_insn_idx_;

  // Storage for the register status we're saving for later.
  std::unique_ptr<RegisterLine> saved_line_;

  const uint32_t dex_method_idx_;  // The method we're working on.
  // Its object representation if known.
  mirror::ArtMethod* mirror_method_ GUARDED_BY(Locks::mutator_lock_);
  const uint32_t method_access_flags_;  // Method's access flags.
  RegType* return_type_;  // Lazily computed return type of the method.
  const DexFile* const dex_file_;  // The dex file containing the method.
  // The dex_cache for the declaring class of the method.
  Handle<mirror::DexCache>* dex_cache_ GUARDED_BY(Locks::mutator_lock_);
  // The class loader for the declaring class of the method.
  Handle<mirror::ClassLoader>* class_loader_ GUARDED_BY(Locks::mutator_lock_);
  const DexFile::ClassDef* const class_def_;  // The class def of the declaring class of the method.
  const DexFile::CodeItem* const code_item_;  // The code item containing the code for the method.
  RegType* declaring_class_;  // Lazily computed reg type of the method's declaring class.
  // Instruction widths and flags, one entry per code unit.
  std::unique_ptr<InstructionFlags[]> insn_flags_;
  // The dex PC of a FindLocksAtDexPc request, -1 otherwise.
  uint32_t interesting_dex_pc_;
  // The container into which FindLocksAtDexPc should write the registers containing held locks,
  // nullptr if we're not doing FindLocksAtDexPc.
  std::vector<uint32_t>* monitor_enter_dex_pcs_;

  // The types of any error that occurs.
  std::vector<VerifyError> failures_;
  // Error messages associated with failures.
  std::vector<std::ostringstream*> failure_messages_;
  // Is there a pending hard failure?
  bool have_pending_hard_failure_;
  // Is there a pending runtime throw failure? A runtime throw failure is when an instruction
  // would fail at runtime throwing an exception. Such an instruction causes the following code
  // to be unreachable. This is set by Fail and used to ensure we don't process unreachable
  // instructions that would hard fail the verification.
  bool have_pending_runtime_throw_failure_;

  // Info message log use primarily for verifier diagnostics.
  std::ostringstream info_messages_;

  // The number of occurrences of specific opcodes.
  size_t new_instance_count_;
  size_t monitor_enter_count_;

  const bool can_load_classes_;

  // Converts soft failures to hard failures when false. Only false when the compiler isn't
  // running and the verifier is called from the class linker.
  const bool allow_soft_failures_;

  // An optimization where instead of generating unique RegTypes for constants we use imprecise
  // constants that cover a range of constants. This isn't good enough for deoptimization that
  // avoids loading from registers in the case of a constant as the dex instruction set lost the
  // notion of whether a value should be in a floating point or general purpose register file.
  const bool need_precise_constants_;

  // Indicates the method being verified contains at least one check-cast or aput-object
  // instruction. Aput-object operations implicitly check for array-store exceptions, similar to
  // check-cast.
  bool has_check_casts_;

  // Indicates the method being verified contains at least one invoke-virtual/range
  // or invoke-interface/range.
  bool has_virtual_or_interface_invokes_;

  // Indicates whether we verify to dump the info. In that case we accept quickened instructions
  // even though we might detect to be a compiler. Should only be set when running
  // VerifyMethodAndDump.
  const bool verify_to_dump_;
};
std::ostream& operator<<(std::ostream& os, const MethodVerifier::FailureKind& rhs);

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_
