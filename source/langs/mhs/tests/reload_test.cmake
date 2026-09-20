# Regression test: an edited module must run its new code.
#
# MicroHs keys a cached module by the identifier the command line gave it, so a
# file passed by path is cached as "Reload.hs" while the cache validator looks
# for the declared name "Reload" and checks nothing. psnd therefore passes the
# module name and -i for its directory; this test fails if that stops happening.
#
# Invoked by CTest with -DPSND=<binary> -DWORK_DIR=<dir>.

if(NOT PSND OR NOT WORK_DIR)
    message(FATAL_ERROR "reload_test.cmake requires -DPSND and -DWORK_DIR")
endif()

# A directory of its own: the .mhscache it fills must not be the one the other
# MHS tests share.
file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

function(run_reload expected)
    execute_process(
        COMMAND ${PSND} mhs -r ${WORK_DIR}/Reload.hs
        WORKING_DIRECTORY ${WORK_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        TIMEOUT 120
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "psnd mhs exited with ${result}\nstdout:\n${out}\nstderr:\n${err}")
    endif()
    if(NOT out MATCHES "${expected}")
        message(FATAL_ERROR
            "expected output containing '${expected}'\ngot:\n${out}\nstderr:\n${err}")
    endif()
endfunction()

file(WRITE ${WORK_DIR}/Reload.hs
    "module Reload(main) where\nmain :: IO ()\nmain = putStrLn \"psnd-mhs-reload one\"\n")
run_reload("psnd-mhs-reload one")

file(WRITE ${WORK_DIR}/Reload.hs
    "module Reload(main) where\nmain :: IO ()\nmain = putStrLn \"psnd-mhs-reload two\"\n")
run_reload("psnd-mhs-reload two")

message(STATUS "MHS reload test passed")
