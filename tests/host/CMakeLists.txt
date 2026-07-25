find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME cubemx_production_gate
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/scripts/check_cubemx_production.py"
        --repo-root "${CMAKE_SOURCE_DIR}"
        --check-submodules-clean
)
set_tests_properties(cubemx_production_gate PROPERTIES
    FIXTURES_SETUP pnx_cubemx_provenance
)

add_executable(pnx_dji_protocol_host_tests
    dji_protocol_host_tests.cpp
)
target_include_directories(pnx_dji_protocol_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_devices/motors/dji/include"
)
add_test(NAME dji_protocol_runtime COMMAND pnx_dji_protocol_host_tests)

add_executable(pnx_usb_contract_host_tests
    fake_usb_backend.cpp
    usb_contract_host_tests.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usb/src/bsp_usb.cpp"
)
target_include_directories(pnx_usb_contract_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
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
)
    add_test(
        NAME "usb_${scenario}"
        COMMAND pnx_usb_contract_host_tests "${scenario}"
    )
endforeach()

add_executable(pnx_motor_arm_guard_host_tests
    motor_arm_guard_host_tests.cpp
)
target_include_directories(pnx_motor_arm_guard_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_modules/motor/src"
)
add_test(
    NAME motor_arm_guard_runtime
    COMMAND pnx_motor_arm_guard_host_tests
)

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

add_executable(pnx_usart_host_tests
    fake_usart_backend.cpp
    usart_host_tests.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/src/bsp_usart.cpp"
)
target_include_directories(pnx_usart_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
target_link_libraries(pnx_usart_host_tests PRIVATE Threads::Threads)
foreach(scenario IN ITEMS
    block_tx_count
    it_tx_count
    dma_tx_count
    start_rx_rollback
    concurrent_rx_count
    offset_delivery
    tx_failure_telemetry
    rx_delivery_contract
)
    add_test(
        NAME "usart_${scenario}"
        COMMAND pnx_usart_host_tests "${scenario}"
    )
endforeach()

add_executable(pnx_usart_backend_utility_tests
    usart_backend_utility_tests.cpp
)
target_include_directories(pnx_usart_backend_utility_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${CMAKE_SOURCE_DIR}/pnx_bsp/usart/include"
    "${CMAKE_SOURCE_DIR}/pnx_libs/common/include"
)
target_link_libraries(
    pnx_usart_backend_utility_tests PRIVATE Threads::Threads
)
add_test(
    NAME usart_backend_utility
    COMMAND pnx_usart_backend_utility_tests
)

add_executable(pnx_diagnostics_host_tests
    diagnostics_host_tests.cpp
    "${CMAKE_SOURCE_DIR}/pnx_bsp/diagnostics/src/bsp_diagnostics.cpp"
)
target_include_directories(pnx_diagnostics_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_bsp/diagnostics/include"
    "${CMAKE_SOURCE_DIR}/demo/cboard/common"
    "${CMAKE_SOURCE_DIR}/boards/dji_c_board_f407/pnx_backends"
)
add_test(NAME diagnostics_runtime COMMAND pnx_diagnostics_host_tests)

add_executable(pnx_referee_rx_ring_host_tests
    referee_rx_ring_host_tests.cpp
)
target_include_directories(pnx_referee_rx_ring_host_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/pnx_modules/referee/include"
)
add_test(
    NAME referee_rx_ring_runtime
    COMMAND pnx_referee_rx_ring_host_tests
)

add_test(
    NAME cubemx_gate_unit
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cubemx_gate_unit_tests.py"
)

add_test(
    NAME f407_usb_source_acceptance
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/f407_usb_source_tests.py"
)

add_test(
    NAME cdc_harness_runtime
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cdc_harness_tests.py"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/f407_export_unit_tests.py")
    add_test(
        NAME f407_export_unit
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/f407_export_unit_tests.py"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/release/f407-only-provenance.json")
    add_test(
        NAME f407_only_candidate_gate
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/check_f407_only.py"
            --repo-root "${CMAKE_SOURCE_DIR}"
    )
endif()

get_property(PNX_HOST_TEST_NAMES DIRECTORY PROPERTY TESTS)
foreach(PNX_HOST_TEST_NAME IN LISTS PNX_HOST_TEST_NAMES)
    if(NOT PNX_HOST_TEST_NAME STREQUAL "cubemx_production_gate")
        set_tests_properties("${PNX_HOST_TEST_NAME}" PROPERTIES
            FIXTURES_REQUIRED pnx_cubemx_provenance
        )
    endif()
endforeach()

get_filename_component(
    PNX_HOST_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY
)
set_tests_properties(${PNX_HOST_TEST_NAMES} PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${PNX_HOST_TOOLCHAIN_BIN}"
)
