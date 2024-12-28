#pragma once

// IWYU pragma: private

#if defined(__clang__)
  #define CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
  #define GCC 1
#elif defined(_MSC_VER)
  #define MSVC 1
#else
  #error Unknown Compiler
#endif

#if defined(__linux__)
  #define LINUX 1
#elif defined(_WIN32)
  #define WINDOWS 1
#else
  #error Unknown OS
#endif

#if defined(__x86_64__) || defined(_M_X64)
  #define X86_64 1
#else
  #error Unknown OS
#endif

#ifndef GCC
  #define GCC 0
#endif
#ifndef CLANG
  #define CLANG 0
#endif

#ifndef MSVC
  #define MSVC 0
#endif

#ifndef WINDOWS
  #define WINDOWS 0
#endif

#ifndef LINUX
  #define LINUX 0
#endif

#ifndef X86_64
  #define X86_64 0
#endif
