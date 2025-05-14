# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(monitoramento_rios C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(monitoramento_rios monitoramento_rios.c inc/ssd1306.c)

pico_set_program_name(monitoramento_rios "monitoramento_rios")
pico_set_program_version(monitoramento_rios "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(monitoramento_rios 1)
pico_enable_stdio_usb(monitoramento_rios 1)

target_compile_definitions(monitoramento_rios PRIVATE LWIP_HTTPD=1)

# Add any user requested libraries
target_link_libraries(monitoramento_rios
        pico_stdlib
        pico_time
        hardware_adc
        hardware_clocks
        hardware_i2c
        hardware_uart
        pico_cyw43_arch_lwip_threadsafe_background)

# Add the standard include files to the build
target_include_directories(monitoramento_rios PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${PICO_SDK_PATH}/lib/lwip/src/include
        ${PICO_SDK_PATH}/lib/lwip/src/include/arch
        ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
)

target_sources(monitoramento_rios PRIVATE
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/httpd.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/fs.c
)

pico_add_extra_outputs(monitoramento_rios)

