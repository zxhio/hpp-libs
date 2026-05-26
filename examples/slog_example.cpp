#include <slog/slog.hpp>

int main() {
  slog::log(slog::Level::Info, "hello");
  slog::log(slog::Level::Info, "listening", slog::K("port", 8080));
  slog::log(slog::Level::Info, "connected", slog::K("addr", "127.0.0.1"),
            slog::K("port", 443));

  LOG_INFO("macro style");
  LOG_WARN("something wrong", slog::K("code", -1));
  LOG_ERROR("failed", slog::K("file", "/tmp/test"), slog::K("err", "no such file"));
}
