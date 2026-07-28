cmake_minimum_required(VERSION 3.22)

option(PNX_HOST_TESTS "Build host-only shared-logic tests" OFF)
if(PNX_HOST_TESTS)
    project(pnx_host_tests LANGUAGES CXX)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "PNX_HOST_TESTS requires a native compiler")
    endif()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    enable_testing()
    add_subdirectory(tests/host)
    return()
endif()

option(PNX_ENABLE_USB_CDC "Build the additive F407 USB CDC closure" OFF)
set(PNX_F407_USB_ENABLED "${PNX_ENABLE_USB_CDC}")

option(PNX_USB_DEVICE_IDENTITY_CONFIRMED
    "Confirm that the configured USB VID/PID identity is authorized" OFF)
set(PNX_USB_DEVICE_VID "0x0000" CACHE STRING
    "Authorized USB vendor ID; 0x0000 is fail-closed")
set(PNX_USB_DEVICE_PID "0x0000" CACHE STRING
    "Authorized USB product ID; 0x0000 is fail-closed")
set(PNX_USB_DEVICE_MANUFACTURER "PnX Development" CACHE STRING
    "USB manufacturer string")
set(PNX_USB_DEVICE_PRODUCT "PnX F407 CDC (unassigned)" CACHE STRING
    "USB product string")
set(PNX_USB_DEVICE_SERIAL "UNASSIGNED" CACHE STRING
    "Stable USB serial string")

if(PNX_USB_DEVICE_IDENTITY_CONFIRMED)
    if(NOT PNX_ENABLE_USB_CDC)
        message(FATAL_ERROR
            "USB identity confirmation requires PNX_ENABLE_USB_CDC")
    endif()
    if(NOT PNX_USB_DEVICE_VID MATCHES
       "^(0[xX][0-9A-Fa-f]+|[0-9]+)$" OR
       NOT PNX_USB_DEVICE_PID MATCHES
       "^(0[xX][0-9A-Fa-f]+|[0-9]+)$")
        message(FATAL_ERROR "USB VID and PID must be integer literals")
    endif()
    math(EXPR PNX_USB_DEVICE_VID_VALUE "${PNX_USB_DEVICE_VID}")
    math(EXPR PNX_USB_DEVICE_PID_VALUE "${PNX_USB_DEVICE_PID}")
    if(PNX_USB_DEVICE_VID_VALUE EQUAL 0 OR
       PNX_USB_DEVICE_PID_VALUE EQUAL 0)
        message(FATAL_ERROR
            "Confirmed USB identity requires non-zero VID and PID")
    endif()
    if(PNX_USB_DEVICE_VID_VALUE EQUAL 0x0483)
        message(FATAL_ERROR
            "STMicroelectronics VID 0x0483 is not authorized")
    endif()
    if(PNX_USB_DEVICE_SERIAL STREQUAL "" OR
       PNX_USB_DEVICE_SERIAL STREQUAL "UNASSIGNED")
        message(FATAL_ERROR
            "Confirmed USB identity requires an assigned serial")
    endif()
    set(PNX_USB_DEVICE_IDENTITY_CONFIRMED_VALUE 1)
else()
    set(PNX_USB_DEVICE_VID_VALUE 0)
    set(PNX_USB_DEVICE_PID_VALUE 0)
    set(PNX_USB_DEVICE_IDENTITY_CONFIRMED_VALUE 0)
endif()

