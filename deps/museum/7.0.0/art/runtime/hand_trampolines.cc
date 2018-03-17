#include <museum/7.0.0/external/libcxx/string>

#include <museum/7.0.0/art/runtime/elf_file.h>
#include <museum/7.0.0/art/runtime/elf_file_impl.h>
#include <museum/libart.h>

namespace art {
  class ArtMethod;

  std::string PrettyMethod(ArtMethod* p1, bool p2)  {
    return ""; // don't bother faking diff-STL std::string magic, this isn't important enough
  }

  std::string StringPrintf(const char* p1, ...) {
    // forwarding varargs doesn't seem possible, so just return an empty string
    // also don't bother faking diff-STL std::string magic, this isn't important enough
    return "";
  }
} // namespace art

namespace facebook { namespace museum { namespace art { namespace detail {
  template<typename T>
  T* lookup___ZN3art11ElfFileImplI10ElfTypes32ED1Ev() {
    static auto const symbol = ::facebook::libart().get_symbol<T>({
      "_ZN3art11ElfFileImplI10ElfTypes32ED1Ev",
    });
    return symbol;
  }
} } } } // namespace facebook::museum::art::detail
namespace art {

   template<>
   ElfFileImpl<ElfTypes32>::~ElfFileImpl()  {

      ::facebook::museum::art::detail::lookup___ZN3art11ElfFileImplI10ElfTypes32ED1Ev
      <void
        (ElfFileImpl<ElfTypes32> *)>()
      (this);
  }
} // namespace art


namespace facebook { namespace museum { namespace art { namespace detail {
  template<typename T>
  T* lookup___ZN3art11ElfFileImplI10ElfTypes64ED1Ev() {
    static auto const symbol = ::facebook::libart().get_symbol<T>({
      "_ZN3art11ElfFileImplI10ElfTypes64ED1Ev",
    });
    return symbol;
  }
} } } } // namespace facebook::museum::art::detail
namespace art {

   template<>
   ElfFileImpl<ElfTypes64>::~ElfFileImpl()  {

      ::facebook::museum::art::detail::lookup___ZN3art11ElfFileImplI10ElfTypes64ED1Ev
      <void
        (ElfFileImpl<ElfTypes64> *)>()
      (this);
  }
} // namespace art
