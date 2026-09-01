cmake_minimum_required(VERSION 3.22)

# 組態生成入口：驗證 board/vehicle JSON 與 IOC 後輸出到 build/generated。
# 請改 JSON 或本檔規則，不要直接手改 generated 檔案。

include(${CMAKE_CURRENT_LIST_DIR}/import_ioc.cmake)

if(NOT DEFINED IOC OR NOT DEFINED PARAMS OR NOT DEFINED OUT_DIR)
    message(FATAL_ERROR "generate_config.cmake requires -DIOC=... -DPARAMS=... -DOUT_DIR=...")
endif()

pnx_ioc_parse("${IOC}")

file(READ "${PARAMS}" params_json)

# Required keys have no default. A missing key is a configuration error, not an
# invitation to guess: silently falling back would let one board's params.json
# inherit another board's semantics.
function(_pnx_require_json out_var)
    string(JSON value ERROR_VARIABLE err GET "${params_json}" ${ARGN})
    if(err)
        string(REPLACE ";" "." key_path "${ARGN}")
        message(FATAL_ERROR
            "params.json is missing required key '${key_path}' (${PARAMS}). "
            "Add it explicitly; there is no default.")
    endif()
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

# --- params.json: build ---
_pnx_require_json(motor_dji build motors dji)
_pnx_require_json(motor_dm build motors dm)
_pnx_require_json(motor_lk build motors lk)

function(_pnx_json_bool_to_cmake val out_var)
    if(val STREQUAL "true" OR val STREQUAL "1" OR val STREQUAL "ON")
        set(${out_var} ON PARENT_SCOPE)
    else()
        set(${out_var} OFF PARENT_SCOPE)
    endif()
endfunction()

function(_pnx_feature_override key default_value out_var)
    string(JSON feature_value ERROR_VARIABLE feature_err
        GET "${params_json}" build features ${key})
    if(feature_err)
        set(${out_var} "${default_value}" PARENT_SCOPE)
        return()
    endif()
    if(feature_value STREQUAL "true"
       OR feature_value STREQUAL "1"
       OR feature_value STREQUAL "ON")
        set(${out_var} 1 PARENT_SCOPE)
    else()
        set(${out_var} 0 PARENT_SCOPE)
    endif()
endfunction()

function(_pnx_can_id_type_expr val out_var)
    string(TOLOWER "${val}" val_lower)
    if(val_lower STREQUAL "standard" OR val_lower STREQUAL "std")
        set(${out_var} "id_type::standard" PARENT_SCOPE)
    elseif(val_lower STREQUAL "extended" OR val_lower STREQUAL "ext")
        set(${out_var} "id_type::extended" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "can id_type must be standard or extended")
    endif()
endfunction()

_pnx_json_bool_to_cmake("${motor_dji}" MOTOR_DJI)
_pnx_json_bool_to_cmake("${motor_dm}" MOTOR_DM)
_pnx_json_bool_to_cmake("${motor_lk}" MOTOR_LK)

# --- params.json: bindings ---
# Use "none" to state that a board has no such binding. Omitting the key is an
# error: an absent remoter_uart previously defaulted to the H7 wiring (uart5).
_pnx_require_json(remoter_uart bindings remoter_uart)
_pnx_require_json(referee_uart bindings referee_uart)
_pnx_require_json(remoter_source remoter source)
string(JSON vision_uart ERROR_VARIABLE vision_uart_error
    GET "${params_json}" bindings vision_uart)
if(vision_uart_error)
    set(vision_uart "none")
endif()
string(TOLOWER "${remoter_uart}" remoter_uart)
string(TOLOWER "${referee_uart}" referee_uart)
string(TOLOWER "${vision_uart}" vision_uart)
string(TOLOWER "${remoter_source}" remoter_source)

if(PNX_ENABLE_MYCAR_COMBINED AND PNX_MYCAR_COMBINED_PS2 AND
   vision_uart STREQUAL "none")
    message(FATAL_ERROR
        "PS2 combined product requires bindings.vision_uart")
endif()

# --- HAS_* from IOC + bindings ---
pnx_ioc_hw_in_list("${PNX_IOC_UART_HW}" "${remoter_uart}" remoter_uart_present)
pnx_ioc_uart_has_dma("${PNX_IOC_LINES}" "${remoter_uart}" "RX" remoter_has_rx_dma)
if(remoter_uart_present AND remoter_has_rx_dma)
    set(HAS_REMOTER 1)
else()
    set(HAS_REMOTER 0)
endif()

# VT03 is wired to UART7 on the H7 reference board. This is a hardware fact of
# that board, not a portable one: any board without UART7 simply reports
# HAS_VT03=0. Do not treat "uart7" here as a configurable binding.
set(PNX_VT03_FIXED_UART "uart7")
pnx_ioc_hw_in_list("${PNX_IOC_UART_HW}" "${PNX_VT03_FIXED_UART}" vt03_uart_present)
pnx_ioc_uart_has_dma("${PNX_IOC_LINES}" "${PNX_VT03_FIXED_UART}" "RX" vt03_has_rx_dma)
if(vt03_uart_present AND vt03_has_rx_dma)
    set(HAS_VT03 1)
else()
    set(HAS_VT03 0)
endif()

