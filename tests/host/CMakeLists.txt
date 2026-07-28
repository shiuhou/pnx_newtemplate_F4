add_executable(pnx_usb_contract_host_tests
    fake_usb_backend.cpp
    usb_contract_host_tests.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usb/src/bsp_usb.cpp"
)
target_include_directories(pnx_usb_contract_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/boards/dji_c_board_f407/pnx_backends"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usb/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)

foreach(scenario IN ITEMS
    init_once
    init_failure
    write_before_connected
    bounded_write
    backpressure
    invalid_write
    rx_callback
    disconnect_cancels
    tx_error
    capabilities
    fill_tx_compat
    f407_zlp_boundary
)
    add_test(
        NAME "usb_${scenario}"
        COMMAND pnx_usb_contract_host_tests "${scenario}"
    )
endforeach()

add_executable(pnx_m2006_host_tests
    m2006_host_tests.cpp
    fake_can_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/src/bsp_can.cpp"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/motor/src/motorhandler.cpp"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/src/djimotors.cpp"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/src/djimotorhandler.cpp"
)
target_include_directories(pnx_m2006_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/motor/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/math/include"
)
add_test(NAME m2006_template_path COMMAND pnx_m2006_host_tests)

add_executable(pnx_can_m2006_sequence_tests
    can_m2006_sequence_tests.cpp
)
target_include_directories(pnx_can_m2006_sequence_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/demo/cboard/can_m2006"
)
add_test(NAME can_m2006_bounded_sequence
    COMMAND pnx_can_m2006_sequence_tests)

add_executable(pnx_pwm_contract_host_tests
    pwm_contract_host_tests.cpp
    fake_pwm_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src/bsp_pwm.cpp"
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

add_executable(pnx_spi_contract_host_tests
    spi_contract_host_tests.cpp
    fake_spi_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/spi/src/bsp_spi.cpp"
)
target_include_directories(pnx_spi_contract_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/spi/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME spi_contract COMMAND pnx_spi_contract_host_tests)

add_executable(pnx_bmi088_transport_host_tests
    bmi088_transport_host_tests.cpp
    fake_bmi088_spi_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/spi/src/bsp_spi.cpp"
    "${CMAKE_SOURCE_DIR}/pnx_devices/imu/bmi088/src/bmi088_transport.cpp"
)
target_include_directories(pnx_bmi088_transport_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/spi/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/imu/bmi088/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME bmi088_transport COMMAND pnx_bmi088_transport_host_tests)

add_executable(pnx_flash_contract_host_tests
    flash_contract_host_tests.cpp
    fake_flash_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/flash/src/bsp_flash.cpp"
)
target_include_directories(pnx_flash_contract_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/flash/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME flash_contract COMMAND pnx_flash_contract_host_tests)

add_executable(pnx_dr16_protocol_host_tests
    dr16_protocol_host_tests.cpp
    "${CMAKE_SOURCE_DIR}/pnx_modules/remoter/src/dr16_protocol.cpp"
)
target_include_directories(pnx_dr16_protocol_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_modules/remoter/include"
)
add_test(NAME dr16_protocol COMMAND pnx_dr16_protocol_host_tests)

add_executable(pnx_usart_contract_host_tests
    usart_contract_host_tests.cpp
    fake_usart_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/src/bsp_usart.cpp"
)
target_include_directories(pnx_usart_contract_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME usart_contract COMMAND pnx_usart_contract_host_tests)

add_executable(pnx_can_host_tests
    can_host_tests.cpp
    fake_can_backend.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/src/bsp_can.cpp"
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
