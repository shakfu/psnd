# Alda language tests

# Helper macro for alda tests
macro(add_alda_test TEST_NAME)
    add_executable(test_alda_${TEST_NAME}
        test_${TEST_NAME}.c
        ${PSND_ROOT_DIR}/source/testing/test_framework.c
    )
    target_link_libraries(test_alda_${TEST_NAME} PRIVATE alda $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
    target_include_directories(test_alda_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/testing
    )
    # Absolute path to the test fixtures, so tests work from any build
    # directory rather than only an in-tree ./build.
    target_compile_definitions(test_alda_${TEST_NAME} PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )
    add_test(
        NAME alda_${TEST_NAME}_tests
        COMMAND $<TARGET_FILE:test_alda_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(alda_${TEST_NAME}_tests PROPERTIES LABELS "unit")
endmacro()

# Add all alda tests
add_alda_test(scanner)
add_alda_test(interpreter)
add_alda_test(parser)
add_alda_test(parser_fuzz)
add_alda_test(backends)
add_alda_test(integration)
add_alda_test(microtuning)
add_alda_test(examples)

# The examples test parses every score in examples/ as a real-world corpus.
target_compile_definitions(test_alda_examples PRIVATE
    ALDA_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples"
)

# Apply Csound backend definition if enabled
if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(test_alda_backends PRIVATE BUILD_CSOUND_BACKEND)
endif()

# Set integration label for integration tests
set_tests_properties(alda_integration_tests PROPERTIES LABELS "integration")

# Csound microtuning manual test (only when Csound is enabled)
if(BUILD_CSOUND_BACKEND)
    add_executable(test_alda_csound_microtuning
        test_csound_microtuning.c
    )
    target_link_libraries(test_alda_csound_microtuning PRIVATE alda $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
    target_include_directories(test_alda_csound_microtuning PRIVATE
        ${PSND_ROOT_DIR}/source/testing
    )
    target_compile_definitions(test_alda_csound_microtuning PRIVATE BUILD_CSOUND_BACKEND)
endif()

# Shared suite test runner (Unix only - uses POSIX dirent.h)
if(NOT WIN32)
    add_executable(test_alda_shared_suite test_shared_suite.c)
    target_link_libraries(test_alda_shared_suite PRIVATE alda m)
    target_include_directories(test_alda_shared_suite PRIVATE
        ${PSND_ROOT_DIR}/source/testing
    )
    add_test(
        NAME alda_midi_test_suite
        COMMAND $<TARGET_FILE:test_alda_shared_suite> "${CMAKE_CURRENT_SOURCE_DIR}/shared_suite"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(alda_midi_test_suite PROPERTIES LABELS "integration")

    # Every example against Alda's own output (docs/dev/conformance.md)
    add_test(
        NAME alda_conformance_examples
        COMMAND $<TARGET_FILE:test_alda_shared_suite>
                "${CMAKE_CURRENT_SOURCE_DIR}/../examples"
                "${CMAKE_CURRENT_SOURCE_DIR}/alda_reference"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(alda_conformance_examples PROPERTIES LABELS "integration")
endif()