pnx_ioc_hw_in_list("${PNX_IOC_UART_HW}" "${referee_uart}" referee_uart_present)
if(referee_uart_present)
    set(HAS_REFEREE 1)
else()
    set(HAS_REFEREE 0)
endif()

# UI is drawn by sending referee-system packets, so its hardware capability is
# exactly the referee UART's. HAS_UI is therefore derived from HAS_REFEREE
# rather than being an independent hardware fact. The two are still separate
# feature switches: build.features.ui can turn UI off on a board that has
# referee support (see the override below).
set(HAS_UI ${HAS_REFEREE})

_pnx_feature_override("remoter" "${HAS_REMOTER}" HAS_REMOTER)
_pnx_feature_override("vt03" "${HAS_VT03}" HAS_VT03)
_pnx_feature_override("referee" "${HAS_REFEREE}" HAS_REFEREE)
_pnx_feature_override("ui" "${HAS_UI}" HAS_UI)
set(HAS_PS2 ${HAS_REMOTER})

# "auto" infers the source from what the IOC and bindings actually support.
# It must be requested explicitly -- it is no longer what an absent key means.
if(remoter_source STREQUAL "auto")
    if(HAS_REMOTER)
        set(ENABLE_DR16 1)
        set(ENABLE_VT03 0)
        set(ENABLE_PS2 0)
    elseif(HAS_VT03)
        set(ENABLE_DR16 0)
        set(ENABLE_VT03 1)
        set(ENABLE_PS2 0)
    else()
        set(ENABLE_DR16 0)
        set(ENABLE_VT03 0)
        set(ENABLE_PS2 0)
    endif()
elseif(remoter_source STREQUAL "dr16")
    if(NOT HAS_REMOTER)
        message(FATAL_ERROR "params.remoter.source=dr16 requires remoter UART RX DMA support in board/board.ioc")
    endif()
    set(ENABLE_DR16 1)
    set(ENABLE_VT03 0)
    set(ENABLE_PS2 0)
elseif(remoter_source STREQUAL "vt03")
    if(NOT HAS_VT03)
        message(FATAL_ERROR "params.remoter.source=vt03 requires UART7 RX DMA support in board/board.ioc")
    endif()
    set(ENABLE_DR16 0)
    set(ENABLE_VT03 1)
    set(ENABLE_PS2 0)
elseif(remoter_source STREQUAL "ps2")
    if(NOT HAS_PS2)
        message(FATAL_ERROR
            "params.remoter.source=ps2 requires the bound remoter UART to have RX DMA support")
    endif()
    set(ENABLE_DR16 0)
    set(ENABLE_VT03 0)
    set(ENABLE_PS2 1)
elseif(remoter_source STREQUAL "none"
       OR remoter_source STREQUAL "off"
       OR remoter_source STREQUAL "disabled")
    set(ENABLE_DR16 0)
    set(ENABLE_VT03 0)
    set(ENABLE_PS2 0)
else()
    message(FATAL_ERROR
        "params.remoter.source='${remoter_source}' is invalid; "
        "must be one of: dr16, vt03, ps2, none, auto")
endif()

list(LENGTH PNX_IOC_CAN_HW can_hw_count)
if(can_hw_count GREATER 0)
    set(HAS_MOTORS 1)
else()
    set(HAS_MOTORS 0)
endif()
_pnx_feature_override("motors" "${HAS_MOTORS}" HAS_MOTORS)

if(PNX_IOC_HAS_USB_FS)
    set(HW_HAS_USB_FS 1)
else()
    set(HW_HAS_USB_FS 0)
endif()

if(PNX_IOC_HAS_USB_HS)
    set(HW_HAS_USB_HS 1)
else()
    set(HW_HAS_USB_HS 0)
endif()

if(PNX_IOC_HAS_USB)
    set(HW_HAS_USB 1)
else()
    set(HW_HAS_USB 0)
endif()

if(PNX_ENABLE_USB_CDC AND NOT PNX_IOC_HAS_USB)
    message(FATAL_ERROR
        "PNX_ENABLE_USB_CDC requires USB hardware in ${IOC}")
endif()

if(PNX_ENABLE_USB_CDC AND PNX_IOC_HAS_USB)
    set(ENABLE_USBX ON)
else()
    set(ENABLE_USBX OFF)
endif()

if(ENABLE_USBX)
    set(ENABLE_USBX_C 1)
else()
    set(ENABLE_USBX_C 0)
endif()

# --- BSP policy defaults (embedded, formerly board.json) ---
set(can_max_rx_callbacks 8)
set(tx_delay_tdc 13)
set(tx_delay_filter 13)

set(can_enabled_list "")
set(can_type_list "")
set(can_id_type_list "")
set(can_config_list "")
set(can_handle_enum_entries "")
set(can_bus_enum_entries "none = 0xFF")
set(can_bus_index 0)

