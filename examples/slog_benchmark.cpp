#include <chrono>
#include <cstdio>
#include <slog/slog.hpp>

template <typename Fn>
double bench(const char *name, int n, Fn &&fn) {
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < n; ++i)
    fn(i);
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  std::fprintf(stdout, "%-35s %8d iters  %8.2f ms  %8.1f ns/op\n", name, n,
               ms, ms * 1e6 / n);
  return ms;
}

int main() {
  constexpr int N = 5000000;

  // ── No-op emitter (baseline) ────────────────────────────────────────

  std::fprintf(stdout, "=== no-op emitter ===\n");
  slog::g_level = slog::Level::Debug;
  slog::set_emitter([](slog::Level, const char *, std::size_t) noexcept {});

  bench("log (no field)", N, [](int) {
    slog::log(slog::Level::Info, "hello world");
  });

  bench("log (1 field)", N, [](int i) {
    slog::log(slog::Level::Info, "request", slog::K("id", i));
  });

  bench("log (2 fields)", N, [](int i) {
    slog::log(slog::Level::Info, "request", slog::K("id", i),
              slog::K("addr", "127.0.0.1"));
  });

  bench("log (3 fields)", N, [](int i) {
    slog::log(slog::Level::Info, "request", slog::K("id", i),
              slog::K("addr", "127.0.0.1"), slog::K("port", 8080));
  });

  bench("log (float)", N, [](int i) {
    slog::log(slog::Level::Info, "metric", slog::K("value", 3.14f));
  });

  bench("log (double)", N, [](int i) {
    slog::log(slog::Level::Info, "metric", slog::K("value", 3.14159265));
  });

  bench("log (mixed int+float)", N, [](int i) {
    slog::log(slog::Level::Info, "request", slog::K("id", i),
              slog::K("latency", 12.5), slog::K("code", 200));
  });

  // ── SyncFileEmitter ─────────────────────────────────────────────────

  std::fprintf(stdout, "\n=== SyncFileEmitter ===\n");
  {
    slog::SyncFileEmitter sync("/tmp/bench_sync.log");
    slog::set_file_emitter(sync);

    bench("log (no field)", N, [](int) {
      slog::log(slog::Level::Info, "hello world");
    });

    bench("log (1 field)", N, [](int i) {
      slog::log(slog::Level::Info, "request", slog::K("id", i));
    });

    bench("log (3 fields)", N, [](int i) {
      slog::log(slog::Level::Info, "request", slog::K("id", i),
                slog::K("addr", "127.0.0.1"), slog::K("port", 8080));
    });
  }

  // ── AsyncFileEmitter ────────────────────────────────────────────────

  std::fprintf(stdout, "\n=== AsyncFileEmitter ===\n");
  {
    slog::AsyncFileEmitter<> async("/tmp/bench_async.log");
    slog::set_async_file_emitter(async);

    bench("log (no field)", N, [](int) {
      slog::log(slog::Level::Info, "hello world");
    });

    bench("log (1 field)", N, [](int i) {
      slog::log(slog::Level::Info, "request", slog::K("id", i));
    });

    bench("log (3 fields)", N, [](int i) {
      slog::log(slog::Level::Info, "request", slog::K("id", i),
                slog::K("addr", "127.0.0.1"), slog::K("port", 8080));
    });

    async.stop();
  }

  // ── SPSC queue raw throughput ────────────────────────────────────────

  std::fprintf(stdout, "\n=== SPSC queue (raw push/pop) ===\n");
  {
    slog::SpscQueue<int, 4096> q;

    bench("try_push", N, [&](int i) { q.try_push(i); });

    int v;
    bench("try_pop", N, [&](int) { q.try_pop(v); });

    bench("push+pop", N, [&](int i) {
      q.try_push(i);
      q.try_pop(v);
    });
  }

  // ── Level filtered (baseline) ───────────────────────────────────────

  std::fprintf(stdout, "\n=== level filtered (g_level=Error, Info skipped) ===\n");
  slog::g_level = slog::Level::Error;
  slog::set_emitter([](slog::Level, const char *, std::size_t) noexcept {});

  bench("log (no field)", N, [](int) {
    slog::log(slog::Level::Info, "hello world");
  });

  bench("log (1 field)", N, [](int i) {
    slog::log(slog::Level::Info, "request", slog::K("id", i));
  });

  bench("log (3 fields)", N, [](int i) {
    slog::log(slog::Level::Info, "request", slog::K("id", i),
              slog::K("addr", "127.0.0.1"), slog::K("port", 8080));
  });
}