set(PNX_BOARD_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/boards/dji_c_board_f407")
set(PNX_BOARD_IOC
    "${PNX_BOARD_ROOT}/dji_c_board_f407.ioc")
set(PNX_BOARD_PARAMS
    "${CMAKE_CURRENT_SOURCE_DIR}/configs/boards/dji_c_board_f407/params.json")
set(PNX_BOARD_ROBOT_CONFIG
    "${CMAKE_CURRENT_SOURCE_DIR}/configs/boards/dji_c_board_f407/robot.json")
set(PNX_BOARD_CUBEMX_DIR
    "${PNX_BOARD_ROOT}/cmake/stm32cubemx")

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(PNX_EMBEDDED_COMPILER_HINT
        "${CMAKE_C_COMPILER};${CMAKE_CXX_COMPILER}")
    if(PNX_EMBEDDED_COMPILER_HINT MATCHES "arm-none-eabi")
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake")
    endif()
    unset(PNX_EMBEDDED_COMPILER_HINT)
endif()

project(pnx_embedded LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PNX_EMBEDDED_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/configs")
set(PNX_EMBEDDED_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PNX_BOARD_IOC}"
    "${PNX_BOARD_PARAMS}"
    "${PNX_BOARD_ROBOT_CONFIG}"
    "${PNX_EMBEDDED_CONFIG_DIR}/cmake/generate_config.cmake"
    "${PNX_EMBEDDED_CONFIG_DIR}/cmake/import_ioc.cmake"
)

set(IOC "${PNX_BOARD_IOC}")
set(PARAMS "${PNX_BOARD_PARAMS}")
set(ROBOT_CONFIG "${PNX_BOARD_ROBOT_CONFIG}")
set(OUT_DIR "${PNX_EMBEDDED_GENERATED_DIR}")
include("${PNX_EMBEDDED_CONFIG_DIR}/cmake/generate_config.cmake")
unset(IOC)
unset(PARAMS)
unset(ROBOT_CONFIG)
unset(OUT_DIR)

set(PNX_EMBEDDED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/diagnostics/src/bsp_diagnostics.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/dwt/src/bsp_dwt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/indicator/src/bsp_indicator.cpp"
    "${PNX_BOARD_ROOT}/pnx_backends/diagnostics_backend.cpp"
    "${PNX_BOARD_ROOT}/pnx_backends/fault_handlers.S"
    "${PNX_BOARD_ROOT}/pnx_backends/indicator_backend.cpp"
)

set(PNX_EMBEDDED_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/diagnostics/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/dwt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/indicator/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_libs/common/include"
    "${PNX_BOARD_ROOT}/pnx_backends"
    "${PNX_EMBEDDED_GENERATED_DIR}"
)

if(PNX_ENABLE_USB_CDC)
    list(APPEND PNX_EMBEDDED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/usb/src/bsp_usb.cpp"
        "${PNX_BOARD_ROOT}/pnx_backends/usb_backend.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/demo/cboard/usb_cdc/app.cpp"
    )
    list(APPEND PNX_EMBEDDED_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/usb/include"
    )
else()
    list(APPEND PNX_EMBEDDED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/demo/cboard/board_smoke/app.cpp"
    )
endif()

add_executable(${PROJECT_NAME})
add_subdirectory("${PNX_BOARD_CUBEMX_DIR}" board_support)

target_sources(${PROJECT_NAME} PRIVATE ${PNX_EMBEDDED_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PNX_EMBEDDED_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} stm32cubemx)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "PNX_F407_USB_ENABLED=$<BOOL:${PNX_ENABLE_USB_CDC}>"
    "PNX_USB_DEVICE_IDENTITY_CONFIRMED=${PNX_USB_DEVICE_IDENTITY_CONFIRMED_VALUE}"
    "PNX_USB_DEVICE_VID=${PNX_USB_DEVICE_VID_VALUE}"
    "PNX_USB_DEVICE_PID=${PNX_USB_DEVICE_PID_VALUE}"
    "PNX_USB_DEVICE_MANUFACTURER=\"${PNX_USB_DEVICE_MANUFACTURER}\""
    "PNX_USB_DEVICE_PRODUCT=\"${PNX_USB_DEVICE_PRODUCT}\""
    "PNX_USB_DEVICE_SERIAL=\"${PNX_USB_DEVICE_SERIAL}\""
)

message(STATUS "PnX target: DJI C-board STM32F407")
message(STATUS "PnX USB CDC closure: ${PNX_ENABLE_USB_CDC}")
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
