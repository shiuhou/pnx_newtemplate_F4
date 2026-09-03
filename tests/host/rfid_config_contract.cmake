if(NOT DEFINED PNX_SOURCE_DIR OR NOT DEFINED PNX_BINARY_DIR)
    message(FATAL_ERROR
        "rfid_config_contract requires PNX_SOURCE_DIR and PNX_BINARY_DIR")
endif()

set(contract_root "${PNX_BINARY_DIR}/rfid-config-contract")
set(generator "${PNX_SOURCE_DIR}/configs/cmake/generate_config.cmake")
set(board_ioc
    "${PNX_SOURCE_DIR}/boards/dji_c_board_f407/dji_c_board_f407.ioc")
set(board_robot
    "${PNX_SOURCE_DIR}/configs/boards/dji_c_board_f407/robot.json")
set(rfid_params
    "${PNX_SOURCE_DIR}/configs/boards/dji_c_board_f407/params.rfid.json")
file(REMOVE_RECURSE "${contract_root}")
file(MAKE_DIRECTORY "${contract_root}")

function(generate_config name ioc params expect_success)
    set(output_dir "${contract_root}/${name}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DIOC=${ioc}"
            "-DPARAMS=${params}"
            "-DROBOT_CONFIG=${board_robot}"
            "-DOUT_DIR=${output_dir}"
            -DPNX_ENABLE_USB_CDC=OFF
            -DPNX_ENABLE_PWM_A2=OFF
            -DPNX_ENABLE_RFID_UID_DEBUG=ON
            -P "${generator}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(expect_success AND NOT result EQUAL 0)
        message(FATAL_ERROR
            "RFID config ${name} unexpectedly failed:\n${output}\n${error}")
    endif()
    if(NOT expect_success AND result EQUAL 0)
        message(FATAL_ERROR "RFID config ${name} unexpectedly succeeded")
    endif()
    set(${name}_out "${output_dir}" PARENT_SCOPE)
endfunction()

generate_config(valid "${board_ioc}" "${rfid_params}" TRUE)
file(READ "${valid_out}/config.hpp" valid_config)
foreach(required IN ITEMS
        "inline constexpr bool has_rfid = 1"
        "inline constexpr bsp::usart::port rfid = usart6"
        "inline constexpr bsp::usart::port test_report = usart1"
        "inline constexpr std::uint32_t address = 32"
        "inline constexpr std::uint32_t baud = 9600")
    string(FIND "${valid_config}" "${required}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Generated RFID config is missing: ${required}")
    endif()
endforeach()

file(READ "${rfid_params}" params_json)
function(expect_params_failure name original replacement)
    string(REPLACE "${original}" "${replacement}" modified "${params_json}")
    set(path "${contract_root}/${name}.json")
    file(WRITE "${path}" "${modified}")
    generate_config(${name} "${board_ioc}" "${path}" FALSE)
endfunction()

expect_params_failure(no_binding
    "\"rfid_uart\": \"usart6\"" "\"rfid_uart\": \"none\"")
expect_params_failure(disabled_but_bound
    "\"rfid\": true" "\"rfid\": false")
expect_params_failure(unknown_uart
    "\"rfid_uart\": \"usart6\"" "\"rfid_uart\": \"usart9\"")
expect_params_failure(no_tx_dma
    "\"rfid_uart\": \"usart6\"" "\"rfid_uart\": \"usart3\"")
expect_params_failure(test_report_collision
    "\"rfid_uart\": \"usart6\"" "\"rfid_uart\": \"usart1\"")

file(READ "${board_ioc}" ioc_text)
string(REPLACE "Dma.Request3=USART6_RX" "Dma.Request3=UNUSED"
    no_rx_dma_ioc "${ioc_text}")
set(no_rx_dma_path "${contract_root}/no-rx-dma.ioc")
file(WRITE "${no_rx_dma_path}" "${no_rx_dma_ioc}")
generate_config(no_rx_dma "${no_rx_dma_path}" "${rfid_params}" FALSE)

set(combined_params
    "${PNX_SOURCE_DIR}/configs/vehicles/mycar_combined/params.ps2.json")
file(READ "${combined_params}" combined_json)
string(REPLACE "\"rfid_uart\": \"usart6\""
    "\"rfid_uart\": \"usart1\"" remoter_collision "${combined_json}")
set(remoter_collision_path "${contract_root}/remoter-collision.json")
file(WRITE "${remoter_collision_path}" "${remoter_collision}")
generate_config(remoter_collision "${board_ioc}"
    "${remoter_collision_path}" FALSE)

string(REPLACE "\"referee\": false" "\"referee\": true"
    referee_collision "${params_json}")
string(REPLACE "\"referee_uart\": \"none\""
    "\"referee_uart\": \"usart6\""
    referee_collision "${referee_collision}")
set(referee_collision_path "${contract_root}/referee-collision.json")
file(WRITE "${referee_collision_path}" "${referee_collision}")
generate_config(referee_collision "${board_ioc}"
    "${referee_collision_path}" FALSE)
