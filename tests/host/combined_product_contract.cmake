if(NOT DEFINED PNX_SOURCE_DIR)
    message(FATAL_ERROR
        "combined_product_contract requires PNX_SOURCE_DIR")
endif()

set(combined_robot
    "${PNX_SOURCE_DIR}/configs/vehicles/mycar_combined/robot.json")
set(combined_params
    "${PNX_SOURCE_DIR}/configs/vehicles/mycar_combined/params.json")
set(combined_ps2_params
    "${PNX_SOURCE_DIR}/configs/vehicles/mycar_combined/params.ps2.json")
if(NOT EXISTS "${combined_robot}" OR NOT EXISTS "${combined_params}" OR
   NOT EXISTS "${combined_ps2_params}")
    message(FATAL_ERROR "Combined vehicle configuration is missing")
endif()

file(READ "${combined_ps2_params}" ps2_params_json)
string(JSON ps2_source GET "${ps2_params_json}" remoter source)
string(JSON ps2_uart GET "${ps2_params_json}" bindings remoter_uart)
string(JSON ps2_offline_timeout GET "${ps2_params_json}"
    remoter ps2_offline_timeout_ticks)
string(JSON ps2_frame_timeout GET "${ps2_params_json}"
    remoter ps2_frame_timeout_ticks)
string(JSON ps2_deadzone GET "${ps2_params_json}" remoter ps2_deadzone)
if(NOT ps2_source STREQUAL "ps2" OR NOT ps2_uart STREQUAL "usart1" OR
   NOT ps2_offline_timeout EQUAL 600 OR NOT ps2_frame_timeout EQUAL 20 OR
   NOT ps2_deadzone EQUAL 0.08)
    message(FATAL_ERROR "Combined PS2 configuration is not the approved profile")
endif()

file(READ "${combined_robot}" robot_json)
string(JSON motor_count LENGTH "${robot_json}" devices motors list)
if(NOT motor_count EQUAL 5)
    message(FATAL_ERROR
        "Combined robot must define exactly five motors; found ${motor_count}")
endif()

set(expected_names front_left front_right rear_left rear_right j1)
set(expected_ids 0x201 0x202 0x204 0x203 0x205)
foreach(index RANGE 0 4)
    string(JSON actual_name GET "${robot_json}"
        devices motors list ${index} name)
    string(JSON actual_id GET "${robot_json}"
        devices motors list ${index} can_id)
    list(GET expected_names ${index} expected_name)
    list(GET expected_ids ${index} expected_id)
    if(NOT actual_name STREQUAL expected_name OR
       NOT actual_id STREQUAL expected_id)
        message(FATAL_ERROR
            "Motor ${index} must be ${expected_name}/${expected_id}; "
            "found ${actual_name}/${actual_id}")
    endif()
endforeach()

file(READ "${PNX_SOURCE_DIR}/CMakeLists.txt" root_cmake)
file(READ "${PNX_SOURCE_DIR}/CMakePresets.json" presets)
file(READ "${PNX_SOURCE_DIR}/demo/app.cpp" app_source)

foreach(required_token IN ITEMS
        "PNX_ENABLE_MYCAR_COMBINED"
        "PNX_MYCAR_COMBINED_PS2"
        "PNX_APP_MYCAR_COMBINED"
        "configs/vehicles/mycar_combined/params.json"
        "configs/vehicles/mycar_combined/params.ps2.json"
        "pnx_modules/remoter/src/dr16.cpp"
        "pnx_modules/remoter/src/ps2.cpp"
        "configs/vehicles/mycar_combined/robot.json"
        "vehicle/combined.cpp"
        "vehicle/combined/runtime/runtime.cpp")
    string(FIND "${root_cmake}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Root CMake is missing combined token: ${required_token}")
    endif()
endforeach()

foreach(required_token IN ITEMS
        "f407-mycar-combined-debug"
        "f407-mycar-combined-ps2-debug"
        "PNX_MYCAR_COMBINED_PS2"
        "PNX_ENABLE_MYCAR_COMBINED")
    string(FIND "${presets}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "CMakePresets.json is missing combined token: ${required_token}")
    endif()
endforeach()

foreach(required_token IN ITEMS
        "PNX_APP_MYCAR_COMBINED"
        "vehicle/combined.hpp"
        "vehicle::combined::run()")
    string(FIND "${app_source}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "demo/app.cpp is missing combined token: ${required_token}")
    endif()
endforeach()
