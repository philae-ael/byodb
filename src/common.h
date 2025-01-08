#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "attributes.h"
#include "macro_utils.h"

#define KB(x) 1024 * x
#define MB(x) 1024 * KB(x)

#define todo(...)                                                                                                  \
  panic __VA_OPT__(DROP_4_ARG)(                                                                                    \
      "function %s has not been implemented in file %s:%d", __func__, __FILE__, __LINE__ __VA_OPT__(, __VA_ARGS__) \
  )

using usize = std::size_t;
using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using uptr  = std::uintptr_t;

#define ALIGN_MASK_DOWN(x, mask) ((x) & ~(mask))
#define ALIGN_DOWN(x, AMOUNT) ((decltype(x))ALIGN_MASK_DOWN((uptr(x)), AMOUNT - 1))

[[noreturn]] void panic(const char* msg, ...) PRINTF_ATTRIBUTE(1, 2);