foreach(hw ${PNX_IOC_CAN_HW})
    string(TOLOWER "${hw}" hw_lower)

    list(APPEND can_enabled_list "true")

    pnx_ioc_fdcan_frame_format("${PNX_IOC_LINES}" "${hw_lower}" bus_type)
    if(bus_type STREQUAL "fd")
        set(can_type_expr "bus_type::fd")
    else()
        set(can_type_expr "bus_type::classic")
    endif()
    list(APPEND can_type_list "${can_type_expr}")

    string(JSON manual_can_id_type ERROR_VARIABLE json_err GET "${params_json}" can ${hw_lower} id_type)
    if(NOT json_err AND NOT manual_can_id_type STREQUAL "")
        _pnx_can_id_type_expr("${manual_can_id_type}" can_id_type_expr)
    else()
        pnx_ioc_fdcan_std_filters("${PNX_IOC_LINES}" "${hw_lower}" std_filters)
        pnx_ioc_fdcan_ext_filters("${PNX_IOC_LINES}" "${hw_lower}" ext_filters)
        if(std_filters GREATER 0)
            set(can_id_type_expr "id_type::standard")
        elseif(ext_filters GREATER 0)
            set(can_id_type_expr "id_type::extended")
        else()
            set(can_id_type_expr "id_type::standard")
        endif()
    endif()
    list(APPEND can_id_type_list "${can_id_type_expr}")
    list(APPEND can_config_list "{ true, handle_id::${hw_lower}, ${can_type_expr}, ${can_id_type_expr} }")

    string(APPEND can_handle_enum_entries ", ${hw_lower}")
    string(APPEND can_bus_enum_entries ", ${hw_lower} = ${can_bus_index}")
    math(EXPR can_bus_index "${can_bus_index} + 1")
endforeach()

list(JOIN can_enabled_list ", " can_enabled_cpp)
list(JOIN can_type_list ", " can_type_cpp)
list(JOIN can_id_type_list ", " can_id_type_cpp)
list(JOIN can_config_list ", " can_config_cpp)
list(LENGTH PNX_IOC_CAN_HW can_bus_count)

set(can_template_compat_aliases "")
list(FIND PNX_IOC_CAN_HW "CAN1" can1_index)
if(can1_index GREATER_EQUAL 0)
    string(APPEND can_template_compat_aliases ", fdcan1 = can1")
endif()
list(FIND PNX_IOC_CAN_HW "CAN2" can2_index)
if(can2_index GREATER_EQUAL 0)
    string(APPEND can_template_compat_aliases ", fdcan2 = can2")
endif()

set(usart_enabled_list "")
set(usart_config_list "")
set(usart_handle_enum_entries "")
set(usart_port_enum_entries "")
set(uart_binding_body "")
set(usart_port_index 0)

foreach(hw ${PNX_IOC_UART_HW})
    string(TOLOWER "${hw}" hw_lower)

    pnx_ioc_uart_has_dma("${PNX_IOC_LINES}" "${hw_lower}" "RX" has_rx_dma)
    pnx_ioc_uart_has_dma("${PNX_IOC_LINES}" "${hw_lower}" "TX" has_tx_dma)
    pnx_to_json_bool("${has_rx_dma}" has_rx_dma_cpp)
    pnx_to_json_bool("${has_tx_dma}" has_tx_dma_cpp)
    list(APPEND usart_enabled_list "true")
    list(APPEND usart_config_list "{ true, handle_id::${hw_lower}, ${has_rx_dma_cpp}, ${has_tx_dma_cpp} }")
    string(APPEND usart_handle_enum_entries ", ${hw_lower}")

    if(usart_port_index GREATER 0)
        string(APPEND uart_binding_body "\n")
    endif()
    string(APPEND uart_binding_body "inline constexpr bsp::usart::port ${hw_lower} = ${usart_port_index};")

    if(usart_port_index GREATER 0)
        string(APPEND usart_port_enum_entries ", ")
    endif()
    string(APPEND usart_port_enum_entries "${hw_lower} = ${usart_port_index}")
    math(EXPR usart_port_index "${usart_port_index} + 1")
endforeach()

list(JOIN usart_enabled_list ", " usart_enabled_cpp)
list(JOIN usart_config_list ", " usart_config_cpp)
list(LENGTH PNX_IOC_UART_HW usart_port_count)

pnx_ioc_uart_index("${PNX_IOC_UART_HW}" "${remoter_uart}" dr16_port_idx)
pnx_ioc_uart_index("${PNX_IOC_UART_HW}" "${remoter_uart}" ps2_port_idx)
pnx_ioc_uart_index("${PNX_IOC_UART_HW}" "uart7" vt03_port_idx)
pnx_ioc_uart_index("${PNX_IOC_UART_HW}" "${referee_uart}" referee_port_idx)
pnx_ioc_uart_index("${PNX_IOC_UART_HW}" "${vision_uart}" vision_port_idx)

if(dr16_port_idx GREATER_EQUAL 0)
    set(dr16_binding "${remoter_uart}")
else()
    set(dr16_binding "bsp::usart::none")
endif()
if(vt03_port_idx GREATER_EQUAL 0)
    set(vt03_binding "uart7")
else()
    set(vt03_binding "bsp::usart::none")
endif()
if(ps2_port_idx GREATER_EQUAL 0)
    set(ps2_binding "${remoter_uart}")
else()
    set(ps2_binding "bsp::usart::none")
endif()
if(referee_port_idx GREATER_EQUAL 0)
    set(referee_binding "${referee_uart}")
else()
    set(referee_binding "bsp::usart::none")
endif()

set(active_remoter_uart "")
if(ENABLE_DR16)
    set(active_remoter_uart "${remoter_uart}")
