add_executable(pnx_usb_contract_host_tests
    # Host suite 只驗證硬體無關邏輯與 contract；不能取代 CAN、DR16 或實車證據。
    fake_usb.cpp
    usb_contract_host_tests.cpp
)
target_include_directories(pnx_usb_contract_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usb/src"
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
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME pwm_contract COMMAND pnx_pwm_contract_host_tests)

# host 沒有 HAL stub，故 fake 只能鏡像 BSP 契約；靜態檢查防止兩邊悄悄分岔。
add_test(
    NAME pwm_multichannel_source_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_PWM_SOURCE=${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src/bsp_pwm.cpp"
        "-DPNX_PWM_CHANNELS=${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src/pwm_channels.hpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/pwm_multichannel_source_contract.cmake"
)

add_executable(pnx_pwm_a2_sequence_tests
    pwm_a2_sequence_tests.cpp
)
target_include_directories(pnx_pwm_a2_sequence_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/demo/cboard/pwm_a2"
)
add_test(NAME pwm_a2_sequence COMMAND pnx_pwm_a2_sequence_tests)

add_executable(pnx_chassis_kinematics_tests
    chassis_kinematics_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/kinematics.cpp"
)
target_include_directories(pnx_chassis_kinematics_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME chassis_kinematics COMMAND pnx_chassis_kinematics_tests)

add_executable(pnx_chassis_manual_safety_tests
    chassis_manual_safety_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/manual_control.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/safety_gate.cpp"
)
target_include_directories(pnx_chassis_manual_safety_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME chassis_manual_safety COMMAND pnx_chassis_manual_safety_tests)

add_executable(pnx_chassis_command_slew_tests
    chassis_command_slew_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/command_slew.cpp"
)
target_include_directories(pnx_chassis_command_slew_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME chassis_command_slew COMMAND pnx_chassis_command_slew_tests)

add_executable(pnx_chassis_controller_tests
    chassis_controller_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/command_slew.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/controller.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/kinematics.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/manual_control.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/safety_gate.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/velocity_pi.cpp"
)
target_include_directories(pnx_chassis_controller_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME chassis_controller COMMAND pnx_chassis_controller_tests)

add_executable(pnx_chassis_config_tests
    chassis_config_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/runtime/config.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/command_slew.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/controller.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/kinematics.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/manual_control.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/safety_gate.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/velocity_pi.cpp"
)
target_include_directories(pnx_chassis_config_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME chassis_config COMMAND pnx_chassis_config_tests)

add_executable(pnx_m2006_p36_contract_tests
    fake_can.cpp
    m2006_p36_contract_tests.cpp
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/motor/src/motorhandler.cpp"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/src/djimotors.cpp"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/src/djimotorhandler.cpp"
)
target_include_directories(pnx_m2006_p36_contract_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/include"
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/motor/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/math/include"
)
add_test(NAME m2006_p36_contract COMMAND pnx_m2006_p36_contract_tests)

add_executable(pnx_chassis_runtime_policy_tests
    chassis_runtime_policy_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/runtime/runtime_policy.cpp"
)
target_include_directories(pnx_chassis_runtime_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
    "${CMAKE_SOURCE_DIR}/pnx_modules/remoter/include"
)
add_test(NAME chassis_runtime_policy COMMAND pnx_chassis_runtime_policy_tests)

add_executable(pnx_arm_servo_map_tests
    arm_servo_map_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/servo_map.cpp"
)
target_include_directories(pnx_arm_servo_map_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME arm_servo_map COMMAND pnx_arm_servo_map_tests)

add_executable(pnx_arm_servo_pwm_output_tests
    arm_servo_pwm_output_tests.cpp
    fake_pwm.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/arm/runtime/servo_pwm_output.cpp"
)
target_include_directories(pnx_arm_servo_pwm_output_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME arm_servo_pwm_output COMMAND pnx_arm_servo_pwm_output_tests)

add_test(
    NAME chassis_runtime_source_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_RUNTIME_SOURCE=${CMAKE_SOURCE_DIR}/vehicle/chassis/runtime/runtime.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/chassis_runtime_source_contract.cmake"
)

add_executable(pnx_arm_position_pid_tests
    arm_position_pid_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/position_pid.cpp"
)
target_include_directories(pnx_arm_position_pid_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME arm_position_pid COMMAND pnx_arm_position_pid_tests)

add_executable(pnx_arm_j1_control_tests
    arm_j1_control_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/gravity_feedforward.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/j1_manual_command.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/j1_stall_guard.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/j1_zero_reference.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/position_pid.cpp"
)
target_include_directories(pnx_arm_j1_control_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME arm_j1_control COMMAND pnx_arm_j1_control_tests)

add_executable(pnx_arm_config_tests
    arm_config_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/j1_stall_guard.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/servo_map.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/runtime/arm_config.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/position_pid.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/chassis/control/velocity_pi.cpp"
)
target_include_directories(pnx_arm_config_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
add_test(NAME arm_config COMMAND pnx_arm_config_tests)

add_executable(pnx_arm_runtime_policy_tests
    arm_runtime_policy_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/arm/control/arm_safety_gate.cpp"
    "${CMAKE_SOURCE_DIR}/vehicle/arm/runtime/arm_runtime_policy.cpp"
)
target_include_directories(pnx_arm_runtime_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/can/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/include"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/pwm/src"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
    "${CMAKE_SOURCE_DIR}/pnx_modules/remoter/include"
)
add_test(NAME arm_runtime_policy COMMAND pnx_arm_runtime_policy_tests)

add_executable(pnx_combined_mode_router_tests
    combined_mode_router_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/combined/control/mode_router.cpp"
)
target_include_directories(pnx_combined_mode_router_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
    "${CMAKE_SOURCE_DIR}/pnx_modules/remoter/include"
)
add_test(NAME combined_mode_router COMMAND pnx_combined_mode_router_tests)

add_executable(pnx_combined_output_arbiter_tests
    combined_output_arbiter_tests.cpp
    "${CMAKE_SOURCE_DIR}/vehicle/combined/control/output_arbiter.cpp"
)
target_include_directories(pnx_combined_output_arbiter_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
)
add_test(NAME combined_output_arbiter
    COMMAND pnx_combined_output_arbiter_tests)

add_test(
    NAME combined_product_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/combined_product_contract.cmake"
)

add_test(
    NAME combined_runtime_source_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_RUNTIME_SOURCE=${CMAKE_SOURCE_DIR}/vehicle/combined/runtime/runtime.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/combined_runtime_source_contract.cmake"
)

add_test(
    NAME arm_runtime_source_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_RUNTIME_SOURCE=${CMAKE_SOURCE_DIR}/vehicle/arm/runtime/arm_runtime.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/arm_runtime_source_contract.cmake"
)

add_test(
    NAME arm_remoter_stack_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_REMOTER_HEADER=${CMAKE_SOURCE_DIR}/pnx_modules/remoter/include/remoter.hpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/arm_remoter_stack_contract.cmake"
)

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

add_test(
    NAME direct_bsp_source_layout
    COMMAND ${CMAKE_COMMAND}
        "-DPNX_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/direct_bsp_source_layout_contract.cmake"
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
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/src"
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
