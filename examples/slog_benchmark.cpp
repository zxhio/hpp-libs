#include <chrono>
#include <cstdio>
#include <slog/slog.hpp>

template <typename Fn>
double bench(const char *name, int n, Fn &&fn) {
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < n; ++i) {
    fn(i);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  std::fprintf(stdout, "%-30s %8d iters  %8.2f ms  %8.1f ns/op\n", name, n,
               ms, ms * 1e6 / n);
  return ms;
}

int main() {
  constexpr int N = 5000000;

  bench("log (no field)", N, [](int) {
    slog::log<slog::NullEmitter>(slog::Level::Info, "hello world");
  });

  bench("log (1 field)", N, [](int i) {
    slog::log<slog::NullEmitter>(slog::Level::Info, "request",
                                 slog::K("id", i));
  });

  bench("log (2 fields)", N, [](int i) {
    slog::log<slog::NullEmitter>(slog::Level::Info, "request",
                                 slog::K("id", i),
                                 slog::K("addr", "127.0.0.1"));
  });

  bench("log (3 fields)", N, [](int i) {
    slog::log<slog::NullEmitter>(slog::Level::Info, "request",
                                 slog::K("id", i),
                                 slog::K("addr", "127.0.0.1"),
                                 slog::K("port", 8080));
  });
}