elseif(ENABLE_VT03)
    set(active_remoter_uart "uart7")
elseif(ENABLE_PS2)
    set(active_remoter_uart "${remoter_uart}")
endif()

if(NOT vision_uart STREQUAL "none")
    pnx_ioc_uart_has_dma(
        "${PNX_IOC_LINES}" "${vision_uart}" "RX" vision_has_rx_dma)
    if(vision_port_idx LESS 0 OR NOT vision_has_rx_dma)
        message(FATAL_ERROR
            "params.bindings.vision_uart=${vision_uart} requires an IOC UART with RX DMA")
    endif()
    if(NOT active_remoter_uart STREQUAL "" AND
       vision_uart STREQUAL active_remoter_uart)
        message(FATAL_ERROR
            "params.bindings.vision_uart=${vision_uart} conflicts with the active remoter UART")
    endif()
    if(NOT referee_uart STREQUAL "none" AND
       vision_uart STREQUAL referee_uart)
        message(FATAL_ERROR
            "params.bindings.vision_uart=${vision_uart} conflicts with the referee UART")
    endif()
    set(vision_binding "${vision_uart}")
else()
    set(vision_binding "bsp::usart::none")
endif()

# The test-report UART belongs to the validation closures, not to a product
# image. Only require and validate it when a validation closure is actually
# selected, so a product build is not forced to nominate a test port.
if(PNX_ENABLE_PWM_A2)
    set(PNX_VALIDATION_CLOSURE_SELECTED 1)
else()
    set(PNX_VALIDATION_CLOSURE_SELECTED 0)
endif()

if(PNX_VALIDATION_CLOSURE_SELECTED)
    _pnx_require_json(test_report_uart test report_uart)
    string(TOLOWER "${test_report_uart}" test_report_uart)
    pnx_ioc_uart_index("${PNX_IOC_UART_HW}" "${test_report_uart}" test_report_port_idx)
    if(test_report_port_idx LESS 0)
        message(FATAL_ERROR "params.test.report_uart=${test_report_uart} is not present in board/board.ioc")
    endif()
    if(NOT active_remoter_uart STREQUAL "" AND test_report_uart STREQUAL active_remoter_uart)
        message(FATAL_ERROR "params.test.report_uart=${test_report_uart} conflicts with the active remoter UART")
    endif()
    set(test_report_binding "${test_report_uart}")
else()
    set(test_report_binding "bsp::usart::none")
endif()

# --- params namespace (explicit keys per section) ---
set(generated_semicolon_token "__PNX_GENERATED_SEMICOLON__")

function(_pnx_param_float section key out_var)
    string(JSON val ERROR_VARIABLE err GET "${params_json}" ${section} ${key})
    if(err)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    set(${out_var} "  inline constexpr float ${key} = ${val}f${generated_semicolon_token}\n" PARENT_SCOPE)
endfunction()

function(_pnx_param_uint section key out_var)
    string(JSON val ERROR_VARIABLE err GET "${params_json}" ${section} ${key})
    if(err)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    set(${out_var} "  inline constexpr std::uint32_t ${key} = ${val}${generated_semicolon_token}\n" PARENT_SCOPE)
endfunction()

function(_pnx_param_bool section key out_var)
    string(JSON val ERROR_VARIABLE err GET "${params_json}" ${section} ${key})
    if(err)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    if(val STREQUAL "true" OR val STREQUAL "1" OR val STREQUAL "ON")
        set(${out_var} "  inline constexpr bool ${key} = true${generated_semicolon_token}\n" PARENT_SCOPE)
    else()
        set(${out_var} "  inline constexpr bool ${key} = false${generated_semicolon_token}\n" PARENT_SCOPE)
    endif()
endfunction()

set(params_ahrs_body "")
_pnx_param_float("ahrs" "imu_offset_x" _line)
string(APPEND params_ahrs_body "${_line}")
_pnx_param_uint("ahrs" "imu_thread_priority" _line)
string(APPEND params_ahrs_body "${_line}")
_pnx_param_uint("ahrs" "temp_thread_priority" _line)
string(APPEND params_ahrs_body "${_line}")
_pnx_param_float("ahrs" "target_temp" _line)
string(APPEND params_ahrs_body "${_line}")
if(params_ahrs_body STREQUAL "")
    string(CONCAT params_ahrs_body
        "  inline constexpr float imu_offset_x = 0.0f${generated_semicolon_token}\n"
        "  inline constexpr std::uint32_t imu_thread_priority = 3${generated_semicolon_token}\n"
        "  inline constexpr std::uint32_t temp_thread_priority = 4${generated_semicolon_token}\n"
        "  inline constexpr float target_temp = 45.0f${generated_semicolon_token}\n")
endif()

set(params_remoter_body "")
_pnx_param_uint("remoter" "thread_priority" _line)
if(_line STREQUAL "")
    set(_line "  inline constexpr std::uint32_t thread_priority = 2${generated_semicolon_token}\n")
endif()
string(APPEND params_remoter_body "${_line}")
_pnx_param_uint("remoter" "rx_timeout_ticks" _line)
if(_line STREQUAL "")
    set(_line "  inline constexpr std::uint32_t rx_timeout_ticks = 100${generated_semicolon_token}\n")
