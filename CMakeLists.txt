
# WSL Linux의 POSIX FreeRTOS 시뮬레이터를 빌드하는 설정 파일입니다.

cmake_minimum_required(VERSION 3.16)

project(asak_rtos C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(DEFINED ENV{FREERTOS_KERNEL_PATH})
    set(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}")
elseif(EXISTS "$ENV{HOME}/rtos-kiosk-course/third_party/FreeRTOS-Kernel/CMakeLists.txt")
    set(FREERTOS_KERNEL_PATH "$ENV{HOME}/rtos-kiosk-course/third_party/FreeRTOS-Kernel")
elseif(EXISTS "$ENV{HOME}/FreeRTOS-Kernel/CMakeLists.txt")
    set(FREERTOS_KERNEL_PATH "$ENV{HOME}/FreeRTOS-Kernel")
else()
    message(FATAL_ERROR "FreeRTOS-Kernel not found. Set FREERTOS_KERNEL_PATH.")
endif()

add_library(freertos_config INTERFACE)
target_include_directories(
    freertos_config
    SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
)

set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "" FORCE)
set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

add_subdirectory("${FREERTOS_KERNEL_PATH}" freertos-kernel)

find_package(Threads REQUIRED)

add_executable(asak_rtos
    src/main.c
    src/http_client.c
)

target_include_directories(asak_rtos PRIVATE src)

target_compile_options(asak_rtos PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(asak_rtos PRIVATE
    freertos_kernel
    freertos_config
    Threads::Threads
)