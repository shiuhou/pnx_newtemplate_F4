cmake_minimum_required(VERSION 3.22)

enable_language(C ASM)

set(MX_Defines_Syms
    TX_INCLUDE_USER_DEFINE_FILE
    USE_HAL_DRIVER
    STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>
)
if(PNX_F407_USB_ENABLED)
    list(APPEND MX_Defines_Syms UX_INCLUDE_USER_DEFINE_FILE)
endif()

set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../AZURE_RTOS/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/inc
)
if(PNX_F407_USB_ENABLED)
    list(APPEND MX_Include_Dirs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App
        ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/Target
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/ports/generic/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_device_controllers
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_device_classes/inc
    )
endif()

# CubeMX-owned production closure generated from the minimal F407 IOC. The
# immutable RM26 full IOC lives under reference/ and is forbidden here.
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../AZURE_RTOS/App/app_azure_rtos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tx_initialize_low_level.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/app_threadx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/can.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f407xx.s
)
if(PNX_F407_USB_ENABLED)
    list(APPEND MX_Application_Src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App/ux_device_descriptors.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App/ux_device_cdc_acm.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App/app_usbx_device.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/usb_otg.c
    )
endif()

set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32f4xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)
if(PNX_F407_USB_ENABLED)
    list(APPEND STM32_Drivers_Src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
    )
endif()

if(PNX_F407_USB_ENABLED)
    file(GLOB USBX_Core_Src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/*.c)
    # Keep host/HCD/simulation/OTG-role and generic PCI helpers in the accepted
    # vendor/provenance tree, but never compile them into this device-only DCD
    # target. The PCI stubs also trigger invalid null-port diagnostics on
    # Cortex-M when optimization is enabled.
    list(FILTER USBX_Core_Src EXCLUDE REGEX
        "/ux_(host_|hcd_|dcd_sim_|system_otg_|utility_pci_).*\\.c$")
    file(GLOB USBX_CDC_Src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_*.c)
    file(GLOB USBX_DCD_Src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/*.c)
endif()

file(GLOB ThreadX_Common_Src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/*.c)
set(ThreadX_Port_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_context_restore.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_context_save.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_interrupt_control.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_schedule.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_stack_build.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_system_return.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_timer_interrupt.S
)

add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)

add_library(ThreadX OBJECT)
target_sources(ThreadX PRIVATE ${ThreadX_Common_Src} ${ThreadX_Port_Src})
target_link_libraries(ThreadX PUBLIC stm32cubemx)

if(PNX_F407_USB_ENABLED)
    add_library(USBX OBJECT)
    target_sources(USBX PRIVATE
        ${USBX_Core_Src}
        ${USBX_CDC_Src}
        ${USBX_DCD_Src}
    )
    target_link_libraries(USBX PUBLIC stm32cubemx)
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})
target_link_libraries(${CMAKE_PROJECT_NAME}
    STM32_Drivers
    ThreadX
    ${TOOLCHAIN_LINK_LIBRARIES}
)
if(PNX_F407_USB_ENABLED)
    target_link_libraries(${CMAKE_PROJECT_NAME} USBX)
endif()

set_target_properties(${CMAKE_PROJECT_NAME}
    PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
