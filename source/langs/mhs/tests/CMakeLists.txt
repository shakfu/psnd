# MHS (MicroHaskell) language tests
#
# Only build tests when MHS integration is enabled

if(NOT ENABLE_MHS_INTEGRATION)
    message(STATUS "MHS tests: Skipped (MHS integration disabled)")
    return()
endif()

# Helper macro for mhs tests
macro(add_mhs_test TEST_NAME)
    add_executable(test_mhs_${TEST_NAME}
        test_${TEST_NAME}.c
        ${PSND_ROOT_DIR}/source/testing/test_framework.c
    )
    target_include_directories(test_mhs_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/testing
        ${PSND_ROOT_DIR}/source/core/include
        ${PSND_ROOT_DIR}/source/core/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(test_mhs_${TEST_NAME} PRIVATE
        mhs
        shared
        $<$<NOT:$<PLATFORM_ID:Windows>>:m>
    )
    add_test(
        NAME mhs_${TEST_NAME}_tests
        COMMAND $<TARGET_FILE:test_mhs_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(mhs_${TEST_NAME}_tests PROPERTIES LABELS "unit")
endmacro()

# Add MHS tests
add_mhs_test(midi_ffi)
add_mhs_test(context)

# End-to-end smoke test: runs Smoke.hs through the psnd binary, exercising the
# VFS and the embedded base and music packages. The unit tests above never
# start the interpreter, so this is the only coverage of that path.
add_test(
    NAME mhs_smoke_tests
    COMMAND ${CMAKE_COMMAND}
        -DPSND=$<TARGET_FILE:psnd_bin>
        -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(mhs_smoke_tests PROPERTIES LABELS "integration" TIMEOUT 180)

# Regression test: a module that changed on disk must run its new code. The
# smoke test reruns one fixed file, so it cannot see a stale cache entry.
add_test(
    NAME mhs_reload_tests
    COMMAND ${CMAKE_COMMAND}
        -DPSND=$<TARGET_FILE:psnd_bin>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/reload
        -P ${CMAKE_CURRENT_SOURCE_DIR}/reload_test.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(mhs_reload_tests PROPERTIES LABELS "integration" TIMEOUT 180)