endif()
string(APPEND params_remoter_body "${_line}")
_pnx_param_uint("remoter" "ps2_offline_timeout_ticks" _line)
if(_line STREQUAL "")
    set(_line "  inline constexpr std::uint32_t ps2_offline_timeout_ticks = 600${generated_semicolon_token}\n")
endif()
string(APPEND params_remoter_body "${_line}")
_pnx_param_uint("remoter" "ps2_frame_timeout_ticks" _line)
if(_line STREQUAL "")
    set(_line "  inline constexpr std::uint32_t ps2_frame_timeout_ticks = 20${generated_semicolon_token}\n")
endif()
string(APPEND params_remoter_body "${_line}")
_pnx_param_float("remoter" "ps2_deadzone" _line)
if(_line STREQUAL "")
    set(_line "  inline constexpr float ps2_deadzone = 0.08f${generated_semicolon_token}\n")
endif()
string(APPEND params_remoter_body "${_line}")

set(params_referee_body "")
_pnx_param_uint("referee" "thread_priority" _line)
string(APPEND params_referee_body "${_line}")
if(params_referee_body STREQUAL "")
    set(params_referee_body "  inline constexpr std::uint32_t thread_priority = 8${generated_semicolon_token}\n")
endif()

# params::test only exists for validation closures; a product image gets an
# empty namespace rather than a test thread priority it will never honour.
set(params_test_body "")
if(PNX_VALIDATION_CLOSURE_SELECTED)
    _pnx_param_uint("test" "thread_priority" _line)
    string(APPEND params_test_body "${_line}")
    _pnx_param_bool("test" "auto_run_on_boot" _line)
    string(APPEND params_test_body "${_line}")
    if(params_test_body STREQUAL "")
        string(CONCAT params_test_body
            "  inline constexpr std::uint32_t thread_priority = 10${generated_semicolon_token}\n"
            "  inline constexpr bool auto_run_on_boot = true${generated_semicolon_token}\n")
    endif()
endif()

set(params_usb_body "")
_pnx_param_uint("usb" "read_thread_priority" _line)
string(APPEND params_usb_body "${_line}")
_pnx_param_uint("usb" "write_thread_priority" _line)
string(APPEND params_usb_body "${_line}")
_pnx_param_uint("usb" "period_ticks" _line)
string(APPEND params_usb_body "${_line}")
if(params_usb_body STREQUAL "")
    string(CONCAT params_usb_body
        "  inline constexpr std::uint32_t read_thread_priority = 5${generated_semicolon_token}\n"
        "  inline constexpr std::uint32_t write_thread_priority = 5${generated_semicolon_token}\n"
        "  inline constexpr std::uint32_t period_ticks = 2${generated_semicolon_token}\n")
endif()

if(MOTOR_DJI)
    set(MOTOR_DJI_C 1)
else()
    set(MOTOR_DJI_C 0)
endif()
if(MOTOR_DM)
    set(MOTOR_DM_C 1)
else()
    set(MOTOR_DM_C 0)
endif()
if(MOTOR_LK)
    set(MOTOR_LK_C 1)
else()
    set(MOTOR_LK_C 0)
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

set(CONFIG_HPP "${OUT_DIR}/config.hpp")
set(ROBOT_CONFIG_HPP "${OUT_DIR}/robot_config.hpp")

