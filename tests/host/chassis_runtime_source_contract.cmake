# 靜態 contract：控制 thread 不可直接讀訊息；DR16 由 ingest thread 取樣後交給快照。
if(NOT DEFINED PNX_RUNTIME_SOURCE)
    message(FATAL_ERROR
        "chassis_runtime_source_contract requires PNX_RUNTIME_SOURCE")
endif()

file(READ "${PNX_RUNTIME_SOURCE}" runtime_source)

# Hardware fault evidence showed the control thread descending to within one
# exception frame of its 1024-byte stack boundary during message startup.
# Keep enough headroom for ThreadX bookkeeping and the control call chain.
string(REGEX MATCH
    "control_stack_bytes[ \t]*=[ \t]*([0-9]+)U"
    control_stack_match "${runtime_source}")
if(NOT control_stack_match)
    message(FATAL_ERROR "Runtime control stack declaration is missing")
endif()
if(CMAKE_MATCH_1 LESS 2048)
    message(FATAL_ERROR
        "Runtime control stack must be at least 2048 bytes; found ${CMAKE_MATCH_1}")
endif()

string(FIND "${runtime_source}" "void remote_ingest_entry(ULONG)"
    ingest_entry_index)
string(FIND "${runtime_source}" "void control_entry(ULONG)"
    control_entry_index)
string(FIND "${runtime_source}" "msg::read(" msg_read_index)
string(REGEX MATCHALL "msg::read\\(" msg_read_matches "${runtime_source}")
list(LENGTH msg_read_matches msg_read_count)

if(ingest_entry_index EQUAL -1 OR control_entry_index EQUAL -1)
    message(FATAL_ERROR "Runtime ingest/control entry point is missing")
endif()
if(NOT msg_read_count EQUAL 1)
    message(FATAL_ERROR
        "Runtime must contain exactly one msg::read; found ${msg_read_count}")
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
    message(FATAL_ERROR "Runtime control loop is missing")
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
        "Runtime control loop snapshot-copy/time-sample contract is missing")
endif()
if(NOT snapshot_copy_index LESS now_sample_index)
    message(FATAL_ERROR
        "Control loop must copy the remote snapshot before sampling now")
endif()

string(FIND "${control_loop_source}"
    "const auto controller_safety = controller_safety_for(policy_output);"
    controller_safety_index)
string(FIND "${control_loop_source}"
    "controller_safety,"
    controller_safety_use_index)
if(controller_safety_index EQUAL -1 OR
   controller_safety_use_index EQUAL -1 OR
   NOT controller_safety_index LESS controller_safety_use_index)
    message(FATAL_ERROR
        "Runtime must inhibit the controller with runtime policy faults")
endif()

string(FIND "${control_loop_source}"
    "if (trusted_release_observed(policy_output))"
    trusted_release_index)
if(trusted_release_index EQUAL -1)
    message(FATAL_ERROR
        "Runtime must expose a trusted release path to controller reset")
endif()

string(REGEX MATCHALL "latch_overrun\\(\\)" overrun_latch_calls
    "${control_loop_source}")
list(LENGTH overrun_latch_calls overrun_latch_count)
if(NOT overrun_latch_count EQUAL 4)
    message(FATAL_ERROR
        "Runtime must reset controller state on all four overrun checks; found ${overrun_latch_count}")
endif()

string(REGEX MATCHALL "motor_handler\\.send_control\\(\\)"
    control_send_calls "${control_loop_source}")
list(LENGTH control_send_calls control_send_count)
if(NOT control_send_count EQUAL 1)
    message(FATAL_ERROR
        "Control loop must contain exactly one handler send; found ${control_send_count}")
endif()

string(REGEX MATCHALL "register_motor\\(" registration_calls
    "${runtime_source}")
list(LENGTH registration_calls registration_count)
if(NOT registration_count EQUAL 4)
    message(FATAL_ERROR
        "Runtime must contain exactly four one-shot registrations; found ${registration_count}")
endif()
