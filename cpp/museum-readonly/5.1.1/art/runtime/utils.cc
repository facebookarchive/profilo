#include <museum/5.1.1/external/libcxx/algorithm>

#include <museum/5.1.1/art/runtime/utils.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

/**
 * NOTE: These two functions are *intentionally* not calling through to the real ART functions.
 *
 * ART's STL has a different structure for std::string than ours (their string is 12 bytes, ours is 4).
 * If we were to call their impl, it would expect us to pull 12 bytes off the stack, and we would
 * only pull 4, resulting in stack corruption despite "type correctness". So far, the best (only) solution
 * thats allows us std::string "interop" we have is to change the headers so that any std::string is
 * immediately followed by a uint8_t[8] variable - this fixes struct/class size differences and is
 * compatible-enough with our std::string. However, changing every std::string parameter and return value
 * for function declarations is a little less savory than slipping in an extra eight bytes after fields.
 */

// TODO: get rid of these with libcxx import

std::string DotToDescriptor(char const* class_name) {
  std::string descriptor(class_name);
  std::replace(descriptor.begin(), descriptor.end(), '.', '/');
  if (descriptor.length() > 0 && descriptor[0] != '[') {
    descriptor = "L" + descriptor + ";";
  }
  return descriptor;
}

std::string DescriptorToDot(char const* descriptor) {
  size_t length = strlen(descriptor);
  if (length > 1) {
    if (descriptor[0] == 'L' && descriptor[length - 1] == ';') {
      // Descriptors have the leading 'L' and trailing ';' stripped.
      std::string result(descriptor + 1, length - 2);
      std::replace(result.begin(), result.end(), '/', '.');
      return result;
    } else {
      // For arrays the 'L' and ';' remain intact.
      std::string result(descriptor);
      std::replace(result.begin(), result.end(), '/', '.');
      return result;
    }
  }
  // Do nothing for non-class/array descriptors.
  return descriptor;
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
