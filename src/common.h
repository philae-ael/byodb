#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

using usize = std::size_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

#define KB(x) 1024 * x
#define MB(x) 1024 * x

#define EXPORTC extern "C" __attribute__((visibility("default"))) __attribute__((used))
