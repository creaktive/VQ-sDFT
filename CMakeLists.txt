cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(vqsdft C CXX)
pico_sdk_init()

add_executable(benchmark benchmark.c 12tet.c vqsdft.c)
target_compile_definitions(benchmark PRIVATE TARGET_PICO=1)
target_link_libraries(benchmark pico_stdlib)

pico_enable_stdio_uart(benchmark 0)
pico_enable_stdio_usb(benchmark 1)

pico_add_extra_outputs(benchmark)
