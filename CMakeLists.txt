cmake_minimum_required(VERSION 3.22)

option(PNX_HOST_TESTS "Build host-only protocol and state-machine tests" OFF)
if(PNX_HOST_TESTS)
    project(pnx_host_tests LANGUAGES CXX)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "PNX_HOST_TESTS requires a native compiler, not an embedded toolchain")
    endif()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    enable_testing()
    add_subdirectory(tests/host)
    return()
endif()

set(PNX_SUPPORTED_DEMOS
    board_smoke can_receive motor_safe usb_cdc pwm_servo_lab
    imu_bmi088_lab ist8310_mag_lab)
if(NOT DEFINED PNX_DEMO OR PNX_DEMO STREQUAL "")
    set(PNX_DEMO "board_smoke" CACHE STRING
        "F407 application/demo entry" FORCE)
else()
    set(PNX_DEMO "${PNX_DEMO}" CACHE STRING
        "F407 application/demo entry")
endif()
set_property(CACHE PNX_DEMO PROPERTY STRINGS ${PNX_SUPPORTED_DEMOS})
if(NOT PNX_DEMO IN_LIST PNX_SUPPORTED_DEMOS)
    list(JOIN PNX_SUPPORTED_DEMOS ", " PNX_SUPPORTED_DEMOS_TEXT)
    message(FATAL_ERROR
        "Unsupported PNX_DEMO='${PNX_DEMO}'. "
        "Supported demos: ${PNX_SUPPORTED_DEMOS_TEXT}")
endif()

option(PNX_MOTOR_TEST_USB_ARM
    "Enable the attended USB CDC arm channel for motor_safe" OFF)

set(PNX_F407_USB_ENABLED OFF)
if(PNX_DEMO STREQUAL "usb_cdc" OR PNX_MOTOR_TEST_USB_ARM)
    set(PNX_F407_USB_ENABLED ON)
endif()
if(PNX_MOTOR_TEST_USB_ARM AND NOT PNX_DEMO STREQUAL "motor_safe")
    message(FATAL_ERROR
        "PNX_MOTOR_TEST_USB_ARM is valid only for motor_safe")
endif()

option(PNX_USB_DEVICE_IDENTITY_CONFIRMED
    "Confirm that the configured USB VID/PID identity is authorized" OFF)
set(PNX_USB_DEVICE_VID "0x0000" CACHE STRING
    "Authorized USB vendor ID; 0x0000 is the fail-closed default")
set(PNX_USB_DEVICE_PID "0x0000" CACHE STRING
    "Authorized USB product ID; 0x0000 is the fail-closed default")
set(PNX_USB_DEVICE_MANUFACTURER "PnX Development" CACHE STRING
    "USB manufacturer string for the authorized identity")
set(PNX_USB_DEVICE_PRODUCT "PnX F407 CDC (unassigned)" CACHE STRING
    "USB product string for the authorized identity")
set(PNX_USB_DEVICE_SERIAL "UNASSIGNED" CACHE STRING
    "Stable USB serial string for the target board")

if(PNX_USB_DEVICE_IDENTITY_CONFIRMED)
    if(NOT PNX_F407_USB_ENABLED)
        message(FATAL_ERROR
            "PNX_USB_DEVICE_IDENTITY_CONFIRMED requires a USB-enabled demo")
    endif()
    if(NOT PNX_USB_DEVICE_VID MATCHES "^(0[xX][0-9A-Fa-f]+|[0-9]+)$" OR
       NOT PNX_USB_DEVICE_PID MATCHES "^(0[xX][0-9A-Fa-f]+|[0-9]+)$")
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
            "STMicroelectronics VID 0x0483 is not authorized for this project")
    endif()
    if(PNX_USB_DEVICE_SERIAL STREQUAL "" OR
       PNX_USB_DEVICE_SERIAL STREQUAL "UNASSIGNED")
        message(FATAL_ERROR
            "Confirmed USB identity requires an assigned stable serial")
    endif()
    set(PNX_USB_DEVICE_IDENTITY_CONFIRMED_VALUE 1)
else()
    set(PNX_USB_DEVICE_VID_VALUE 0)
    set(PNX_USB_DEVICE_PID_VALUE 0)
    set(PNX_USB_DEVICE_IDENTITY_CONFIRMED_VALUE 0)
endif()

option(PNX_ENABLE_NONZERO_MOTOR_TEST
    "Compile the one-shot C-board non-zero motor pulse gate" OFF)
if(PNX_MOTOR_TEST_USB_ARM AND NOT PNX_ENABLE_NONZERO_MOTOR_TEST)
    message(FATAL_ERROR
        "PNX_MOTOR_TEST_USB_ARM requires the non-zero motor test gate")
