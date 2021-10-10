#pragma once

#include <inttypes.h>
#include <time.h>

#include <chrono>
#include <string>

namespace time_rfc3339 {

// The digits table is used to look up for number within 100.
// Each two character corresponds to one digit and ten digits.
static constexpr char DigitsTable[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0',
    '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2',
    '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3',
    '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5',
    '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6',
    '7', '6', '8', '6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8', '0', '8', '1', '8',
    '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9',
    '7', '9', '8', '9', '9'};

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatUInt(char *to, T v, size_t fmtlen) {
  char buf[24];
  char *p = buf;
  size_t length = 0;

  while (v >= 100) {
    const unsigned idx = (v % 100) << 1;
    v /= 100;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  if (v < 10) {
    *p++ = v + '0';
  } else {
    const unsigned idx = v << 1;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  length = p - buf;

  for (size_t i = length; i < fmtlen; i++)
    *to++ = '0';

  size_t minlen = std::min(length, fmtlen);
  for (size_t i = 0; i < minlen; ++i)
    *to++ = *--p;

  return fmtlen;
}

static inline size_t formatChar(char *to, char c) {
  *to = c;
  return sizeof(char);
}

class Time {
public:
  using TimePoint = std::chrono::time_point<std::chrono::system_clock,
                                            std::chrono::nanoseconds>;
  Time() = delete;
  Time(TimePoint tp) : tp_(tp) {}

  Time(const Time &) = default;
  Time &operator=(const Time &) = default;

  /// Current time.
  static Time now() { return Time(std::chrono::system_clock::now()); }

  /// Year (4 digits, e.g. 1996).
  int year() const { return toTm().tm_year + 1900; }

  /// Month of then year, in the range [1, 12].
  int month() const { return toTm().tm_mon + 1; }

  /// Day of the month, in the range [1, 28/29/30/31].
  int day() const { return toTm().tm_mday; }

  /// Day of the week, in the range [1, 7].
  int weekday() const { return toTm().tm_wday; }

  /// Hour within day, in the range [0, 23].
  int hour() const { return toTm().tm_hour; }

  /// Minute offset within the hour, in the range [0, 59].
  int minute() const { return toTm().tm_min; }

  /// Second offset within the minute, in the range [0, 59].
  int second() const { return toTm().tm_sec; }

  /// Nanosecond offset within the second, in the range [0, 999999999].
  int nanosecond() const { return static_cast<int>(count() % std::nano::den); }

  /// Count of nanosecond elapsed since 1970-01-01T00:00:00Z .
  int64_t count() const { return tp_.time_since_epoch().count(); }

  /// Timezone name and offset in seconds east of UTC.
  std::pair<long int, std::string> timezone() const {
    static thread_local long int t_off = std::numeric_limits<long int>::min();
    static thread_local char t_zone[8];

    if (t_off == std::numeric_limits<long int>::min()) {
      struct tm t;
      time_t c = std::chrono::system_clock::to_time_t(tp_);
      localtime_r(&c, &t);
      t_off = t.tm_gmtoff;
      std::copy(t.tm_zone,
                t.tm_zone + std::char_traits<char>::length(t.tm_zone), t_zone);
    }

    return std::make_pair(t_off, t_zone);
  }

  /// Standard date-time format using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T23:20:50.52Z
  ///   2021-10-10T16:39:57-08:00
  ///   2021-10-10T05:46:58:343241947+08:00
  std::string format() const {
    char datetime[40];
    char *p = datetime;
    struct tm t = toTm();

    p += formatDate(p, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    p += formatChar(p, 'T');
    p += formatTime(p, t.tm_hour, t.tm_min, t.tm_sec, nanosecond());

    return std::string(datetime, p - datetime);
  }

private:
  struct tm toTm() const {
    struct tm t;
    time_t c = std::chrono::system_clock::to_time_t(tp_);

    // gmtime_r() is 150% faster than localtime_r() because it doesn't need to
    // calculate the time zone. For a more faster implementation refer to
    // musl-libc, which is 10% faster than the glibc gmtime_r().
    //   __secs_to_tm(c, &t);
    // ref: https://git.musl-libc.org/cgit/musl/tree/src/time/__secs_to_tm.c#n11
    gmtime_r(&c, &t);

    return t;
  }

  size_t formatDate(char *to, int year, int mon, int mday) const {
    char *p = to;
    p += formatUInt(p, year, 4);
    p += formatChar(p, '-');
    p += formatUInt(p, mon, 2);
    p += formatChar(p, '-');
    p += formatUInt(p, mday, 2);
    return p - to;
  }

  size_t formatTime(char *to, int hour, int min, int sec,
                    int64_t secfrac) const {
    char *p = to;
    p += formatPartialTime(p, hour, min, sec, secfrac);
    p += formatTimeOff(p);
    return p - to;
  }

  size_t formatPartialTime(char *to, int hour, int min, int sec,
                           int64_t secfrac) const {
    char *p = to;
    p += formatUInt(p, hour, 2);
    p += formatChar(p, ':');
    p += formatUInt(p, min, 2);
    p += formatChar(p, ':');
    p += formatUInt(p, sec, 2);
    p += formatSecfrac(p, secfrac, 9);
    return p - to;
  }

  size_t formatSecfrac(char *to, int64_t frac, size_t fmtlen) const {
    if (frac == 0)
      return 0;

    char *p = to;
    p += formatChar(p, '.');
    p += formatUInt(p, frac, fmtlen);
    return p - to;
  }

  size_t formatTimeOff(char *to) const {
    long int off = timezone().first;
    char *p = to;

    if (off == 0) {
      p += formatChar(p, 'Z');
    } else {
      p += formatChar(p, off < 0 ? '-' : '+');
      p += formatUInt(p, off / 3600, 2);
      p += formatChar(p, ':');
      p += formatUInt(p, off % 3600, 2);
    }

    return p - to;
  }

  TimePoint tp_;
};

} // namespace time_rfc3339