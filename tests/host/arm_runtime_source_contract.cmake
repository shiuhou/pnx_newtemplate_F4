# 靜態 contract：arm control thread 不可直接讀訊息；DR16 由 ingest thread 取樣後交給快照。
if(NOT DEFINED PNX_RUNTIME_SOURCE)
    message(FATAL_ERROR
        "arm_runtime_source_contract requires PNX_RUNTIME_SOURCE")
endif()

file(READ "${PNX_RUNTIME_SOURCE}" runtime_source)

string(FIND "${runtime_source}" "void remote_ingest_entry(ULONG)"
    ingest_entry_index)
string(FIND "${runtime_source}" "void control_entry(ULONG)"
    control_entry_index)
string(FIND "${runtime_source}" "msg::read(" msg_read_index)
string(REGEX MATCHALL "msg::read\\(" msg_read_matches "${runtime_source}")
list(LENGTH msg_read_matches msg_read_count)

if(ingest_entry_index EQUAL -1 OR control_entry_index EQUAL -1)
    message(FATAL_ERROR "Arm runtime ingest/control entry point is missing")
endif()
if(NOT msg_read_count EQUAL 1)
    message(FATAL_ERROR
        "Arm runtime must contain exactly one msg::read; found ${msg_read_count}")
endif()
if(msg_read_index LESS ingest_entry_index OR
   NOT msg_read_index LESS control_entry_index)
    message(FATAL_ERROR
        "The only msg::read must remain inside remote_ingest_entry")
endif()

string(SUBSTRING "${runtime_source}" ${control_entry_index} -1
    control_source)
string(FIND "${control_source}" "    for (;;)" control_loop_index)
if(control_loop_index EQUAL -1)
    message(FATAL_ERROR "Arm runtime control loop is missing")
endif()
string(SUBSTRING "${control_source}" ${control_loop_index} -1
    control_loop_source)

string(FIND "${control_loop_source}"
    "const auto remote_snapshot = copy_remote_snapshot();"
    snapshot_copy_index)
string(FIND "${control_loop_source}" "const std::uint32_t now ="
    now_sample_index)
if(snapshot_copy_index EQUAL -1 OR now_sample_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime snapshot-copy/time-sample contract is missing")
endif()
if(NOT snapshot_copy_index LESS now_sample_index)
    message(FATAL_ERROR
        "Arm control loop must copy the remote snapshot before sampling now")
endif()

string(FIND "${control_loop_source}"
    "const auto controller_safety = controller_safety_for(policy_output);"
    controller_safety_index)
string(FIND "${control_loop_source}"
    "const auto controller_state = runtime_safety.update(controller_safety);"
    controller_safety_use_index)
if(controller_safety_index EQUAL -1 OR controller_safety_use_index EQUAL -1 OR
   NOT controller_safety_index LESS controller_safety_use_index)
    message(FATAL_ERROR
        "Arm runtime must route runtime policy through arm_safety_gate")
endif()

string(FIND "${control_loop_source}"
    "if (trusted_release_observed(policy_output))"
    trusted_release_index)
if(trusted_release_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime must expose a trusted release path to controller reset")
endif()

string(REGEX MATCHALL "register_motor\\(j1_motor\\)" registration_matches
    "${runtime_source}")
list(LENGTH registration_matches registration_count)
if(NOT registration_count EQUAL 1)
    message(FATAL_ERROR
        "Arm runtime must register exactly one J1 motor; found ${registration_count}")
endif()

foreach(required_call IN ITEMS
        "j1_zero.capture("
        "j1_stall.update("
        "j1_manual.update("
        "gravity_current_raw("
        "combine_current_raw("
        "j1_hold.update("
        "should_hold_j2_output(j2_pwm.enabled(), policy_output)"
        "should_hold_j2_output(j3_pwm.enabled(), policy_output)"
        "should_hold_j2_output(j4_pwm.enabled(), policy_output)"
        "j1_motor.get_feedback()"
        "j1_motor.set_current(current_raw)"
        "j1_motor.relax()"
        "j2_pwm.update("
        "j3_pwm.update("
        "j4_pwm.update("
        "servo_control.update("
        "motor_handler.send_control()")
    string(FIND "${runtime_source}" "${required_call}" required_call_index)
    if(required_call_index EQUAL -1)
        message(FATAL_ERROR
            "Arm runtime is missing required J1 call: ${required_call}")
    endif()
endforeach()

foreach(required_state IN ITEMS
        "arm_outputs_enabled"
        "j1_outputs_enabled"
        "j2_control_enabled"
        "j3_control_enabled"
        "j4_control_enabled")
    string(FIND "${runtime_source}" "${required_state}" required_state_index)
    if(required_state_index EQUAL -1)
        message(FATAL_ERROR
            "Arm runtime is missing split output state: ${required_state}")
    endif()
endforeach()

string(FIND "${runtime_source}"
    "j2_pwm.update(outputs_enabled" eager_j2_start_index)
if(NOT eager_j2_start_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime must not start J2 PWM merely because Arm is unlocked")
endif()

string(FIND "${runtime_source}" "direction_check_offset_rad"
    fixed_offset_index)
if(NOT fixed_offset_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime must not retain the fixed direction-check offset")
endif()

string(FIND "${runtime_source}" ".left_y" left_y_index)
if(left_y_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime must route remoter::state::left_y to J2")
endif()

string(FIND "${runtime_source}" ".left_x" left_x_index)
if(left_x_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime must route remoter::state::left_x to J4")
endif()

string(FIND "${runtime_source}" ".wheel" wheel_index)
if(NOT wheel_index EQUAL -1)
    message(FATAL_ERROR
        "Arm runtime must not depend on the unavailable DR16 wheel input")
endif()
