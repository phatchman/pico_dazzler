cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD vgaboard)
set(BOARD pico_sdk)
set(FAMILY rp2040)
set(DAZ_AUDIO false)

set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/pico-sdk)
set(PICO_EXTRAS_PATH ${CMAKE_SOURCE_DIR}/pico-extras)

include(add_git_submodule.cmake)
add_git_submodule(tinyusb_due)
add_git_submodule(pico-sdk)
add_git_submodule_recurse(pico-extras)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(pico_dazzler_project C CXX)


include(tinyusb_due/hw/bsp/family_support.cmake)

#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(${CMAKE_SOURCE_DIR})

#pico_sdk_init()

family_initialize_project(pico_dazzler_project ${CMAKE_CURRENT_LIST_DIR})

if (NOT ${DAZ_AUDIO})
  add_executable(pico_dazzler
    main.c
    usb_joystick.c
    parse_descriptor.c
    daz_audio.c
  )
  
target_compile_definitions(pico_dazzler PUBLIC 
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=128
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
    PICO_AUDIO_I2S_DMA_IRQ=1
    PICO_AUDIO_I2S_PIO=1
    DEBUG_MAIN=0
    TRACE_MAIN=0
    DEBUG_AUDIO=0
    TRACE_AUDIO=0
    DEBUG_DESCRIPTOR=0
    TRACE_DESCRIPTOR=0
    DEBUG_JOYSTICK=0
    TRACE_JOYSTICK=0
  )

  family_configure_target(pico_dazzler)
  pico_enable_stdio_uart(pico_dazzler 1)
  pico_enable_stdio_usb(pico_dazzler 0)
  pico_add_extra_outputs(pico_dazzler)
  target_link_libraries(pico_dazzler PRIVATE
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    pico_audio_i2s
    tinyusb_host)
else()

  add_executable(daz_audio
  daz_audio.c
  )

  pico_enable_stdio_uart(daz_audio 1)
  pico_enable_stdio_usb(daz_audio 0)
  pico_add_extra_outputs(daz_audio)

  target_compile_definitions(daz_audio PUBLIC 
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=128
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
    USE_AUDIO_I2S=1
    PICO_AUDIO_I2S_DMA_IRQ=1
    PICO_AUDIO_I2S_PIO=0
    DAZAUDIO_STANDALONE=1
  )


  target_link_libraries(daz_audio PRIVATE
    pico_stdlib
    pico_audio_i2s
  )
endif()
