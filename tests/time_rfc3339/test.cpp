//===- test.cpp - Test routine ----------------------------------*- C++ -*-===//
//
/// \file
/// Time for RFC3339 test routine
//
// Author:  zxh
// Date:    2021/10/11 21:29:40
//===----------------------------------------------------------------------===//

#include "test.h"
#include <cstdlib>
#include <cstdio>
#include <ratio>
#include <time.h>
#include <time_rfc3339/time_rfc3339.h>

using namespace time_rfc3339;

void test_from_unix_second() {
  // 1633959411 2021-10-11 13:36:51 UTC
  Time t(1633959411);

  TEST_INT_EQ(t.year(), 2021);
  TEST_INT_EQ(t.month(), 10);
  TEST_INT_EQ(t.day(), 11);
  TEST_INT_EQ(t.weekday(), 1);
  TEST_INT_EQ(t.hour(), 13);
  TEST_INT_EQ(t.minute(), 36);
  TEST_INT_EQ(t.second(), 51);
  TEST_INT_EQ(t.nanosecond(), 0);
  TEST_INT64_EQ(t.count(), 1633959411 * std::nano::den);

  TEST_STRING_EQ(t.format(), "2021-10-11T13:36:51Z");
}

void test_timezone_UTC() {
  TEST_LONG_EQ(Time::now().timezone().first, 0L);
  TEST_STRING_EQ(Time::now().timezone().second, "UTC");
}

void test_format() {
  Time now = Time::now();
  char buf[32];

  int n = snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                   now.year(), now.month(), now.day(), now.hour(), now.minute(),
                   now.second());

  TEST_STRING_EQ(now.format(), std::string(buf, static_cast<size_t>(n)));
}

void test_Time() {
  test_from_unix_second();
  test_timezone_UTC();
  test_format();
}

int main() {
  setenv("TZ", "UTC", 1);
  tzset();
  test_Time();
  PRINT_PASS_RATE();
  return ALL_TEST_PASS() ? 0 : 1;
}
