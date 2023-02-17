cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD vgaboard)
set(BOARD pico_sdk)
set(FAMILY rp2040)
set(DAZ_AUDIO false)
include(tinyusb/hw/bsp/family_support.cmake)
include(pico-sdk/external/pico_sdk_import.cmake)
include(pico-extras/external/pico_extras_import.cmake)
project(pico_dazzler_project C CXX)
#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(~/src/pico_dazzler)

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
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
    USE_AUDIO_I2S=1
    PICO_AUDIO_I2S_DMA_IRQ=1
    PICO_AUDIO_I2S_PIO=1
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
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
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