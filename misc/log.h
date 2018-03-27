#pragma once
#include <string>
#include <cstdarg>
#include <ostream>
#include <memory>


namespace AlLogger {
  constexpr const char *Debug = "D";
  constexpr const char *Info = "I";
  constexpr const char *Normal = "N";
  constexpr const char *Warning = "W";
  constexpr const char *Error = "E";

  std::string stdsprintf(const char *s, ...);
  std::string vstdsprintf(const char *s, std::va_list);

  class Logger {
  public:
    static Logger &getStdErrLogger();

    explicit Logger(std::ostream &os) :os(os) { }

    template <typename T>
    Logger &operator<<(const T &t) {
      os << t;
      return *this;
    }
    Logger &d();
    Logger &i();
    Logger &w();
    Logger &e();

    Logger &log(const char *level);

  private:
    static std::unique_ptr<Logger> stderrLogger;
    static std::unique_ptr<Logger> stdoutLogger;
    std::ostream &os;
  };
}
