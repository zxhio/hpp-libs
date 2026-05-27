#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "spsc.hpp"

namespace slog {

enum class Level : std::uint8_t {
  Debug,
  Info,
  Warn,
  Error,
};

// ---------------------------------------------------------------------------
// File I/O helpers
// ---------------------------------------------------------------------------

namespace detail {

inline void write_fd(int fd, const char *data, std::size_t n) noexcept {
  while (n > 0) {
    auto w = ::write(fd, data, n);
    if (w <= 0)
      break;
    data += static_cast<std::size_t>(w);
    n -= static_cast<std::size_t>(w);
  }
}

inline int open_log_file(const char *path) noexcept {
  return ::open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
}

inline std::size_t file_size(int fd) noexcept {
  struct stat st;
  if (::fstat(fd, &st) != 0)
    return 0;
  return static_cast<std::size_t>(st.st_size);
}

inline void rotate_file(const char *base, int fd) noexcept {
  ::close(fd);

  auto now = std::chrono::system_clock::now();
  auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm;
  ::localtime_r(&tt, &tm);

  char path[512];
  std::size_t len = std::strlen(base);
  if (len + 24 >= sizeof(path))
    return;

  std::memcpy(path, base, len);
  char *p = path + len;
  *p++ = '.';

  auto y = tm.tm_year + 1900;
  *p++ = static_cast<char>('0' + y / 1000);
  *p++ = static_cast<char>('0' + (y / 100) % 10);
  *p++ = static_cast<char>('0' + (y / 10) % 10);
  *p++ = static_cast<char>('0' + y % 10);
  auto m = tm.tm_mon + 1;
  *p++ = static_cast<char>('0' + m / 10);
  *p++ = static_cast<char>('0' + m % 10);
  auto d = tm.tm_mday;
  *p++ = static_cast<char>('0' + d / 10);
  *p++ = static_cast<char>('0' + d % 10);
  *p++ = 'T';
  *p++ = static_cast<char>('0' + tm.tm_hour / 10);
  *p++ = static_cast<char>('0' + tm.tm_hour % 10);
  *p++ = static_cast<char>('0' + tm.tm_min / 10);
  *p++ = static_cast<char>('0' + tm.tm_min % 10);
  *p++ = static_cast<char>('0' + tm.tm_sec / 10);
  *p++ = static_cast<char>('0' + tm.tm_sec % 10);
  std::memcpy(p, ".log", 4);
  p += 4;
  *p = '\0';

  ::rename(base, path);
}

} // namespace detail

// ---------------------------------------------------------------------------
// SyncFileEmitter
// ---------------------------------------------------------------------------

class SyncFileEmitter {
public:
  SyncFileEmitter(const char *path, std::size_t max_size = 16 * 1024 * 1024)
      : path_(path), fd_(detail::open_log_file(path)),
        cur_size_(detail::file_size(fd_)), max_size_(max_size) {}

  ~SyncFileEmitter() {
    if (fd_ >= 0)
      ::close(fd_);
  }

  SyncFileEmitter(const SyncFileEmitter &) = delete;
  SyncFileEmitter &operator=(const SyncFileEmitter &) = delete;

  void emit(Level, const char *data, std::size_t n) noexcept {
    if (fd_ < 0)
      return;

    if (cur_size_ + n > max_size_) {
      detail::rotate_file(path_, fd_);
      fd_ = detail::open_log_file(path_);
      cur_size_ = 0;
    }

    detail::write_fd(fd_, data, n);
    cur_size_ += n;
  }

private:
  const char *path_;
  int fd_;
  std::size_t cur_size_;
  std::size_t max_size_;
};

// ---------------------------------------------------------------------------
// AsyncFileEmitter
// ---------------------------------------------------------------------------

template <std::size_t QueueCapacity = 4096, std::size_t FlushBatch = 128,
          std::size_t MaxMsgSize = 1024>
class AsyncFileEmitter {
public:
  struct Message {
    char data[MaxMsgSize];
    std::size_t size;
  };

  explicit AsyncFileEmitter(const char *path,
                            std::size_t max_size = 16 * 1024 * 1024)
      : file_(path, max_size), running_(true) {
    worker_ = std::thread([this] { run(); });
  }

  ~AsyncFileEmitter() { stop(); }

  AsyncFileEmitter(const AsyncFileEmitter &) = delete;
  AsyncFileEmitter &operator=(const AsyncFileEmitter &) = delete;

  void emit(Level, const char *data, std::size_t n) noexcept {
    if (n > MaxMsgSize)
      n = MaxMsgSize;
    Message msg;
    std::memcpy(msg.data, data, n);
    msg.size = n;
    queue_.try_push(msg);
  }

  void stop() noexcept {
    if (!running_.exchange(false))
      return;
    if (worker_.joinable())
      worker_.join();
    Message msg;
    while (queue_.try_pop(msg))
      file_.emit(Level{}, msg.data, msg.size);
  }

private:
  void run() noexcept {
    Message msg;
    std::size_t batch = 0;
    while (running_.load(std::memory_order_relaxed)) {
      if (queue_.try_pop(msg)) {
        file_.emit(Level{}, msg.data, msg.size);
        if (++batch >= FlushBatch) {
          batch = 0;
          std::this_thread::yield();
        }
      } else {
        std::this_thread::yield();
      }
    }
  }

  SyncFileEmitter file_;
  SpscQueue<Message, QueueCapacity> queue_;
  std::atomic<bool> running_;
  std::thread worker_;
};

} // namespace slog
