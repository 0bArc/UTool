#pragma once

#include <iostream>

inline int& testFailures() {
  static int failures = 0;
  return failures;
}

#define CHECK(cond)                                                                 \
  do {                                                                              \
    if (!(cond)) {                                                                  \
      std::cerr << __FILE__ << ":" << __LINE__ << " CHECK failed: " << #cond << '\n'; \
      ++testFailures();                                                             \
    }                                                                               \
  } while (0)

#define CHECK_EQ(a, b)                                                              \
  do {                                                                              \
    const auto& _utool_a = (a);                                                     \
    const auto& _utool_b = (b);                                                     \
    if (!(_utool_a == _utool_b)) {                                                  \
      std::cerr << __FILE__ << ":" << __LINE__ << " CHECK_EQ failed: " << #a       \
                << " == " << #b << '\n';                                            \
      ++testFailures();                                                             \
    }                                                                               \
  } while (0)
