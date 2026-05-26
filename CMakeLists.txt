cmake_minimum_required(VERSION 3.10)
project(hpp-libs LANGUAGES CXX)

add_library(hpp-libs INTERFACE)
add_library(hpp::libs ALIAS hpp-libs)

target_compile_features(hpp-libs INTERFACE cxx_std_17)
target_include_directories(hpp-libs INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

option(HPP_LIBS_BUILD_EXAMPLES "Build examples" OFF)
if(HPP_LIBS_BUILD_EXAMPLES)
  add_executable(slog_example examples/slog_example.cpp)
  target_link_libraries(slog_example PRIVATE hpp::libs)

  add_executable(slog_benchmark examples/slog_benchmark.cpp)
  target_link_libraries(slog_benchmark PRIVATE hpp::libs)
  target_compile_options(slog_benchmark PRIVATE -O2)
endif()
