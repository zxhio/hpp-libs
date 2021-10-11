//===- test.cpp - Test routine ----------------------------------*- C++ -*-===//
//
/// \file
/// Time for RFC3339 test routine
//
// Author:  zxh
// Date:    2021/10/11 21:29:40
//===----------------------------------------------------------------------===//

#include "test.h"
#include <cstdio>
#include <ratio>
#include <time_rfc3339.h>

using namespace time_rfc3339;

void test_from_unix_second() {
  // 1633959411 2021-10-11 21:36:51 CST
  Time t(1633959411);

  TEST_INT_EQ(t.year(), 2021);
  TEST_INT_EQ(t.month(), 10);
  TEST_INT_EQ(t.day(), 11);
  TEST_INT_EQ(t.weekday(), 1);
  TEST_INT_EQ(t.hour() + 8, 21);
  TEST_INT_EQ(t.minute(), 36);
  TEST_INT_EQ(t.second(), 51);
  TEST_INT_EQ(t.nanosecond(), 0);
  TEST_INT64_EQ(t.count(), 1633959411 * std::nano::den);

  TEST_STRING_EQ(t.format(), "2021-10-11T13:36:51+08:00");
}

void test_timezone_CST() {
  TEST_LONG_EQ(Time::now().timezone().first, 8 * 3600L);
  TEST_STRING_EQ(Time::now().timezone().second, "CST");
}

void test_format() {
  Time now = Time::now();
  char buf[40];

  int n = snprintf(
      buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d+%02ld:%02ld", now.year(),
      now.month(), now.day(), now.hour(), now.minute(), now.second(),
      now.timezone().first / 3600, now.timezone().first % 3600);

  TEST_STRING_EQ(now.format(), std::string(buf, n));
}

void test_Time() {
  test_from_unix_second();
  test_timezone_CST();
  test_format();
}

int main() {
  test_Time();
  PRINT_PASS_RATE();
}