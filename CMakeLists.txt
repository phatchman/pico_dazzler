cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD vgaboard)
include(pico-sdk/external/pico_sdk_import.cmake)
include(pico-extras/external/pico_extras_import.cmake)
project(pico_dazzler_project C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_dazzler
  main.c
 )
target_compile_definitions(pico_dazzler PUBLIC -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
  -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8)

pico_enable_stdio_uart(pico_dazzler 0)
pico_enable_stdio_usb(pico_dazzler 1)
pico_add_extra_outputs(pico_dazzler)
target_link_libraries(pico_dazzler PRIVATE
  pico_stdlib
  pico_scanvideo_dpi
  pico_multicore)
