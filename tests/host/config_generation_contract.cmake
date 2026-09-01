# 生成 contract：Core 與 MyCar 不能混用 JSON，產物必須反映各自選定的裝置組態。
if(NOT DEFINED PNX_SOURCE_DIR OR NOT DEFINED PNX_BINARY_DIR)
    message(FATAL_ERROR
        "config_generation_contract requires PNX_SOURCE_DIR and PNX_BINARY_DIR")
endif()

set(core_contract_out "${PNX_BINARY_DIR}/f407-core-config-contract")
set(mycar_contract_out "${PNX_BINARY_DIR}/f407-mycar-config-contract")
set(combined_ps2_contract_out
    "${PNX_BINARY_DIR}/f407-combined-ps2-config-contract")
file(REMOVE_RECURSE "${core_contract_out}" "${mycar_contract_out}"
    "${combined_ps2_contract_out}")
file(MAKE_DIRECTORY "${core_contract_out}" "${mycar_contract_out}"
    "${combined_ps2_contract_out}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DIOC=${PNX_SOURCE_DIR}/boards/dji_c_board_f407/dji_c_board_f407.ioc"
        "-DPARAMS=${PNX_SOURCE_DIR}/configs/boards/dji_c_board_f407/params.json"
        "-DROBOT_CONFIG=${PNX_SOURCE_DIR}/configs/boards/dji_c_board_f407/robot.json"
        "-DOUT_DIR=${core_contract_out}"
        -DPNX_ENABLE_USB_CDC=OFF
        -P "${PNX_SOURCE_DIR}/configs/cmake/generate_config.cmake"
    RESULT_VARIABLE generation_result
    OUTPUT_VARIABLE generation_output
    ERROR_VARIABLE generation_error
)
if(NOT generation_result EQUAL 0)
    message(FATAL_ERROR
        "F407 Core config generation failed:\n${generation_output}\n${generation_error}")
endif()

file(READ "${core_contract_out}/config.hpp" core_config)
file(READ "${core_contract_out}/robot_config.hpp" core_robot_config)

foreach(required_text IN ITEMS
        "inline constexpr bool enable_ps2 = 0"
        "inline constexpr bsp::usart::port ps2 = bsp::usart::none"
        "inline constexpr std::uint32_t ps2_offline_timeout_ticks = 600"
        "inline constexpr std::uint32_t ps2_frame_timeout_ticks = 20"
        "inline constexpr float ps2_deadzone = 0.08f")
    string(FIND "${core_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated F407 config is missing: ${required_text}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DIOC=${PNX_SOURCE_DIR}/boards/dji_c_board_f407/dji_c_board_f407.ioc"
        "-DPARAMS=${PNX_SOURCE_DIR}/configs/vehicles/mycar_combined/params.ps2.json"
        "-DROBOT_CONFIG=${PNX_SOURCE_DIR}/configs/vehicles/mycar_combined/robot.json"
        "-DOUT_DIR=${combined_ps2_contract_out}"
        -DPNX_ENABLE_USB_CDC=OFF
        -DPNX_ENABLE_MYCAR_COMBINED=ON
        -DPNX_MYCAR_COMBINED_PS2=ON
        -P "${PNX_SOURCE_DIR}/configs/cmake/generate_config.cmake"
    RESULT_VARIABLE combined_generation_result
    OUTPUT_VARIABLE combined_generation_output
    ERROR_VARIABLE combined_generation_error
)
if(NOT combined_generation_result EQUAL 0)
    message(FATAL_ERROR
        "F407 combined PS2 config generation failed:\n"
        "${combined_generation_output}\n${combined_generation_error}")
endif()

file(READ "${combined_ps2_contract_out}/config.hpp" combined_ps2_config)
string(FIND "${combined_ps2_config}"
    "inline constexpr bsp::usart::port vision = usart6"
    vision_binding_index)
if(vision_binding_index EQUAL -1)
    message(FATAL_ERROR
        "Generated combined PS2 config must bind vision to USART6")
endif()

foreach(required_text IN ITEMS
        "inline constexpr bool has_remoter = 0"
        "inline constexpr bool enable_dr16 = 0"
        "inline constexpr bool has_motors = 0"
        "inline constexpr bool motor_dji = 0"
        "inline constexpr bsp::usart::port dr16 = bsp::usart::none")
    string(FIND "${core_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated Core config is missing: ${required_text}")
    endif()
endforeach()

foreach(required_text IN ITEMS
        "inline constexpr std::size_t motor_count = 0"
        "inline constexpr bool has_dji = 0"
        "lk_lk8016"
        "lk_lk9025")
    string(FIND "${core_robot_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated F407 robot config is missing: ${required_text}")
    endif()
endforeach()

foreach(forbidden_text IN ITEMS
        "front_left"
        "front_right"
        "rear_left"
        "rear_right")
    string(FIND "${core_robot_config}" "${forbidden_text}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
        message(FATAL_ERROR
            "Generated Core robot config leaked MyCar symbol: ${forbidden_text}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DIOC=${PNX_SOURCE_DIR}/boards/dji_c_board_f407/dji_c_board_f407.ioc"
        "-DPARAMS=${PNX_SOURCE_DIR}/configs/vehicles/mycar/params.json"
        "-DROBOT_CONFIG=${PNX_SOURCE_DIR}/configs/vehicles/mycar/robot.json"
        "-DOUT_DIR=${mycar_contract_out}"
        -DPNX_ENABLE_USB_CDC=OFF
        -P "${PNX_SOURCE_DIR}/configs/cmake/generate_config.cmake"
    RESULT_VARIABLE mycar_generation_result
    OUTPUT_VARIABLE mycar_generation_output
    ERROR_VARIABLE mycar_generation_error
)
if(NOT mycar_generation_result EQUAL 0)
    message(FATAL_ERROR
        "F407 MyCar config generation failed:\n${mycar_generation_output}\n${mycar_generation_error}")
endif()

file(READ "${mycar_contract_out}/config.hpp" mycar_config)
file(READ "${mycar_contract_out}/robot_config.hpp" mycar_robot_config)

foreach(required_text IN ITEMS
        "inline constexpr bool has_remoter = 1"
        "inline constexpr bool enable_dr16 = 1"
        "inline constexpr bool has_motors = 1"
        "inline constexpr bool motor_dji = 1"
        "inline constexpr bsp::usart::port dr16 = usart3")
    string(FIND "${mycar_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated MyCar config is missing: ${required_text}")
    endif()
endforeach()

foreach(required_text IN ITEMS
        "inline constexpr std::size_t motor_count = 4"
        "inline constexpr bool has_dji = 1"
        "front_left_model = model::dji_m2006"
        "front_right_model = model::dji_m2006"
        "rear_left_model = model::dji_m2006"
        "rear_right_model = model::dji_m2006"
        "inline constexpr ::motors::config front_left{\n    bsp::can::bus::can1,\n    bsp::can::bus_type::classic,\n    0x201U,\n    ::motors::mode::relax"
        "inline constexpr ::motors::config front_right{\n    bsp::can::bus::can1,\n    bsp::can::bus_type::classic,\n    0x202U,\n    ::motors::mode::relax"
        "inline constexpr ::motors::config rear_left{\n    bsp::can::bus::can1,\n    bsp::can::bus_type::classic,\n    0x204U,\n    ::motors::mode::relax"
        "inline constexpr ::motors::config rear_right{\n    bsp::can::bus::can1,\n    bsp::can::bus_type::classic,\n    0x203U,\n    ::motors::mode::relax")
    string(FIND "${mycar_robot_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated MyCar robot config is missing: ${required_text}")
    endif()
endforeach()