file(WRITE "${CONFIG_HPP}"
"#pragma once\n"
"// Generated from the selected board IOC and params.json. Do not edit.\n\n"
"#include <array>\n"
"#include <cstddef>\n"
"#include <cstdint>\n\n"
"#define HW_HAS_USB_FS ${HW_HAS_USB_FS}\n"
"#define HW_HAS_USB_HS ${HW_HAS_USB_HS}\n"
"#define HW_HAS_USB ${HW_HAS_USB}\n"
"#define ENABLE_USBX ${ENABLE_USBX_C}\n"
"#define HAS_REMOTER ${HAS_REMOTER}\n"
"#define HAS_VT03 ${HAS_VT03}\n"
"#define HAS_PS2 ${HAS_PS2}\n"
"#define ENABLE_DR16 ${ENABLE_DR16}\n"
"#define ENABLE_VT03 ${ENABLE_VT03}\n"
"#define ENABLE_PS2 ${ENABLE_PS2}\n"
"#define HAS_REFEREE ${HAS_REFEREE}\n"
"#define HAS_UI ${HAS_UI}\n"
"#define HAS_MOTORS ${HAS_MOTORS}\n"
"#define MOTOR_DJI ${MOTOR_DJI_C}\n"
"#define MOTOR_DM ${MOTOR_DM_C}\n"
"#define MOTOR_LK ${MOTOR_LK_C}\n\n"
"namespace config::feature {\n\n"
"inline constexpr bool hw_has_usb_fs = ${HW_HAS_USB_FS};\n"
"inline constexpr bool hw_has_usb_hs = ${HW_HAS_USB_HS};\n"
"inline constexpr bool hw_has_usb = ${HW_HAS_USB};\n"
"inline constexpr bool enable_usbx = ${ENABLE_USBX_C};\n"
"inline constexpr bool has_remoter = ${HAS_REMOTER};\n"
"inline constexpr bool has_vt03 = ${HAS_VT03};\n"
"inline constexpr bool has_ps2 = ${HAS_PS2};\n"
"inline constexpr bool enable_dr16 = ${ENABLE_DR16};\n"
"inline constexpr bool enable_vt03 = ${ENABLE_VT03};\n"
"inline constexpr bool enable_ps2 = ${ENABLE_PS2};\n"
"inline constexpr bool has_referee = ${HAS_REFEREE};\n"
"inline constexpr bool has_ui = ${HAS_UI};\n"
"inline constexpr bool has_motors = ${HAS_MOTORS};\n"
"inline constexpr bool motor_dji = ${MOTOR_DJI_C};\n"
"inline constexpr bool motor_dm = ${MOTOR_DM_C};\n"
"inline constexpr bool motor_lk = ${MOTOR_LK_C};\n\n"
"} // namespace config::feature\n\n"
"namespace bsp {\n"
"namespace can {\n\n"
"enum class bus_type : std::uint8_t { classic = 0, fd = 1 };\n"
"enum class id_type : std::uint8_t { standard = 0, extended = 1 };\n"
"enum class handle_id : std::uint8_t { none = 0${can_handle_enum_entries} };\n"
"enum class bus : std::uint8_t { ${can_bus_enum_entries}${can_template_compat_aliases} };\n\n"
"struct bus_config\n"
"{\n"
"    bool enabled = false;\n"
"    handle_id handle = handle_id::none;\n"
"    bus_type type = bus_type::classic;\n"
"    id_type filter_id_type = id_type::standard;\n"
"};\n\n"
"inline constexpr std::size_t bus_count = ${can_bus_count};\n"
"inline constexpr std::size_t max_rx_callbacks = ${can_max_rx_callbacks};\n"
"inline constexpr std::uint32_t tx_delay_comp_tdc = ${tx_delay_tdc};\n"
"inline constexpr std::uint32_t tx_delay_comp_filter = ${tx_delay_filter};\n\n"
"inline constexpr std::array<bus_config, bus_count> configs = {{ ${can_config_cpp} }};\n"
"inline constexpr std::array<bool, bus_count> enabled = { ${can_enabled_cpp} };\n"
"inline constexpr std::array<bus_type, bus_count> configured_bus_types = { ${can_type_cpp} };\n"
"inline constexpr std::array<id_type, bus_count> filter_id_types = { ${can_id_type_cpp} };\n\n"
"} // namespace can\n\n"
"namespace usart {\n\n"
"using port = std::size_t;\n\n"
"inline constexpr port none = static_cast<port>(-1);\n\n"
"enum class handle_id : std::uint8_t { none = 0${usart_handle_enum_entries} };\n\n"
"struct port_config\n"
"{\n"
"    bool enabled = false;\n"
"    handle_id handle = handle_id::none;\n"
"    bool has_rx_dma = false;\n"
"    bool has_tx_dma = false;\n"
"};\n\n"
"inline constexpr std::size_t port_count = ${usart_port_count};\n"
"inline constexpr std::array<port_config, port_count> configs = {{ ${usart_config_cpp} }};\n"
"inline constexpr std::array<bool, port_count> enabled = { ${usart_enabled_cpp} };\n\n"
"} // namespace usart\n"
"} // namespace bsp\n\n"
"namespace app {\n"
"namespace uart {\n\n"
"${uart_binding_body}\n\n"
"inline constexpr bsp::usart::port dr16 = ${dr16_binding};\n"
"inline constexpr bsp::usart::port vt03 = ${vt03_binding};\n"
"inline constexpr bsp::usart::port ps2 = ${ps2_binding};\n"
"inline constexpr bsp::usart::port referee = ${referee_binding};\n"
"inline constexpr bsp::usart::port vision = ${vision_binding};\n"
"inline constexpr bsp::usart::port test_report = ${test_report_binding};\n\n"
"} // namespace uart\n"
"} // namespace app\n\n"
"namespace params::ahrs {\n"
"${params_ahrs_body}"
"} // namespace params::ahrs\n\n"
"namespace params::remoter {\n"
"${params_remoter_body}"
"} // namespace params::remoter\n\n"
"namespace params::referee {\n"
"${params_referee_body}"
"} // namespace params::referee\n\n"
"namespace params::test {\n"
"${params_test_body}"
"} // namespace params::test\n\n"
"namespace params::usb {\n"
"${params_usb_body}"
"} // namespace params::usb\n"
)
file(READ "${CONFIG_HPP}" config_hpp_raw)
string(REPLACE "${generated_semicolon_token}" ";" config_hpp_fixed "${config_hpp_raw}")
file(WRITE "${CONFIG_HPP}" "${config_hpp_fixed}")

message(STATUS "Generated ${CONFIG_HPP}")

function(_pnx_cpp_identifier input out_var)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" ident "${input}")
    string(REGEX REPLACE "_+" "_" ident "${ident}")
    string(REGEX REPLACE "^_+|_+$" "" ident "${ident}")
    if(ident STREQUAL "")
        set(ident "unnamed")
    endif()
    if(ident MATCHES "^[0-9]")
        set(ident "_${ident}")
    endif()
    set(${out_var} "${ident}" PARENT_SCOPE)
endfunction()

