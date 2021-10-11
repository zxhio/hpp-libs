//===- benchmark.cpp - Benchmark routine ------------------------*- C++ -*-===//
//
/// \file
/// Date and time library benchmark, just for fun
//
// Author:  zxh
// Date:    2021/10/11 22:08:37
//===----------------------------------------------------------------------===//

#include <inttypes.h>
#include <stdio.h>
#include <time_rfc3339.h>

using namespace time_rfc3339;

uint64_t test_count = std::nano::den;

void benchmark() {
  int64_t begin = Time::now().count();
  for (uint64_t i = 0; i < test_count; ++i)
    Time::now();
  int64_t end = Time::now().count();

  fprintf(stderr, "get current time cost time avg %" PRId64 "(ns)\n",
          (end - begin) / test_count);

  begin = Time::now().count();
  for (uint64_t i = 0; i < test_count; ++i)
    Time::now().format();
  end = Time::now().count();

  fprintf(stderr, "format current time cost time avg %" PRId64 "(ns)\n",
          (end - begin) / test_count);
}

int main() { benchmark(); }