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
    slog::log(slog::Level::Info, "server started");
    LOG_INFO("server started");
    LOG_INFO("listening", slog::K("port", 8080));
}
```

## License

MIT
