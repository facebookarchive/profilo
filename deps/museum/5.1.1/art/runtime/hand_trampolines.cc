#include <museum/5.1.1/external/libcxx/string>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
  std::string StringPrintf(const char* p1, ...) {
    // forwarding varargs doesn't seem possible, so just return an empty string
    return "";
  }
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
