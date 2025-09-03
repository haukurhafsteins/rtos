// include/rtos_assert.hpp
#pragma once

#include <cstddef>

// Always-available backend entry point (implemented per RTOS)
namespace rtos::backend {
    [[noreturn]] void assert_fail(const char* expr,
                                  const char* file,
                                  int line,
                                  const char* func) noexcept;
}

// Debug-only assertion (compiled out with NDEBUG)
#ifndef NDEBUG
  #define RTOS_ASSERT(cond) \
      ((cond) ? (void)0 : ::rtos::backend::assert_fail(#cond, __FILE__, __LINE__, __func__))
#else
  #define RTOS_ASSERT(cond) ((void)0)
#endif

// Always-on check (never compiled out)
#define RTOS_ENSURE(cond) \
    ((cond) ? (void)0 : ::rtos::backend::assert_fail(#cond, __FILE__, __LINE__, __func__))

// Unconditional “this should never happen”
#define RTOS_UNREACHABLE() \
    ::rtos::backend::assert_fail("unreachable", __FILE__, __LINE__, __func__)
