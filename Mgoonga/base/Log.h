#pragma once

#include "base.h"

namespace base
{
 /* DLL_BASE void Log(const std::string&);*/

  DLL_BASE void Log(const std::string& msg);
  DLL_BASE void VLog(const char* fmt, va_list args);
  DLL_BASE void Log(const char* fmt, ...);
}