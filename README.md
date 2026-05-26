# hpp-libs

Header-only C++ utility libraries. Each component lives in its own directory under `include/` with an isolated namespace.

## Components

| Component | Path | Namespace | Description |
|-----------|------|-----------|-------------|
| slog | `include/slog/` | `slog` | Lightweight structured logging |

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

int main() {
    slog::g_level = slog::Level::Info;

    slog::log(slog::Level::Info, "server started");
    LOG_INFO("server started");
    LOG_INFO("listening", slog::K("port", 8080), slog::K("addr", "0.0.0.0"));
}
```

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

Measured on Intel i7 (5M iterations, `-O2`):

```
                               Emitter    no field   1 field    2 fields   3 fields
                               -------    --------   -------    --------   --------
StderrEmitter (real I/O)       stderr       12.6 ns   18.5 ns   25.2 ns    33.9 ns
NullEmitter (format only)      null          4.4 ns   11.4 ns   18.9 ns    26.1 ns
NullEmitter (level filtered)   null          0.2 ns    0.4 ns    0.4 ns     0.6 ns
```

## License

MIT
