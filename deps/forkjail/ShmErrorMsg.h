// Copyright 2004-present Facebook. All Rights Reserved.

namespace facebook {
namespace forkjail {

/**
 * An error message based upon shared memory for use with ForkJail.
 *
 * ShmErrorMsg creates and manages a page of shared memory as a character string
 * and allows multiple processes access to it. No synchronization is provided.
 */
class ShmErrorMsg {
public:
  /**
   * Maps 4kb of shared memory to use as an error message buffer.
   *
   * Throws std::bad_alloc in case of mmap failure.
   */
  ShmErrorMsg();

  /**
   * Unmaps the internally-held shared mapping.
   *
   * Note that this only unmaps it for the current process. If another process
   * still has access to this object, then it will still safely be able to use it.
   */
  ~ShmErrorMsg();

  /**
   * Copies up to 4095 characters of msg into the shared memory. This is no more
   * computationally expensive than a regular strcpy, but it *is* a copy.
   */
  void set(char const* msg);

  char const* get() const {
    return map_;
  }
private:
  char* const map_;

  static constexpr size_t kPageSize = 4096;
};

} } // namespace facebook::forkjail
