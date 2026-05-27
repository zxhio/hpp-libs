#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <charconv>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <type_traits>
#include <unistd.h>

#include "slog_backend.hpp"

namespace slog {

inline Level g_level = Level::Info;

inline std::string_view level_name(Level lv) noexcept {
  switch (lv) {
  case Level::Debug:
    return "DEBUG";
  case Level::Info:
    return "INFO";
  case Level::Warn:
    return "WARN";
  case Level::Error:
    return "ERROR";
  }
  return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// Key / Field
// ---------------------------------------------------------------------------

template <std::size_t N>
struct Key {
  const char (&s)[N];
};

template <typename T, std::size_t N>
struct Field {
  Key<N> key;
  const T &value;
};

template <std::size_t N, typename T>
constexpr auto K(const char (&key)[N], const T &value) noexcept {
  return Field<T, N>{{key}, value};
}

template <typename T>
struct is_field : std::false_type {};

template <typename T, std::size_t N>
struct is_field<Field<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool is_field_v =
    is_field<std::remove_cv_t<std::remove_reference_t<T>>>::value;

// ---------------------------------------------------------------------------
// Fixed buffer
// ---------------------------------------------------------------------------

namespace detail {

template <std::size_t Capacity>
class FixedBuffer {
public:
  FixedBuffer() noexcept
      : begin_(data_), cur_(data_), end_(data_ + Capacity), truncated_(false) {}

  void write(const char *s, std::size_t n) noexcept {
    if (n == 0)
      return;

    const std::size_t avail = static_cast<std::size_t>(end_ - cur_);
    const std::size_t m = (n < avail) ? n : avail;

    if (m > 0) {
      std::memcpy(cur_, s, m);
      cur_ += m;
    }

    if (m != n)
      truncated_ = true;
  }

  template <std::size_t N>
  void write(const char (&s)[N]) noexcept {
    static_assert(N > 0, "invalid string literal");
    write(s, N - 1);
  }

  const char *data() const noexcept { return begin_; }
  std::size_t size() const noexcept { return static_cast<std::size_t>(cur_ - begin_); }
  bool truncated() const noexcept { return truncated_; }

private:
  char data_[Capacity];
  char *begin_;
  char *cur_;
  char *end_;
  bool truncated_;
};

using SmallBuffer = FixedBuffer<256>;
using MediumBuffer = FixedBuffer<1024>;
using LargeBuffer = FixedBuffer<4096>;
using DefaultBuffer = MediumBuffer;

} // namespace detail

// ---------------------------------------------------------------------------
// Value formatting
// ---------------------------------------------------------------------------

namespace detail {

inline bool needs_escape(std::string_view sv) noexcept {
  for (char c : sv) {
    if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t')
      return true;
  }
  return false;
}

template <typename BufferT>
inline void write_quoted(BufferT &buf, std::string_view sv) noexcept {
  buf.write("\"");

  if (!needs_escape(sv)) {
    buf.write(sv.data(), sv.size());
    buf.write("\"");
    return;
  }

  std::size_t start = 0;
  for (std::size_t i = 0; i < sv.size(); ++i) {
    const char c = sv[i];
    const bool esc =
        (c == '"') || (c == '\\') || (c == '\n') || (c == '\r') || (c == '\t');

    if (!esc)
      continue;

    if (i > start)
      buf.write(sv.data() + start, i - start);

    switch (c) {
    case '"':
      buf.write("\\\"");
      break;
    case '\\':
      buf.write("\\\\");
      break;
    case '\n':
      buf.write("\\n");
      break;
    case '\r':
      buf.write("\\r");
      break;
    case '\t':
      buf.write("\\t");
      break;
    }

    start = i + 1;
  }

  if (start < sv.size())
    buf.write(sv.data() + start, sv.size() - start);

  buf.write("\"");
}

template <typename BufferT>
inline void write_u64(BufferT &buf, std::uint64_t v) noexcept {
  char tmp[32];
  char *p = tmp + sizeof(tmp);

  do {
    *--p = static_cast<char>('0' + (v % 10));
    v /= 10;
  } while (v);

  buf.write(p, static_cast<std::size_t>(tmp + sizeof(tmp) - p));
}

template <typename BufferT>
inline void write_i64(BufferT &buf, std::int64_t v) noexcept {
  if (v < 0) {
    buf.write("-");
    write_u64(buf, static_cast<std::uint64_t>(-(v + 1)) + 1);
  } else {
    write_u64(buf, static_cast<std::uint64_t>(v));
  }
}

template <typename BufferT, typename T, typename Enable = void>
struct ValueWriter;

template <typename BufferT, typename T>
struct ValueWriter<
    BufferT, T,
    std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T> &&
                     !std::is_same_v<T, bool>>> {
  static void write(BufferT &buf, T v) noexcept {
    write_i64(buf, static_cast<std::int64_t>(v));
  }
};

template <typename BufferT, typename T>
struct ValueWriter<
    BufferT, T,
    std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> &&
                     !std::is_same_v<T, bool>>> {
  static void write(BufferT &buf, T v) noexcept {
    write_u64(buf, static_cast<std::uint64_t>(v));
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, bool> {
  static void write(BufferT &buf, bool v) noexcept {
    buf.write(v ? "true" : "false");
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, const char *> {
  static void write(BufferT &buf, const char *s) noexcept {
    write_quoted(buf, s ? std::string_view{s} : std::string_view{});
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, std::string_view> {
  static void write(BufferT &buf, std::string_view sv) noexcept {
    write_quoted(buf, sv);
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, char> {
  static void write(BufferT &buf, char c) noexcept {
    write_quoted(buf, std::string_view{&c, 1});
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, std::string> {
  static void write(BufferT &buf, const std::string &s) noexcept {
    write_quoted(buf, std::string_view{s});
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, float> {
  static void write(BufferT &buf, float v) noexcept {
    char tmp[64];
    auto [p, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
    if (ec == std::errc{})
      buf.write(tmp, static_cast<std::size_t>(p - tmp));
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, double> {
  static void write(BufferT &buf, double v) noexcept {
    char tmp[64];
    auto [p, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
    if (ec == std::errc{})
      buf.write(tmp, static_cast<std::size_t>(p - tmp));
  }
};

template <typename BufferT, typename T>
struct ValueWriter<BufferT, T, std::enable_if_t<std::is_enum_v<T>>> {
  static void write(BufferT &buf, T v) noexcept {
    write_i64(
        buf, static_cast<std::int64_t>(static_cast<std::underlying_type_t<T>>(v)));
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, const void *> {
  static void write(BufferT &buf, const void *p) noexcept {
    char tmp[32];
    int n = std::snprintf(tmp, sizeof(tmp), "%p", p);
    if (n > 0)
      buf.write(tmp, static_cast<std::size_t>(n));
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, void *> {
  static void write(BufferT &buf, void *p) noexcept {
    ValueWriter<BufferT, const void *>::write(buf, p);
  }
};

template <typename BufferT, std::size_t N>
inline void write_value(BufferT &buf, const char (&s)[N]) noexcept {
  write_quoted(buf, std::string_view{s, N - 1});
}

template <typename BufferT, typename T>
inline void write_value(BufferT &buf, const T &v) noexcept {
  ValueWriter<BufferT, std::remove_cv_t<std::remove_reference_t<T>>>::write(buf, v);
}

} // namespace detail

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

namespace detail {

template <typename BufferT>
inline void write_timestamp(BufferT &buf) noexcept {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto epoch_ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
  auto tt = system_clock::to_time_t(now);
  std::tm tm;
  ::localtime_r(&tt, &tm);

  char tmp[24];
  tmp[0] = static_cast<char>('0' + (tm.tm_year + 1900) / 1000);
  tmp[1] = static_cast<char>('0' + ((tm.tm_year + 1900) / 100) % 10);
  tmp[2] = static_cast<char>('0' + ((tm.tm_year + 1900) / 10) % 10);
  tmp[3] = static_cast<char>('0' + (tm.tm_year + 1900) % 10);
  tmp[4] = '-';
  tmp[5] = static_cast<char>('0' + (tm.tm_mon + 1) / 10);
  tmp[6] = static_cast<char>('0' + (tm.tm_mon + 1) % 10);
  tmp[7] = '-';
  tmp[8] = static_cast<char>('0' + tm.tm_mday / 10);
  tmp[9] = static_cast<char>('0' + tm.tm_mday % 10);
  tmp[10] = 'T';
  tmp[11] = static_cast<char>('0' + tm.tm_hour / 10);
  tmp[12] = static_cast<char>('0' + tm.tm_hour % 10);
  tmp[13] = ':';
  tmp[14] = static_cast<char>('0' + tm.tm_min / 10);
  tmp[15] = static_cast<char>('0' + tm.tm_min % 10);
  tmp[16] = ':';
  tmp[17] = static_cast<char>('0' + tm.tm_sec / 10);
  tmp[18] = static_cast<char>('0' + tm.tm_sec % 10);
  tmp[19] = '.';
  auto ms = static_cast<int>(epoch_ms % 1000);
  tmp[20] = static_cast<char>('0' + ms / 100);
  tmp[21] = static_cast<char>('0' + (ms / 10) % 10);
  tmp[22] = static_cast<char>('0' + ms % 10);

  buf.write(tmp, 23);
}

template <typename BufferT = DefaultBuffer>
class Record {
public:
  explicit Record(BufferT &buf) noexcept : buf_(buf) {}

  void begin(Level lv, std::string_view msg) noexcept {
    write_prefix(lv);
    write_quoted(buf_, msg);
  }

  template <typename T, std::size_t N>
  void write_field(const Field<T, N> &f) noexcept {
    buf_.write(" ");
    buf_.write(f.key.s, N - 1);
    buf_.write("=");
    write_value(buf_, f.value);
  }

  void end() noexcept {
    if (buf_.truncated())
      buf_.write(" truncated=true");
    buf_.write("\n");
  }

private:
  void write_prefix(Level lv) noexcept {
    write_timestamp(buf_);
    buf_.write(" [");
    const auto name = level_name(lv);
    buf_.write(name.data(), name.size());
    buf_.write("] msg=");
  }

  BufferT &buf_;
};

} // namespace detail

// ---------------------------------------------------------------------------
// Emitter dispatch
// ---------------------------------------------------------------------------

using EmitFn = void (*)(Level, const char *data, std::size_t n);

inline EmitFn g_emit = nullptr;

inline void set_emitter(EmitFn fn) noexcept { g_emit = fn; }

inline SyncFileEmitter *g_file_emitter = nullptr;

namespace detail {

inline void default_emit(Level, const char *data, std::size_t n) noexcept {
  (void)::write(STDERR_FILENO, data, n);
}

inline void file_emit(Level, const char *data, std::size_t n) noexcept {
  if (g_file_emitter)
    g_file_emitter->emit(Level{}, data, n);
}

} // namespace detail

inline void set_file_emitter(SyncFileEmitter &emitter) noexcept {
  g_file_emitter = &emitter;
  g_emit = detail::file_emit;
}

template <std::size_t QueueCapacity = 4096, std::size_t FlushBatch = 128,
          std::size_t MaxMsgSize = 1024>
inline void set_async_file_emitter(
    AsyncFileEmitter<QueueCapacity, FlushBatch, MaxMsgSize> &emitter) noexcept {
  static AsyncFileEmitter<QueueCapacity, FlushBatch, MaxMsgSize> *saved =
      nullptr;
  saved = &emitter;
  g_emit = [](Level, const char *data, std::size_t n) noexcept {
    saved->emit(Level{}, data, n);
  };
}

// ---------------------------------------------------------------------------
// Core log function
// ---------------------------------------------------------------------------

template <typename BufferT = detail::DefaultBuffer, std::size_t N,
          typename... Fields>
inline void log(Level lv, const char (&msg)[N],
                const Fields &...fields) noexcept {
  static_assert((is_field_v<Fields> && ...),
                "all args after msg must be K(...)");

  if (lv < g_level)
    return;

  BufferT buf;
  detail::Record<BufferT> rec{buf};

  rec.begin(lv, std::string_view{msg, N - 1});
  (rec.write_field(fields), ...);
  rec.end();

  if (g_emit)
    g_emit(lv, buf.data(), buf.size());
  else
    detail::default_emit(lv, buf.data(), buf.size());
}

} // namespace slog

#define LOG_DEBUG(msg, ...)                                                    \
  ::slog::log(::slog::Level::Debug, (msg), ##__VA_ARGS__)

#define LOG_INFO(msg, ...)                                                     \
  ::slog::log(::slog::Level::Info, (msg), ##__VA_ARGS__)

#define LOG_WARN(msg, ...)                                                     \
  ::slog::log(::slog::Level::Warn, (msg), ##__VA_ARGS__)

#define LOG_ERROR(msg, ...)                                                    \
  ::slog::log(::slog::Level::Error, (msg), ##__VA_ARGS__)
