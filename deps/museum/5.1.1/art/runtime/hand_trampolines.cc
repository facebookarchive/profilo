#include <museum/5.1.1/external/libcxx/string>

namespace art {
  namespace mirror {
    class ArtMethod;
  }

  std::string PrettyMethod(mirror::ArtMethod* p1, bool p2)  {
    return ""; // don't bother faking diff-STL std::string magic, this isn't important enough
  }

  std::string StringPrintf(const char* p1, ...) {
    // forwarding varargs doesn't seem possible, so just return an empty string
    // also don't bother faking diff-STL std::string magic, this isn't important enough
    return "";
  }
} // namespace art
