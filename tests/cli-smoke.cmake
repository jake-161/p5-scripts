# These cases must finish before SDK initialization or camera access.
function(check_cli expected_status expected_output)
    execute_process(
        COMMAND "${P5_CLI}" ${ARGN}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 10
    )
    if(NOT "${status}" STREQUAL "${expected_status}")
        message(FATAL_ERROR "Arguments '${ARGN}': expected exit ${expected_status}, got ${status}\n${stdout}${stderr}")
    endif()
    if(NOT "${stdout}${stderr}" MATCHES "${expected_output}")
        message(FATAL_ERROR "Arguments '${ARGN}': missing '${expected_output}'\n${stdout}${stderr}")
    endif()
    if("${stdout}${stderr}" MATCHES "CameraSDK version")
        message(FATAL_ERROR "Arguments '${ARGN}' unexpectedly initialized the SDK")
    endif()
endfunction()

check_cli(0 "Usage:" --help)
check_cli(0 "Usage:" -h)
check_cli(1 "missing required arguments")
check_cli(1 "unknown command: invalid" 127.0.0.1 invalid)
check_cli(1 "set-property requires a property id and value" 127.0.0.1 set-property 120)
check_cli(1 "download-image requires an image id or filename" 127.0.0.1 download-image)
check_cli(1 "set-ip requires a value" 127.0.0.1 set-ip)
