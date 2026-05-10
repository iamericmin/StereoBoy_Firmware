##########Button Demo CMake##########

# # Generated Cmake Pico project file

# cmake_minimum_required(VERSION 3.13)

# set(CMAKE_C_STANDARD 11)
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# # Initialise pico_sdk from installed location
# # (note this can come from environment, CMake cache etc)

# # == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
# if(WIN32)
#     set(USERHOME $ENV{USERPROFILE})
# else()
#     set(USERHOME $ENV{HOME})
# endif()
# set(sdkVersion 2.2.0)
# set(toolchainVersion 14_2_Rel1)
# set(picotoolVersion 2.2.0-a4)
# set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# if (EXISTS ${picoVscode})
#     include(${picoVscode})
# endif()
# # ====================================================================================
# set(PICO_BOARD pico2 CACHE STRING "Board type")

# # Pull in Raspberry Pi Pico SDK (must be before project)
# include(pico_sdk_import.cmake)

# project(StereoBoy_FW C CXX ASM)

# # Initialise the Raspberry Pi Pico SDK
# pico_sdk_init()

# # --- ISOLATED LED TEST TARGET ---
# # We keep the name StereoBoy_FW so VS Code flashes it automatically, 
# # but we only give it the demo file and the LED driver.
# add_executable(StereoBoy_FW
#     src/buttons_demo.c 
#     lib/buttons/buttons.c
# )

# pico_set_program_name(StereoBoy_FW "StereoBoy_FW")
# pico_set_program_version(StereoBoy_FW "0.1")

# # Commented out PIO headers so they don't break the build
# # pico_generate_pio_header(StereoBoy_FW ${CMAKE_CURRENT_LIST_DIR}/pio/blink.pio)
# # pico_generate_pio_header(StereoBoy_FW ${CMAKE_CURRENT_LIST_DIR}/pio/display.pio)
# # pico_generate_pio_header(StereoBoy_FW ${CMAKE_CURRENT_LIST_DIR}/pio/main.pio)

# # Modify the below lines to enable/disable output over UART/USB
# pico_enable_stdio_uart(StereoBoy_FW 0)
# pico_enable_stdio_usb(StereoBoy_FW 1)

# # Add the standard library to the build (Only linking what the LED test needs)
# target_link_libraries(StereoBoy_FW
#         pico_stdlib
#         hardware_i2c
#         )

# # Add the standard include files to the build
# target_include_directories(StereoBoy_FW PRIVATE
#         ${CMAKE_CURRENT_LIST_DIR}
# )

# set(PICO_FLASH_SIZE_BYTES 2097152 CACHE STRING "RP2350 flash size")

# # Generate memory usage report
# target_link_options(StereoBoy_FW PRIVATE
#     -Wl,-Map=StereoBoy_FW.map
#     -Wl,--cref
#     -Wl,--print-memory-usage
# )

# # Commented out FatFS and other hardware libraries for this isolated test
# # add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

# # target_link_libraries(StereoBoy_FW 
# #         hardware_spi
# #         hardware_dma
# #         hardware_pio
# #         hardware_adc
# #         hardware_timer
# #         hardware_clocks
# #         hardware_interp
# #         pico_multicore
# #         no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
# #         )

# pico_add_extra_outputs(StereoBoy_FW)



########Main CMake###########

# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(StereoBoy_FW C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(StereoBoy_FW
    src/StereoBoy_FW.c
    src/hw_config.c # this is important!!! https://forums.raspberrypi.com/viewtopic.php?t=342315
    lib/sb_util/sb_util.c
    lib/sb_util/jukebox.c
    lib/sb_util/sb_init.c
    lib/sb_util/filehelper.c
    lib/codec/vs1053.c
    lib/dac/dac.c
    lib/display/fft.c
    lib/display/album_art.c
    lib/display/lissajous.c
    lib/display/picojpeg.c
    lib/led_driver/led_driver.c
    lib/buttons/buttons.c
    lib/pot/pot.c
    lib/sb_util/core1_entry.c
    lib/display/display.c
    lib/font/font.c
    lib/font/find_font_char.c
    )

pico_set_program_name(StereoBoy_FW "StereoBoy_FW")
pico_set_program_version(StereoBoy_FW "0.1")

# Generate PIO header
pico_generate_pio_header(StereoBoy_FW ${CMAKE_CURRENT_LIST_DIR}/pio/blink.pio)
pico_generate_pio_header(StereoBoy_FW ${CMAKE_CURRENT_LIST_DIR}/pio/display.pio)
pico_generate_pio_header(StereoBoy_FW ${CMAKE_CURRENT_LIST_DIR}/pio/main.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(StereoBoy_FW 0)
pico_enable_stdio_usb(StereoBoy_FW 1)

# Add the standard library to the build
target_link_libraries(StereoBoy_FW
        pico_stdlib
        hardware_pio
        hardware_adc
        hardware_dma
        hardware_pwm
        pico_multicore
        )

# Add the standard include files to the build
target_include_directories(StereoBoy_FW PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

set(PICO_FLASH_SIZE_BYTES 2097152 CACHE STRING "RP2350 flash size")

# Generate memory usage report
target_link_options(StereoBoy_FW PRIVATE
    -Wl,-Map=StereoBoy_FW.map
    -Wl,--cref
    -Wl,--print-memory-usage
)

add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

# Add any user requested libraries
target_link_libraries(StereoBoy_FW 
        hardware_spi
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_timer
        hardware_clocks
        hardware_interp
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
        )

pico_add_extra_outputs(StereoBoy_FW)

