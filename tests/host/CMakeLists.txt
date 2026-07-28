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
