
#include "log.h"
#include <cstring>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;


namespace AlLogger {
  std::unique_ptr<Logger> Logger::stderrLogger;
  std::unique_ptr<Logger> Logger::stdoutLogger;
  std::string stdsprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string ret = vstdsprintf(format, args);
    va_end(args);
    return ret;
  }

  std::string vstdsprintf(const char *format, std::va_list args) {
    auto bufSize = vsnprintf(nullptr, 0, format, args);
    auto buf = new char[bufSize];
    vsnprintf(buf, bufSize, format, args);
    string s(buf);
    delete []buf;
    return s;
  }

  Logger &Logger::log(const char *level) {
    std::stringstream ss;
    auto now = std::chrono::system_clock::now();
    auto n = std::chrono::high_resolution_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S%z");
    os << "[" << level << "] " << ss.str() << "  ";
    return *this;
  }

  Logger &Logger::d() {
    return log(Debug);
  }

  Logger &Logger::i() {
    return log(Info);
  }
  Logger &Logger::w() {
    return log(Warning);
  }

  Logger &Logger::e() {
    return log(Error);
  }

  Logger &Logger::getStdErrLogger() {
    if (!stderrLogger) {
      stderrLogger = std::make_unique<Logger>(std::cerr);
    }
    return *stderrLogger;
  }

}

