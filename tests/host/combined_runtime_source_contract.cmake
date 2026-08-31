if(NOT DEFINED PNX_RUNTIME_SOURCE)
    message(FATAL_ERROR
        "combined_runtime_source_contract requires PNX_RUNTIME_SOURCE")
endif()
if(NOT EXISTS "${PNX_RUNTIME_SOURCE}")
    message(FATAL_ERROR "Combined runtime source is missing")
endif()

file(READ "${PNX_RUNTIME_SOURCE}" runtime_source)

string(REGEX MATCH
    "control_stack_bytes[ \t]*=[ \t]*([0-9]+)U"
    control_stack_match "${runtime_source}")
if(NOT control_stack_match OR CMAKE_MATCH_1 LESS 2048)
    message(FATAL_ERROR
        "Combined control stack must be declared at 2048 bytes or larger")
endif()

string(FIND "${runtime_source}" "void remote_ingest_entry(ULONG)"
    ingest_entry_index)
string(FIND "${runtime_source}" "void control_entry(ULONG)"
    control_entry_index)
if(ingest_entry_index EQUAL -1 OR control_entry_index EQUAL -1)
    message(FATAL_ERROR "Combined ingest/control entry point is missing")
endif()

string(REGEX MATCHALL "msg::read\\(" msg_read_matches "${runtime_source}")
list(LENGTH msg_read_matches msg_read_count)
if(NOT msg_read_count EQUAL 1)
    message(FATAL_ERROR
        "Combined runtime must have exactly one msg::read; found ${msg_read_count}")
endif()

string(REGEX MATCHALL "motor_handler\\.alive_check\\(\\)"
    alive_matches "${runtime_source}")
list(LENGTH alive_matches alive_count)
if(NOT alive_count EQUAL 1)
    message(FATAL_ERROR
        "Combined runtime must have exactly one alive_check call; found ${alive_count}")
endif()

string(REGEX MATCHALL "register_motor\\(" registration_matches
    "${runtime_source}")
list(LENGTH registration_matches registration_count)
if(NOT registration_count EQUAL 5)
    message(FATAL_ERROR
        "Combined runtime must register exactly five motors; found ${registration_count}")
endif()

string(SUBSTRING "${runtime_source}" ${control_entry_index} -1 control_source)
string(FIND "${control_source}" "    for (;;)" control_loop_index)
if(control_loop_index EQUAL -1)
    message(FATAL_ERROR "Combined 200 Hz control loop is missing")
endif()
string(SUBSTRING "${control_source}" ${control_loop_index} -1
    control_loop_source)

string(FIND "${control_loop_source}" "input_adapter.update("
    adapter_update_index)
string(FIND "${control_loop_source}" "router.update("
    router_update_index)
if(adapter_update_index EQUAL -1 OR router_update_index EQUAL -1 OR
   NOT adapter_update_index LESS router_update_index)
    message(FATAL_ERROR
        "Combined runtime must adapt PS2 input before mode routing")
endif()
string(REGEX MATCHALL "motor_handler\\.send_control\\(\\)"
    loop_send_matches "${control_loop_source}")
list(LENGTH loop_send_matches loop_send_count)
if(NOT loop_send_count EQUAL 1)
    message(FATAL_ERROR
        "Combined control loop must contain exactly one CAN send; found ${loop_send_count}")
endif()

foreach(required_token IN ITEMS
        "front_left"
        "front_right"
        "rear_left"
        "rear_right"
        "j1_motor"
        "chassis_policy.update("
        "arm_policy.update("
        "chassis_controller.update("
        "j1_hold.update("
        "j1_zero.capture("
        "j1_stall.update("
        "j1_manual.update("
        "gravity_current_raw("
        "combine_current_raw("
        "servo_control.update("
        "j2_pwm.update("
        "j3_pwm.update("
        "j4_pwm.update("
        "router.update("
        "select_outputs("
        "copy_remote_snapshot()")
    string(FIND "${runtime_source}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Combined runtime is missing required token: ${required_token}")
    endif()
endforeach()

foreach(required_token IN ITEMS
        "remote_config.ps2.thread_priority"
        "remote_config.ps2.receiver_offline_timeout_ticks"
        "remote_config.ps2.frame_timeout_ticks"
        "remote_config.ps2.deadzone"
        "next_telemetry.active_source"
        "next_telemetry.ps2_link"
        "next_telemetry.ps2_buttons"
        "next_telemetry.ps2_pressed"
        "next_telemetry.ps2_unlocked"
        "next_telemetry.r1_chassis_held"
        "next_telemetry.r2_arm_held")
    string(FIND "${runtime_source}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Combined PS2 runtime is missing token: ${required_token}")
    endif()
endforeach()

foreach(forbidden_token IN ITEMS
        "vehicle::mycar::run("
        "vehicle::arm::run("
        "control_remote.wheel")
    string(FIND "${runtime_source}" "${forbidden_token}" token_index)
    if(NOT token_index EQUAL -1)
        message(FATAL_ERROR
            "Combined runtime contains forbidden token: ${forbidden_token}")
    endif()
endforeach()