function(_pnx_motor_type_flag model out_var)
    string(TOLOWER "${model}" model_lower)
    if(model_lower MATCHES "^dji_")
        set(${out_var} "Dji" PARENT_SCOPE)
    elseif(model_lower MATCHES "^dm_")
        set(${out_var} "Dm" PARENT_SCOPE)
    elseif(model_lower MATCHES "^lk_")
        set(${out_var} "Lk" PARENT_SCOPE)
    elseif(model_lower MATCHES "^xv2_")
        set(${out_var} "Xv2" PARENT_SCOPE)
    else()
        set(${out_var} "Other" PARENT_SCOPE)
    endif()
endfunction()

function(_pnx_motor_control_mode_expr mode out_var)
    string(TOLOWER "${mode}" mode_lower)
    if(mode_lower STREQUAL "" OR mode_lower STREQUAL "relax")
        set(${out_var} "::motors::mode::relax" PARENT_SCOPE)
    elseif(mode_lower STREQUAL "torque")
        set(${out_var} "::motors::mode::torque" PARENT_SCOPE)
    elseif(mode_lower STREQUAL "mit")
        set(${out_var} "::motors::mode::mit" PARENT_SCOPE)
    elseif(mode_lower STREQUAL "pos_speed" OR mode_lower STREQUAL "position_speed")
        set(${out_var} "::motors::mode::pos_speed" PARENT_SCOPE)
    elseif(mode_lower STREQUAL "speed" OR mode_lower STREQUAL "velocity")
        set(${out_var} "::motors::mode::speed" PARENT_SCOPE)
    elseif(mode_lower STREQUAL "multi")
        set(${out_var} "::motors::mode::multi" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "robot motor control_mode must be relax, torque, mit, pos_speed, speed, or multi")
    endif()
endfunction()

function(_pnx_motor_model_expr model out_var)
    string(TOLOWER "${model}" model_lower)
    if(model_lower STREQUAL "dji_m2006")
        set(${out_var} "model::dji_m2006" PARENT_SCOPE)
    elseif(model_lower STREQUAL "dji_m3508")
        set(${out_var} "model::dji_m3508" PARENT_SCOPE)
    elseif(model_lower STREQUAL "dji_gm6020")
        set(${out_var} "model::dji_gm6020" PARENT_SCOPE)
    elseif(model_lower STREQUAL "dji_xroll")
        set(${out_var} "model::dji_xroll" PARENT_SCOPE)
    elseif(model_lower STREQUAL "dm_dm4310")
        set(${out_var} "model::dm_dm4310" PARENT_SCOPE)
    elseif(model_lower STREQUAL "dm_dm8009p")
        set(${out_var} "model::dm_dm8009p" PARENT_SCOPE)
    elseif(model_lower STREQUAL "lk_lk8016")
        set(${out_var} "model::lk_lk8016" PARENT_SCOPE)
    elseif(model_lower STREQUAL "lk_lk9025")
        set(${out_var} "model::lk_lk9025" PARENT_SCOPE)
    elseif(model_lower STREQUAL "unknown" OR model_lower STREQUAL "")
        set(${out_var} "model::unknown" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "robot motor model ${model} is not supported")
    endif()
endfunction()

set(robot_motors_body "")
set(robot_motor_count 0)
set(robot_has_dji 0)
set(robot_has_dm 0)
set(robot_has_lk 0)
set(robot_has_xv2 0)
set(robot_has_other 0)
set(robot_dm_id_base "0x01")
set(robot_dm_master_id_base "0x05")
set(robot_dm_max_motors "4")

