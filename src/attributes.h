#pragma once
#include "platform.h"

#define EXPORTC extern "C" __attribute__((visibility("default"))) __attribute__((used))

#if GCC || CLANG
  #define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
#elif MSVC
  #define PRINTF_ATTRIBUTE(a, b)
#else
  #error platform not supported
#endif