endif()
set(PNX_MOTOR_TEST_BUS "none" CACHE STRING
    "Explicit motor-safe bus: none, can1, or can2")
set_property(CACHE PNX_MOTOR_TEST_BUS PROPERTY STRINGS none can1 can2)
set(PNX_MOTOR_TEST_ID "0" CACHE STRING
    "Explicit DJI feedback ID (0x201 through 0x208)")
set(PNX_MOTOR_TEST_CURRENT "0" CACHE STRING
    "Explicit signed raw current, absolute value no greater than 500")
set(PNX_MOTOR_TEST_MODEL "none" CACHE STRING
    "Explicit test motor model: none, m2006, m3508, or gm6020")
set_property(CACHE PNX_MOTOR_TEST_MODEL PROPERTY STRINGS
    none m2006 m3508 gm6020)

set(PNX_MOTOR_TEST_BUS_INDEX -1)
if(PNX_MOTOR_TEST_BUS STREQUAL "can1")
    set(PNX_MOTOR_TEST_BUS_INDEX 0)
elseif(PNX_MOTOR_TEST_BUS STREQUAL "can2")
    set(PNX_MOTOR_TEST_BUS_INDEX 1)
elseif(NOT PNX_MOTOR_TEST_BUS STREQUAL "none")
    message(FATAL_ERROR
        "PNX_MOTOR_TEST_BUS must be one of: none, can1, can2")
endif()

set(PNX_MOTOR_TEST_MODEL_VALUE 0)
if(PNX_MOTOR_TEST_MODEL STREQUAL "m2006")
    set(PNX_MOTOR_TEST_MODEL_VALUE 1)
elseif(PNX_MOTOR_TEST_MODEL STREQUAL "m3508")
    set(PNX_MOTOR_TEST_MODEL_VALUE 2)
elseif(PNX_MOTOR_TEST_MODEL STREQUAL "gm6020")
    set(PNX_MOTOR_TEST_MODEL_VALUE 3)
elseif(NOT PNX_MOTOR_TEST_MODEL STREQUAL "none")
    message(FATAL_ERROR
        "PNX_MOTOR_TEST_MODEL must be one of: none, m2006, m3508, gm6020")
endif()

if(NOT PNX_MOTOR_TEST_ID MATCHES "^(0[xX][0-9A-Fa-f]+|[0-9]+)$")
    message(FATAL_ERROR
        "PNX_MOTOR_TEST_ID must be an integer or hexadecimal integer")
endif()
if(NOT PNX_MOTOR_TEST_CURRENT MATCHES "^-?[0-9]+$")
    message(FATAL_ERROR
        "PNX_MOTOR_TEST_CURRENT must be a signed decimal integer")
endif()
math(EXPR PNX_MOTOR_TEST_ID_VALUE "${PNX_MOTOR_TEST_ID}")
math(EXPR PNX_MOTOR_TEST_CURRENT_VALUE "${PNX_MOTOR_TEST_CURRENT}")
if(PNX_MOTOR_TEST_CURRENT_VALUE LESS 0)
    math(EXPR PNX_MOTOR_TEST_CURRENT_ABS
        "0 - ${PNX_MOTOR_TEST_CURRENT_VALUE}")
else()
    set(PNX_MOTOR_TEST_CURRENT_ABS "${PNX_MOTOR_TEST_CURRENT_VALUE}")
endif()

if(PNX_ENABLE_NONZERO_MOTOR_TEST)
    if(NOT PNX_DEMO STREQUAL "motor_safe")
        message(FATAL_ERROR
            "PNX_ENABLE_NONZERO_MOTOR_TEST is valid only for motor_safe")
    endif()
    if(PNX_MOTOR_TEST_BUS_INDEX LESS 0 OR
       PNX_MOTOR_TEST_ID_VALUE LESS 0x201 OR
       PNX_MOTOR_TEST_ID_VALUE GREATER 0x208 OR
       PNX_MOTOR_TEST_CURRENT_VALUE EQUAL 0 OR
       PNX_MOTOR_TEST_CURRENT_ABS GREATER 500 OR
       PNX_MOTOR_TEST_MODEL_VALUE EQUAL 0)
        message(FATAL_ERROR
            "Non-zero motor test requires explicit bus can1/can2, "
            "ID 0x201..0x208, model, and non-zero current with abs <= 500")
    endif()
    set(PNX_NONZERO_MOTOR_TEST_ENABLED 1)
