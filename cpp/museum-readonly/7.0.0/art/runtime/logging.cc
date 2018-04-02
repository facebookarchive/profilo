#include <museum/7.0.0/art/runtime/base/logging.h>
#include <museum/7.0.0/art/runtime/base/logging-inl.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

LogMessage::LogMessage(char const*, unsigned int, LogSeverity, int) { }

LogMessage::~LogMessage() { }

std::ostream& LogMessage::stream() {
  class nullstream : public std::ostream {
  public:
    nullstream() : std::ostream(&nb_) { }

  private:
    class nullbuf : public std::streambuf {
    public:
      int overflow(int c) { return c; }
    };
    nullbuf nb_;
  };

  static nullstream ns;
  return ns;
}

} } } }
