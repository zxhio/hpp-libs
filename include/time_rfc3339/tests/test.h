//===- test.h - Test Common Header ------------------------------*- C++ -*-===//
//
/// \file
/// Some macro helper
//
// Author:  zxh
// Date:    2021/10/11 21:28:29
//===----------------------------------------------------------------------===//

#pragma once

#include <inttypes.h>
#include <stdio.h>

#include <string>

static int test_count = 0;
static int test_pass = 0;

#define EQ(equality, actual, expect, format)                                   \
  do {                                                                         \
    test_count++;                                                              \
    if (equality)                                                              \
      test_pass++;                                                             \
    else {                                                                     \
      fprintf(stderr, "%s:%d: actual: '" format "' expect: '" format "'\n",    \
              __FILE__, __LINE__, actual, expect);                             \
    }                                                                          \
  } while (0)

#define TEST_INT64_EQ(actual, expect)                                          \
  EQ(actual == expect, actual, expect, "%" PRId64)
#define TEST_LONG_EQ(actual, expect) EQ(actual == expect, actual, expect, "%ld")
#define TEST_INT_EQ(actual, expect) EQ(actual == expect, actual, expect, "%d")
#define TEST_CHAR_EQ(actual, expect) EQ(actual == expect, actual, expect, "%c")

#define TEST_STRING_EQ(actual, expect)                                         \
  do {                                                                         \
    std::string a(actual);                                                     \
    std::string e(expect);                                                     \
    EQ(a.compare(e) == 0, a.c_str(), e.c_str(), "%s");                         \
  } while (0)

#define PRINT_PASS_RATE()                                                      \
  do {                                                                         \
    fprintf(stderr, "[%.2f%%] all test: %d, pass: %d.\n",                      \
            test_count == 0                                                    \
                ? 0                                                            \
                : static_cast<float>(test_pass * 100) / test_count,            \
            test_count, test_pass);                                            \
  } while (0)

#define ALL_TEST_PASS() (test_count == test_pass)