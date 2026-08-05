if(NOT DEFINED PNX_REMOTER_HEADER)
    message(FATAL_ERROR
        "arm_remoter_stack_contract requires PNX_REMOTER_HEADER")
endif()

file(READ "${PNX_REMOTER_HEADER}" remoter_header)

string(FIND "${remoter_header}" "class dr16" dr16_start)
string(FIND "${remoter_header}" "class vt03" dr16_end)
string(FIND "${remoter_header}" "class service" service_start)
if(dr16_start EQUAL -1 OR dr16_end EQUAL -1 OR service_start EQUAL -1)
    message(FATAL_ERROR "Remoter class boundaries are missing")
endif()

math(EXPR dr16_length "${dr16_end} - ${dr16_start}")
string(SUBSTRING "${remoter_header}" ${dr16_start} ${dr16_length}
    dr16_source)
string(SUBSTRING "${remoter_header}" ${service_start} -1
    service_source)

foreach(component IN ITEMS dr16 service)
    string(FIND "${${component}_source}" "stack_[1536]"
        stack_size_index)
    if(stack_size_index EQUAL -1)
        message(FATAL_ERROR
            "${component} ThreadX stack must be 1536 bytes")
    endif()
endforeach()