else()
    set(PNX_NONZERO_MOTOR_TEST_ENABLED 0)
    set(PNX_MOTOR_TEST_BUS_INDEX -1)
    set(PNX_MOTOR_TEST_ID_VALUE 0)
    set(PNX_MOTOR_TEST_CURRENT_VALUE 0)
    set(PNX_MOTOR_TEST_MODEL_VALUE 0)
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

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PNX_CUBEMX_GATE_COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_cubemx_production.py"
    --repo-root "${CMAKE_CURRENT_SOURCE_DIR}"
    --selected-ioc "${PNX_BOARD_IOC}"
    --check-submodules-clean
)
execute_process(
    COMMAND ${PNX_CUBEMX_GATE_COMMAND}
    RESULT_VARIABLE PNX_CUBEMX_GATE_RESULT
    OUTPUT_VARIABLE PNX_CUBEMX_GATE_OUTPUT
    ERROR_VARIABLE PNX_CUBEMX_GATE_ERROR
)
if(NOT PNX_CUBEMX_GATE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "F407 CubeMX production gate failed before config generation:\n"
        "${PNX_CUBEMX_GATE_OUTPUT}${PNX_CUBEMX_GATE_ERROR}")
endif()
string(STRIP "${PNX_CUBEMX_GATE_OUTPUT}" PNX_CUBEMX_GATE_OUTPUT)
message(STATUS "${PNX_CUBEMX_GATE_OUTPUT}")

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

