# hpp-libs

Header-only C++ utility libraries. Each component lives in its own directory under `include/` with an isolated namespace.

## Components

| Component | Path | Namespace | Description |
|-----------|------|-----------|-------------|
| slog | `include/slog/` | `slog` | Lightweight structured logging |
| time_rfc3339 | `include/time_rfc3339/` | `time_rfc3339` | RFC3339 date-time formatting |

## Requirements

- C++17 or later

## Integration

### CMake (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(hpp-libs
  GIT_REPOSITORY https://github.com/<user>/hpp-libs.git
  GIT_TAG main
)
FetchContent_MakeAvailable(hpp-libs)

target_link_libraries(your_target PRIVATE hpp::libs)
```

### Usage

```cpp
#include <slog/slog.hpp>
#include <time_rfc3339/time_rfc3339.h>

int main() {
    slog::g_level = slog::Level::Info;

    slog::log(slog::Level::Info, "server started");
    LOG_INFO("server started");
    LOG_INFO("listening", slog::K("port", 8080), slog::K("addr", "0.0.0.0"));
}
```

RFC3339 formatting:

```cpp
auto timestamp = time_rfc3339::Time::now().formatNano();
```

The time component is header-only and supports C++11 APIs while being
available through the C++17 library target.

### Value Types

Supports integers, floats, bool, char, string, enum, and pointer types:

```cpp
LOG_INFO("request",
    slog::K("id", 42),
    slog::K("rate", 3.14),
    slog::K("ok", true),
    slog::K("tag", 'A'),
    slog::K("host", std::string("localhost")),
    slog::K("status", Status::Ok),
    slog::K("ptr", (void *)0xDEADBEEF));
```

### Custom Emitter

```cpp
struct FileEmitter {
  static void emit(slog::Level, const char *data, std::size_t n) noexcept {
    fwrite(data, 1, n, fp);
  }
};

slog::log<FileEmitter>(slog::Level::Info, "written to file");
```

### Level Filtering

```cpp
slog::g_level = slog::Level::Warn;
LOG_INFO("skipped");   // returns early, ~0.2 ns
LOG_ERROR("processed"); // formats and emits
```

## Benchmark

Measured on Intel i7-12700, `-O2`, 5M iterations:

```
--- integer fields ---
log (no field)                   4.5 ns/op
log (1 field)                   12.0 ns/op
log (2 fields)                  19.2 ns/op
log (3 fields)                  26.5 ns/op

--- float/double fields (via std::to_chars) ---
log (float)                    27.6 ns/op
log (double)                   35.8 ns/op
log (mixed int+float)          63.3 ns/op

--- level filtered (early return) ---
log (no field)                   0.2 ns/op
log (1 field)                    0.5 ns/op
log (2 fields)                  0.5 ns/op
log (3 fields)                  0.7 ns/op
```

## License

MIT

## Component Documentation

- [time_rfc3339](docs/time_rfc3339.md)
