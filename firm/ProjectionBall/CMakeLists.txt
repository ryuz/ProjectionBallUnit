# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)
set(CMAKE_BUILD_TYPE "Release")
set(OPTIMIZATION_FLAG 1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
# PICO_SDK_PATH is set via environment variable or VS Code Pico extension
#set(PICO_SDK_PATH "/home/mond/pico-sdk")
#set(PICO_SDK_PATH "/mnt/d/31_WS/Pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ProjectionBall C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ProjectionBall)

target_sources(ProjectionBall PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/ProjectionBall.c
		${CMAKE_CURRENT_LIST_DIR}/encoder_ma732.c
		${CMAKE_CURRENT_LIST_DIR}/encoder_as5048a.c
		${CMAKE_CURRENT_LIST_DIR}/motor_ctrl.c
        ${CMAKE_CURRENT_LIST_DIR}/rtc_rv8803.c
        ${CMAKE_CURRENT_LIST_DIR}/rtc_sd30XX.c
        ${CMAKE_CURRENT_LIST_DIR}/path_const.c
        ${CMAKE_CURRENT_LIST_DIR}/path_ctrl.c
        ${CMAKE_CURRENT_LIST_DIR}/path_font.c
        ${CMAKE_CURRENT_LIST_DIR}/flash_ctrl.c
        ${CMAKE_CURRENT_LIST_DIR}/console.c
        )

pico_set_program_name(ProjectionBall "ProjectionBall")
pico_set_program_version(ProjectionBall "0.1")

pico_enable_stdio_uart(ProjectionBall 0)
pico_enable_stdio_usb(ProjectionBall 1)




# Add the standard library to the build
target_link_libraries(ProjectionBall pico_stdlib pico_multicore)

# Add any user requested libraries
target_link_libraries(ProjectionBall
        hardware_spi
        hardware_i2c
        hardware_timer
		hardware_pwm
        hardware_flash
        hardware_rtc
        hardware_sync
        hardware_watchdog
        )
		
target_link_options(ProjectionBall PRIVATE "LINKER:--print-memory-usage")


pico_add_extra_outputs(ProjectionBall)