file(GLOB_RECURSE PNX_EMBEDDED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/can/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/diagnostics/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/dwt/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/indicator/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/usart/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_devices/motors/motor/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_devices/motors/dji/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_modules/motor/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/demo/cboard/common/*.cpp"
)
if(PNX_F407_USB_ENABLED)
    file(GLOB_RECURSE PNX_F407_USB_BSP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/usb/*.cpp")
    list(APPEND PNX_EMBEDDED_SOURCES ${PNX_F407_USB_BSP_SOURCES})
endif()
list(APPEND PNX_EMBEDDED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/demo/cboard/${PNX_DEMO}/app.cpp")

file(GLOB_RECURSE PNX_EMBEDDED_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_bsp/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_devices/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_devices/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_modules/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_modules/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_libs/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_libs/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/demo/cboard/*.hpp"
)
set(PNX_EMBEDDED_INCLUDE_DIRS)
foreach(header IN LISTS PNX_EMBEDDED_HEADERS)
    get_filename_component(header_dir "${header}" DIRECTORY)
    list(APPEND PNX_EMBEDDED_INCLUDE_DIRS "${header_dir}")
endforeach()
list(REMOVE_DUPLICATES PNX_EMBEDDED_INCLUDE_DIRS)

add_executable(${PROJECT_NAME})
add_subdirectory("${PNX_BOARD_CUBEMX_DIR}" board_support)

add_library(pnx_dji_protocol_static_tests OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/dji_protocol_static_tests.cpp")
target_include_directories(pnx_dji_protocol_static_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/pnx_devices/motors/dji/include")
target_compile_features(pnx_dji_protocol_static_tests PRIVATE cxx_std_17)

file(GLOB PNX_BOARD_BACKEND_SOURCES CONFIGURE_DEPENDS
    "${PNX_BOARD_ROOT}/pnx_backends/*.c"
    "${PNX_BOARD_ROOT}/pnx_backends/*.cpp"
    "${PNX_BOARD_ROOT}/pnx_backends/*.S"
)
if(NOT PNX_F407_USB_ENABLED)
    list(FILTER PNX_BOARD_BACKEND_SOURCES EXCLUDE REGEX
        "[/\\\\]usb_backend\\.cpp$")
endif()

target_sources(${PROJECT_NAME} PRIVATE
    ${PNX_EMBEDDED_SOURCES}
    ${PNX_BOARD_BACKEND_SOURCES}
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PNX_EMBEDDED_INCLUDE_DIRS}
    "${PNX_EMBEDDED_GENERATED_DIR}"
    "${PNX_BOARD_ROOT}/pnx_backends"
)
target_link_libraries(${PROJECT_NAME} stm32cubemx)

set(PNX_F407_ACTUAL_TARGET_SOURCES)
set(PNX_F407_SOURCE_GRAPH_TARGETS
    ${PROJECT_NAME} STM32_Drivers ThreadX)
if(PNX_F407_USB_ENABLED)
    list(APPEND PNX_F407_SOURCE_GRAPH_TARGETS USBX)
endif()
foreach(PNX_F407_TARGET IN LISTS PNX_F407_SOURCE_GRAPH_TARGETS)
    get_target_property(PNX_F407_TARGET_SOURCES
        ${PNX_F407_TARGET} SOURCES)
    if(PNX_F407_TARGET_SOURCES STREQUAL
       "PNX_F407_TARGET_SOURCES-NOTFOUND")
        message(FATAL_ERROR
            "F407 target has no inspectable source graph: ${PNX_F407_TARGET}")
    endif()
    list(APPEND PNX_F407_ACTUAL_TARGET_SOURCES
        ${PNX_F407_TARGET_SOURCES})
endforeach()
list(REMOVE_DUPLICATES PNX_F407_ACTUAL_TARGET_SOURCES)

set(PNX_CUBEMX_SOURCE_LIST_FILE
    "${CMAKE_BINARY_DIR}/f407-actual-target-sources.txt")
file(WRITE "${PNX_CUBEMX_SOURCE_LIST_FILE}" "")
foreach(PNX_CUBEMX_SOURCE IN LISTS PNX_F407_ACTUAL_TARGET_SOURCES)
    file(APPEND "${PNX_CUBEMX_SOURCE_LIST_FILE}"
        "${PNX_CUBEMX_SOURCE}\n")
endforeach()
execute_process(
    COMMAND ${PNX_CUBEMX_GATE_COMMAND}
        --source-list-file "${PNX_CUBEMX_SOURCE_LIST_FILE}"
    RESULT_VARIABLE PNX_CUBEMX_GRAPH_RESULT
    OUTPUT_VARIABLE PNX_CUBEMX_GRAPH_OUTPUT
    ERROR_VARIABLE PNX_CUBEMX_GRAPH_ERROR
)
if(NOT PNX_CUBEMX_GRAPH_RESULT EQUAL 0)
    message(FATAL_ERROR
        "F407 actual target source-graph gate failed "
        "(${PNX_CUBEMX_GRAPH_RESULT}):\n"
        "${PNX_CUBEMX_GRAPH_OUTPUT}${PNX_CUBEMX_GRAPH_ERROR}")
endif()

string(TOUPPER "${PNX_DEMO}" PNX_DEMO_DEFINE)
string(REGEX REPLACE "[^A-Z0-9]" "_" PNX_DEMO_DEFINE
    "${PNX_DEMO_DEFINE}")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "PNX_BOARD_DJI_C_BOARD_F407=1"
    "PNX_DEMO_${PNX_DEMO_DEFINE}=1"
    "PNX_NONZERO_MOTOR_TEST_ENABLED=${PNX_NONZERO_MOTOR_TEST_ENABLED}"
    "PNX_MOTOR_TEST_BUS_INDEX=${PNX_MOTOR_TEST_BUS_INDEX}"
    "PNX_MOTOR_TEST_ID=${PNX_MOTOR_TEST_ID_VALUE}"
    "PNX_MOTOR_TEST_CURRENT=${PNX_MOTOR_TEST_CURRENT_VALUE}"
    "PNX_MOTOR_TEST_MODEL=${PNX_MOTOR_TEST_MODEL_VALUE}"
    "PNX_MOTOR_TEST_USB_ARM=$<BOOL:${PNX_MOTOR_TEST_USB_ARM}>"
    "PNX_F407_USB_ENABLED=$<BOOL:${PNX_F407_USB_ENABLED}>"
    "PNX_USB_DEVICE_IDENTITY_CONFIRMED=${PNX_USB_DEVICE_IDENTITY_CONFIRMED_VALUE}"
    "PNX_USB_DEVICE_VID=${PNX_USB_DEVICE_VID_VALUE}"
    "PNX_USB_DEVICE_PID=${PNX_USB_DEVICE_PID_VALUE}"
    "PNX_USB_DEVICE_MANUFACTURER=\"${PNX_USB_DEVICE_MANUFACTURER}\""
    "PNX_USB_DEVICE_PRODUCT=\"${PNX_USB_DEVICE_PRODUCT}\""
    "PNX_USB_DEVICE_SERIAL=\"${PNX_USB_DEVICE_SERIAL}\""
)

if(CMAKE_OBJCOPY)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O ihex
            "$<TARGET_FILE_NAME:${PROJECT_NAME}>"
            "${PROJECT_NAME}.hex"
        COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE_NAME:${PROJECT_NAME}>"
            "${PROJECT_NAME}.bin"
        COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
        VERBATIM
    )
endif()

message(STATUS "PnX product: F407-only team candidate")
message(STATUS "PnX demo: ${PNX_DEMO}")
message(STATUS "PnX board root: ${PNX_BOARD_ROOT}")
message(STATUS
    "PnX non-zero motor test compiled: ${PNX_ENABLE_NONZERO_MOTOR_TEST}")
message(STATUS "PnX F407 USB CDC closure: ${PNX_F407_USB_ENABLED}")
if(PNX_F407_USB_ENABLED)
    message(STATUS
        "PnX USB identity confirmed: ${PNX_USB_DEVICE_IDENTITY_CONFIRMED}")
endif()

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