if(DEFINED ROBOT_CONFIG AND EXISTS "${ROBOT_CONFIG}")
    file(READ "${ROBOT_CONFIG}" robot_json)
    string(JSON robot_dm_id_base_json ERROR_VARIABLE json_err GET "${robot_json}" devices motors dm id_base)
    if(NOT json_err AND NOT robot_dm_id_base_json STREQUAL "")
        set(robot_dm_id_base "${robot_dm_id_base_json}")
    endif()
    string(JSON robot_dm_master_id_base_json ERROR_VARIABLE json_err GET "${robot_json}" devices motors dm master_id_base)
    if(NOT json_err AND NOT robot_dm_master_id_base_json STREQUAL "")
        set(robot_dm_master_id_base "${robot_dm_master_id_base_json}")
    endif()
    string(JSON robot_dm_max_motors_json ERROR_VARIABLE json_err GET "${robot_json}" devices motors dm max_motors)
    if(NOT json_err AND NOT robot_dm_max_motors_json STREQUAL "")
        set(robot_dm_max_motors "${robot_dm_max_motors_json}")
    endif()

    string(JSON motor_count ERROR_VARIABLE json_err LENGTH "${robot_json}" devices motors list)
    if(json_err)
        set(motor_count 0)
    endif()

    if(motor_count GREATER 0)
        math(EXPR motor_last_index "${motor_count} - 1")
        foreach(i RANGE 0 ${motor_last_index})
            string(JSON motor_name ERROR_VARIABLE json_err GET "${robot_json}" devices motors list ${i} name)
            if(json_err OR motor_name STREQUAL "")
                message(FATAL_ERROR "robot motor at index ${i} requires a non-empty name")
            endif()
            string(JSON motor_model ERROR_VARIABLE json_err GET "${robot_json}" devices motors list ${i} model)
            if(json_err OR motor_model STREQUAL "")
                set(motor_model "unknown")
            endif()
            string(JSON motor_can_bus ERROR_VARIABLE json_err GET "${robot_json}" devices motors list ${i} can_bus)
            if(json_err OR motor_can_bus STREQUAL "")
                message(FATAL_ERROR "robot motor ${motor_name} requires can_bus")
            endif()
            string(JSON motor_can_type ERROR_VARIABLE json_err GET "${robot_json}" devices motors list ${i} can_type)
            if(json_err OR motor_can_type STREQUAL "")
                message(FATAL_ERROR "robot motor ${motor_name} requires can_type")
            endif()
            string(JSON motor_can_id ERROR_VARIABLE json_err GET "${robot_json}" devices motors list ${i} can_id)
            if(json_err OR motor_can_id STREQUAL "")
                message(FATAL_ERROR "robot motor ${motor_name} requires can_id")
            endif()
            string(JSON motor_control_mode ERROR_VARIABLE json_err GET "${robot_json}" devices motors list ${i} control_mode)
            if(json_err)
                set(motor_control_mode "relax")
            endif()

            string(TOLOWER "${motor_can_bus}" motor_can_bus_lower)
            string(TOLOWER "${motor_can_type}" motor_can_type_lower)
            pnx_ioc_hw_in_list("${PNX_IOC_CAN_HW}" "${motor_can_bus_lower}" motor_can_bus_present)
            if(NOT motor_can_bus_present)
                message(FATAL_ERROR "robot motor ${motor_name} uses ${motor_can_bus_lower}, but it is not present in ${IOC}")
            endif()
            if(NOT motor_can_type_lower STREQUAL "classic" AND NOT motor_can_type_lower STREQUAL "fd")
                message(FATAL_ERROR "robot motor ${motor_name} can_type must be classic or fd")
            endif()

            _pnx_cpp_identifier("${motor_name}" motor_ident)
            string(REGEX MATCH "^[A-Za-z_][A-Za-z0-9_]*$" valid_ident "${motor_ident}")
            if(NOT valid_ident)
                message(FATAL_ERROR "robot motor ${motor_name} cannot be converted to a valid C++ identifier")
            endif()

            _pnx_motor_type_flag("${motor_model}" motor_type_flag)
            _pnx_motor_control_mode_expr("${motor_control_mode}" motor_control_mode_expr)
            _pnx_motor_model_expr("${motor_model}" motor_model_expr)
            if(motor_type_flag STREQUAL "Dji")
                set(robot_has_dji 1)
            elseif(motor_type_flag STREQUAL "Dm")
                set(robot_has_dm 1)
            elseif(motor_type_flag STREQUAL "Lk")
                set(robot_has_lk 1)
            elseif(motor_type_flag STREQUAL "Xv2")
                set(robot_has_xv2 1)
            else()
                set(robot_has_other 1)
            endif()

            string(APPEND robot_motors_body
                "// ${motor_model}\n"
                "inline constexpr model ${motor_ident}_model = ${motor_model_expr};\n"
                "inline constexpr ::motors::config ${motor_ident}{\n"
                "    bsp::can::bus::${motor_can_bus_lower},\n"
                "    bsp::can::bus_type::${motor_can_type_lower},\n"
                "    ${motor_can_id}U,\n"
                "    ${motor_control_mode_expr},\n"
                "};\n\n")
            math(EXPR robot_motor_count "${robot_motor_count} + 1")
        endforeach()
    endif()
endif()

if(robot_motors_body STREQUAL "")
    set(robot_motors_body "// No motors are described in the robot device tree.\n")
endif()

file(WRITE "${ROBOT_CONFIG_HPP}"
"#pragma once\n"
"// Generated from robot device tree. Do not edit.\n\n"
"#include \"motor.hpp\"\n\n"
"#include <cstddef>\n"
"#include <cstdint>\n\n"
"namespace robot::motors {\n\n"
"inline constexpr std::size_t motor_count = ${robot_motor_count};\n"
"inline constexpr bool has_dji = ${robot_has_dji};\n"
"inline constexpr bool has_dm = ${robot_has_dm};\n"
"inline constexpr bool has_lk = ${robot_has_lk};\n"
"inline constexpr bool has_xv2 = ${robot_has_xv2};\n"
"inline constexpr bool has_other = ${robot_has_other};\n\n"
"enum class model : std::uint8_t {\n"
"    unknown = 0,\n"
"    dji_m2006,\n"
"    dji_m3508,\n"
"    dji_gm6020,\n"
"    dji_xroll,\n"
"    dm_dm4310,\n"
"    dm_dm8009p,\n"
"    lk_lk8016,\n"
"    lk_lk9025,\n"
"};\n\n"
"namespace dm {\n"
"inline constexpr std::uint32_t id_base = ${robot_dm_id_base}U;\n"
"inline constexpr std::uint32_t master_id_base = ${robot_dm_master_id_base}U;\n"
"inline constexpr std::size_t max_motors = ${robot_dm_max_motors};\n"
"} // namespace dm\n\n"
"${robot_motors_body}"
"} // namespace robot::motors\n")

message(STATUS "Generated ${ROBOT_CONFIG_HPP}")
