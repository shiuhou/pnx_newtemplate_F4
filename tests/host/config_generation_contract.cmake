if(NOT DEFINED PNX_SOURCE_DIR OR NOT DEFINED PNX_BINARY_DIR)
    message(FATAL_ERROR
        "config_generation_contract requires PNX_SOURCE_DIR and PNX_BINARY_DIR")
endif()

set(contract_out "${PNX_BINARY_DIR}/f407-config-contract")
file(MAKE_DIRECTORY "${contract_out}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DIOC=${PNX_SOURCE_DIR}/boards/dji_c_board_f407/dji_c_board_f407.ioc"
        "-DPARAMS=${PNX_SOURCE_DIR}/configs/boards/dji_c_board_f407/params.json"
        "-DROBOT_CONFIG=${PNX_SOURCE_DIR}/configs/boards/dji_c_board_f407/robot.json"
        "-DOUT_DIR=${contract_out}"
        -DPNX_ENABLE_USB_CDC=OFF
        -P "${PNX_SOURCE_DIR}/configs/cmake/generate_config.cmake"
    RESULT_VARIABLE generation_result
    OUTPUT_VARIABLE generation_output
    ERROR_VARIABLE generation_error
)
if(NOT generation_result EQUAL 0)
    message(FATAL_ERROR
        "F407 config generation failed:\n${generation_output}\n${generation_error}")
endif()

file(READ "${contract_out}/config.hpp" generated_config)
file(READ "${contract_out}/robot_config.hpp" generated_robot_config)

foreach(required_text IN ITEMS
        "inline constexpr bool enable_ps2 = 0"
        "inline constexpr bsp::usart::port ps2 = bsp::usart::none"
        "inline constexpr std::uint32_t ps2_offline_timeout_ticks = 600"
        "inline constexpr std::uint32_t ps2_frame_timeout_ticks = 20"
        "inline constexpr float ps2_deadzone = 0.08f")
    string(FIND "${generated_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated F407 config is missing: ${required_text}")
    endif()
endforeach()

foreach(required_text IN ITEMS "lk_lk8016" "lk_lk9025")
    string(FIND "${generated_robot_config}" "${required_text}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR
            "Generated F407 robot config is missing: ${required_text}")
    endif()
endforeach()
