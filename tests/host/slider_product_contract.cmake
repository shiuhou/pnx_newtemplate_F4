if(NOT DEFINED PNX_SOURCE_DIR)
    message(FATAL_ERROR
        "slider_product_contract requires PNX_SOURCE_DIR")
endif()

set(slider_robot
    "${PNX_SOURCE_DIR}/configs/vehicles/slider/robot.json")
set(slider_params
    "${PNX_SOURCE_DIR}/configs/vehicles/slider/params.json")
if(NOT EXISTS "${slider_robot}" OR NOT EXISTS "${slider_params}")
    message(FATAL_ERROR "Slider configuration is missing")
endif()

file(READ "${slider_robot}" robot_json)
string(JSON motor_count LENGTH "${robot_json}" devices motors list)
if(NOT motor_count EQUAL 1)
    message(FATAL_ERROR
        "Slider must define exactly one motor; found ${motor_count}")
endif()

foreach(field IN ITEMS name model can_bus can_id)
    string(JSON actual_${field} GET "${robot_json}"
        devices motors list 0 ${field})
endforeach()
if(NOT actual_name STREQUAL "slider" OR
   NOT actual_model STREQUAL "dji_m2006" OR
   NOT actual_can_bus STREQUAL "can1" OR
   NOT actual_can_id STREQUAL "0x205")
    message(FATAL_ERROR
        "Slider motor must be slider/dji_m2006/can1/0x205; found "
        "${actual_name}/${actual_model}/${actual_can_bus}/${actual_can_id}")
endif()

file(READ "${slider_params}" params_json)
string(JSON remoter_source GET "${params_json}" remoter source)
string(JSON remoter_uart GET "${params_json}" bindings remoter_uart)
if(NOT remoter_source STREQUAL "ps2" OR
   NOT remoter_uart STREQUAL "usart1")
    message(FATAL_ERROR
        "Slider remote must be PS2 on USART1; found "
        "${remoter_source}/${remoter_uart}")
endif()
