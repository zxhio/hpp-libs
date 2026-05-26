#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <unistd.h>

namespace slog {

enum class Level : std::uint8_t {
  Debug,
  Info,
  Warn,
  Error,
};

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

struct StderrEmitter {
  static void emit(Level, const char *data, std::size_t n) noexcept {
    (void)::write(STDERR_FILENO, data, n);
  }
};

struct NullEmitter {
  static void emit(Level, const char *, std::size_t) noexcept {}
};

namespace detail {

template <std::size_t Capacity>
class FixedBuffer {
public:
  FixedBuffer() noexcept
      : begin_(data_), cur_(data_), end_(data_ + Capacity), truncated_(false) {}

  void write(const char *s, std::size_t n) noexcept {
    if (n == 0) {
      return;
    }

    const std::size_t avail = static_cast<std::size_t>(end_ - cur_);
    const std::size_t m = (n < avail) ? n : avail;

    if (m > 0) {
      std::memcpy(cur_, s, m);
      cur_ += m;
    }

    if (m != n) {
      truncated_ = true;
    }
  }

  template <std::size_t N>
  void write(const char (&s)[N]) noexcept {
    static_assert(N > 0, "invalid string literal");
    write(s, N - 1);
  }

  const char *data() const noexcept { return begin_; }

  std::size_t size() const noexcept {
    return static_cast<std::size_t>(cur_ - begin_);
  }

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

inline bool needs_escape(std::string_view sv) noexcept {
  for (char c : sv) {
    if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
      return true;
    }
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

    if (!esc) {
      continue;
    }

    if (i > start) {
      buf.write(sv.data() + start, i - start);
    }

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

  if (start < sv.size()) {
    buf.write(sv.data() + start, sv.size() - start);
  }

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
    int n = std::snprintf(tmp, sizeof(tmp), "%g", v);
    if (n > 0) {
      buf.write(tmp, static_cast<std::size_t>(n));
    }
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, double> {
  static void write(BufferT &buf, double v) noexcept {
    char tmp[64];
    int n = std::snprintf(tmp, sizeof(tmp), "%g", v);
    if (n > 0) {
      buf.write(tmp, static_cast<std::size_t>(n));
    }
  }
};

template <typename BufferT, typename T>
struct ValueWriter<
    BufferT, T,
    std::enable_if_t<std::is_enum_v<T>>> {
  static void write(BufferT &buf, T v) noexcept {
    write_i64(buf, static_cast<std::int64_t>(static_cast<std::underlying_type_t<T>>(v)));
  }
};

template <typename BufferT>
struct ValueWriter<BufferT, const void *> {
  static void write(BufferT &buf, const void *p) noexcept {
    char tmp[32];
    int n = std::snprintf(tmp, sizeof(tmp), "%p", p);
    if (n > 0) {
      buf.write(tmp, static_cast<std::size_t>(n));
    }
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
  ValueWriter<BufferT, std::remove_cv_t<std::remove_reference_t<T>>>::write(buf,
                                                                            v);
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
    if (buf_.truncated()) {
      buf_.write(" truncated=true");
    }
    buf_.write("\n");
  }

private:
  void write_prefix(Level lv) noexcept {
    buf_.write("[");
    const auto name = level_name(lv);
    buf_.write(name.data(), name.size());
    buf_.write("] msg=");
  }

  BufferT &buf_;
};

} // namespace detail

template <typename Emitter = StderrEmitter, typename BufferT = detail::DefaultBuffer,
          std::size_t N, typename... Fields>
inline void log(Level lv, const char (&msg)[N],
                const Fields &...fields) noexcept {
  static_assert((is_field_v<Fields> && ...),
                "all args after msg must be K(...)");

  BufferT buf;
  detail::Record<BufferT> rec{buf};

  rec.begin(lv, std::string_view{msg, N - 1});
  (rec.write_field(fields), ...);
  rec.end();

  Emitter::emit(lv, buf.data(), buf.size());
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
