#include "../time_rfc3339.h"
#include <bits/stdint-uintn.h>
#include <iostream>

// using namespace time_rfc3339;

void test_delay() {
  uint64_t test_count = 10000000;
  int64_t begin = time_rfc3339::Time::now().count();

  for (uint64_t i = 0; i < test_count; ++i)
    time_rfc3339::Time::now().format();

  int64_t end = time_rfc3339::Time::now().count();

  std::cout << (end - begin) / test_count << std::endl;
}

int main() {
  test_delay();

  time_rfc3339::Time now = time_rfc3339::Time::now();
  std::cout << now.format() << std::endl;
}