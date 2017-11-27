#include <base/stringprintf.h>
#include <utils.h>

#include <museum/libart.h>

namespace facebook { namespace museum { namespace art { namespace detail {
  template<typename T>
  T* lookup___ZN3art12PrettyMethodEPNS_6mirror9ArtMethodEb() {
    static auto const symbol = ::facebook::libart().get_symbol<T>({
      "_ZN3art12PrettyMethodEPNS_6mirror9ArtMethodEb",
    });
    return symbol;
  }
} } } } // namespace facebook::museum::art::detail
namespace art {

  std::string PrettyMethod(mirror::ArtMethod* p1, bool p2)  {
    /*** define a custom struct that accounts for the std::string size difference */
    struct libcxx_string {
      std::string str;
      uint8_t padding[8];
    };
    /****/

    return
      ::facebook::museum::art::detail::lookup___ZN3art12PrettyMethodEPNS_6mirror9ArtMethodEb
      <libcxx_string
        (mirror::ArtMethod*, bool)>()
      (p1, p2)
      .str; /* return what we expect */
  }

  std::string StringPrintf(const char* p1, ...) {
    // forwarding varargs doesn't seem possible, so just return an empty string
    return "";
  }
} // namespace art
