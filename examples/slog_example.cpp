#include <slog/slog.hpp>
#include <thread>

enum class Status : int { Ok = 0, Error = 1 };

int main() {
  // Default: stderr
  slog::log(slog::Level::Info, "hello");
  slog::log(slog::Level::Info, "listening", slog::K("port", 8080));
  slog::log(slog::Level::Info, "connected", slog::K("addr", "127.0.0.1"),
            slog::K("port", 443));

  slog::log(slog::Level::Info, "types",
            slog::K("char", 'A'),
            slog::K("float", 3.14f),
            slog::K("double", 2.718281828),
            slog::K("str", std::string("hello")),
            slog::K("status", Status::Ok),
            slog::K("ptr", (void *)0xDEADBEEF));

  LOG_INFO("macro style");
  LOG_WARN("something wrong", slog::K("code", -1));
  LOG_ERROR("failed", slog::K("file", "/tmp/test"),
            slog::K("err", "no such file"));

  // SyncFileEmitter: writes to file with size-based rotation
  slog::SyncFileEmitter sync_emitter("/tmp/slog_sync.log");
  slog::set_file_emitter(sync_emitter);
  LOG_INFO("sync file emitter");
  LOG_WARN("rotation demo", slog::K("max_size", 16 * 1024 * 1024));

  // AsyncFileEmitter: background thread + SPSC queue
  slog::AsyncFileEmitter<> async_emitter("/tmp/slog_async.log");
  slog::set_async_file_emitter(async_emitter);
  for (int i = 0; i < 100; ++i)
    LOG_INFO("async", slog::K("i", i));
  async_emitter.stop();
}
