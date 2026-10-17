# Bog language tests

# Helper macro for bog tests
macro(add_bog_test TEST_NAME)
    add_executable(test_bog_${TEST_NAME} test_${TEST_NAME}.c)
    target_link_libraries(test_bog_${TEST_NAME} PRIVATE bog test_framework)
    target_include_directories(test_bog_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/langs/bog/impl
        ${PSND_ROOT_DIR}/source/testing
    )
    if(UNIX)
        target_link_libraries(test_bog_${TEST_NAME} PRIVATE m)
    endif()
    add_test(
        NAME bog_${TEST_NAME}
        COMMAND $<TARGET_FILE:test_bog_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(bog_${TEST_NAME} PROPERTIES LABELS "unit")
endmacro()

# Add all bog tests
add_bog_test(tokenizer)
add_bog_test(parser)
add_bog_test(unify)
add_bog_test(builtins)
add_bog_test(state_manager)
add_bog_test(resolution)
add_bog_test(livecoding)
add_bog_test(scheduler)

# bog_async.c is built only into the REPL, so this test compiles it directly.
# POSIX-only test code (clock_gettime, nanosleep).
if(UNIX)
    find_package(Threads REQUIRED)
    add_executable(test_bog_async test_async.c ${PSND_ROOT_DIR}/source/langs/bog/bog_async.c)
    target_link_libraries(test_bog_async PRIVATE bog test_framework Threads::Threads m)
    target_include_directories(test_bog_async PRIVATE
        ${PSND_ROOT_DIR}/source/langs/bog
        ${PSND_ROOT_DIR}/source/langs/bog/impl
        ${PSND_ROOT_DIR}/source/testing
    )
    add_test(
        NAME bog_async
        COMMAND $<TARGET_FILE:test_bog_async>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(bog_async PROPERTIES LABELS "unit")
endif()
