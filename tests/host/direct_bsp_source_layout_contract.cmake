if(NOT DEFINED PNX_SOURCE_DIR)
    message(FATAL_ERROR "PNX_SOURCE_DIR is required")
endif()

set(PNX_DIRECT_BSP_MODULES
    can
    diagnostics
    dwt
    exti
    flash
    indicator
    pwm
    spi
    usart
    usb
)

foreach(module IN LISTS PNX_DIRECT_BSP_MODULES)
    set(shared_source
        "${PNX_SOURCE_DIR}/pnx_bsp/${module}/src/bsp_${module}.cpp")
    if(NOT EXISTS "${shared_source}")
        message(FATAL_ERROR
            "Direct BSP source is missing from pnx_bsp: ${shared_source}")
    endif()

    set(parent_source
        "${PNX_SOURCE_DIR}/boards/dji_c_board_f407/bsp/bsp_${module}.cpp")
    if(EXISTS "${parent_source}")
        message(FATAL_ERROR
            "Direct BSP implementation still lives in the parent Board tree: "
            "${parent_source}")
    endif()
endforeach()

file(GLOB_RECURSE parent_bsp_files
    LIST_DIRECTORIES false
    "${PNX_SOURCE_DIR}/boards/dji_c_board_f407/bsp/*")
if(parent_bsp_files)
    list(JOIN parent_bsp_files ", " parent_bsp_file_list)
    message(FATAL_ERROR
        "Parent Board BSP directory must not regain implementation/helpers: "
        "${parent_bsp_file_list}")
endif()

file(GLOB_RECURSE direct_bsp_sources
    "${PNX_SOURCE_DIR}/pnx_bsp/*/src/*.cpp"
    "${PNX_SOURCE_DIR}/pnx_bsp/*/src/*.hpp"
    "${PNX_SOURCE_DIR}/pnx_bsp/*/src/*.h"
    "${PNX_SOURCE_DIR}/pnx_bsp/*/src/*.S")
foreach(source IN LISTS direct_bsp_sources)
    file(READ "${source}" source_text)
    if(source_text MATCHES "detail::backend_")
        message(FATAL_ERROR
            "Retired backend forwarding symbol found in ${source}")
    endif()
endforeach()
