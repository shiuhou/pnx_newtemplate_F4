add_executable(pnx_usb_contract_host_tests
    fake_usb.cpp
    usb_contract_host_tests.cpp
)
target_include_directories(pnx_usb_contract_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/boards/dji_c_board_f407/bsp"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usb/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)

foreach(scenario IN ITEMS
    same_config_reinit
    init_resource_failure
    async_startup_success
    controller_startup_failure
    controller_activation_failure
    incompatible_reinit
    callback_ownership
    identity_fail_closed
    write_before_connected
    write_lock_contention
    bounded_write
    backpressure
    invalid_write
    rx_callback
    disconnect_cancels
    disconnect_pending_tx
    tx_error
    capabilities
    fill_tx_compat
    fill_tx_empty_retry
    f407_zlp_boundary
    f407_disconnect_race
    f407_startup_lifecycle
)
    add_test(
        NAME "usb_${scenario}"
        COMMAND pnx_usb_contract_host_tests "${scenario}"
    )
endforeach()

add_executable(pnx_pwm_contract_host_tests
    pwm_contract_host_tests.cpp
    fake_pwm.cpp
)
target_include_directories(pnx_pwm_contract_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME pwm_contract COMMAND pnx_pwm_contract_host_tests)

add_executable(pnx_pwm_a2_sequence_tests
    pwm_a2_sequence_tests.cpp
)
target_include_directories(pnx_pwm_a2_sequence_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/demo/cboard/pwm_a2"
)
add_test(NAME pwm_a2_sequence COMMAND pnx_pwm_a2_sequence_tests)

add_executable(pnx_template_consumer_api_host_tests
    template_consumer_api_host_tests.cpp
)
target_include_directories(pnx_template_consumer_api_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/spi/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/motor/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(
    NAME template_consumer_api
    COMMAND pnx_template_consumer_api_host_tests
)

add_test(
    NAME config_generation_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DPNX_BINARY_DIR=${CMAKE_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/config_generation_contract.cmake"
)

add_executable(pnx_shared_module_api_host_tests
    shared_module_api_host_tests.cpp
)
target_include_directories(pnx_shared_module_api_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/imu/bmi088/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/imu/imu/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/lk/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/motor/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/control/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/filter/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/math/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/msg/include"
    "${CMAKE_SOURCE_DIR}/pnx_modules/remoter/include"
    "${CMAKE_SOURCE_DIR}/boards/dji_c_board_f407/Middlewares/ST/threadx/common/inc"
    "${CMAKE_SOURCE_DIR}/boards/dji_c_board_f407/Middlewares/ST/threadx/ports/cortex_m4/gnu/inc"
)
add_test(
    NAME shared_module_api
    COMMAND pnx_shared_module_api_host_tests
)

add_executable(pnx_spi_contract_host_tests
    spi_contract_host_tests.cpp
    fake_spi.cpp
)
target_include_directories(pnx_spi_contract_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/spi/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME spi_contract COMMAND pnx_spi_contract_host_tests)

add_executable(pnx_flash_contract_host_tests
    flash_contract_host_tests.cpp
    fake_flash.cpp
)
target_include_directories(pnx_flash_contract_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/flash/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME flash_contract COMMAND pnx_flash_contract_host_tests)

add_executable(pnx_usart_contract_host_tests
    usart_contract_host_tests.cpp
    fake_usart.cpp
)
target_include_directories(pnx_usart_contract_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/boards/dji_c_board_f407/bsp"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME usart_contract COMMAND pnx_usart_contract_host_tests)

add_executable(pnx_can_host_tests
    can_host_tests.cpp
    fake_can.cpp
)
target_include_directories(pnx_can_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)

foreach(scenario IN ITEMS
    recovery_success
    recovery_failure
    plain_transmit_recovery
    guarded_transmit_no_recovery
)
    add_test(
        NAME "can_${scenario}"
        COMMAND pnx_can_host_tests "${scenario}"
    )
endforeach()
