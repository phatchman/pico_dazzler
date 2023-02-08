cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD vgaboard)
set(BOARD pico_sdk)
set(FAMILY rp2040)
include(tinyusb/hw/bsp/family_support.cmake)
include(pico-sdk/external/pico_sdk_import.cmake)
include(pico-extras/external/pico_extras_import.cmake)
project(pico_dazzler_project C CXX)
#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(~/src/pico_dazzler)

#pico_sdk_init()

add_executable(pico_dazzler
  main.c
  usb_joystick.c
  parse_descriptor.c
 )
target_compile_definitions(pico_dazzler PUBLIC -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
  -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8)

family_initialize_project(pico_dazzler_project ${CMAKE_CURRENT_LIST_DIR})
family_configure_target(pico_dazzler)
pico_enable_stdio_uart(pico_dazzler 1)
pico_enable_stdio_usb(pico_dazzler 0)
pico_add_extra_outputs(pico_dazzler)
target_link_libraries(pico_dazzler PRIVATE
  pico_stdlib
  pico_scanvideo_dpi
  pico_multicore
  tinyusb_host)
