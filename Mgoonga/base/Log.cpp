#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#endif
namespace base
{
  //---------------------------------------------------------------
  /*void Log(const std::string& _info)
  {
    if(true)
      std::cout << _info << std::endl;
  }*/

  namespace {
    std::mutex& log_mutex() {
      static std::mutex m;
      return m;
    }

    // formats into a std::string using va_list (no trailing newline added)
    std::string vformat(const char* fmt, va_list ap) {
      va_list ap_copy;
      va_copy(ap_copy, ap);
      const int needed = std::vsnprintf(nullptr, 0, fmt, ap_copy);
      va_end(ap_copy);
      if (needed <= 0) return std::string();

      std::string s;
      s.resize(static_cast<size_t>(needed));
      std::vsnprintf(s.data(), s.size() + 1, fmt, ap); // +1 for the hidden '\0'
      return s;
    }

    // writes a single line to outputs
    void write_line(const std::string& line) {
      // single place to lock to keep lines intact across threads
      std::lock_guard<std::mutex> lk(log_mutex());
      std::cout << line << std::endl;
#if defined(_WIN32)
      OutputDebugStringA(line.c_str());
      OutputDebugStringA("\n");
#endif
    }
  }

  // ------- public API --------
  void Log(const std::string& msg) {
    write_line(msg);
  }

  void VLog(const char* fmt, va_list args) {
    const std::string s = vformat(fmt, args);
    write_line(s);
  }

  void Log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VLog(fmt, args);
    va_end(args);
  }
}