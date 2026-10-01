# loki editor tests

# Helper macro for loki tests
macro(add_loki_test TEST_NAME)
    add_executable(test_${TEST_NAME} test_${TEST_NAME}.c)
    target_include_directories(test_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/core/include
        ${PSND_ROOT_DIR}/source/core
        ${PSND_ROOT_DIR}/source/testing
        ${CMAKE_BINARY_DIR}/generated
        ${LUA_INCLUDE_DIR}
    )
    target_link_libraries(test_${TEST_NAME} PRIVATE libloki test_framework)
    # Apply LANG_* definitions so test struct sizes match library
    psnd_apply_lang_definitions(test_${TEST_NAME})
    add_test(NAME test_${TEST_NAME} COMMAND test_${TEST_NAME})
endmacro()

# Add all loki tests
add_loki_test(core)
add_loki_test(file_io)
add_loki_test(lua_api)
add_loki_test(lang_registration)
add_loki_test(modal)
add_loki_test(syntax)
add_loki_test(search)
add_loki_test(buffers)
add_loki_test(indent)
add_loki_test(undo)
add_loki_test(selection)
add_loki_test(command)
add_loki_test(command_handlers)
add_loki_test(terminal)
add_loki_test(row_operations)
add_loki_test(serialize)
add_loki_test(async_queue)
add_loki_test(repl_line_editor)
add_loki_test(picker)
add_loki_test(lang_bridge)
add_loki_test(config)
add_loki_test(soundfont)
add_loki_test(json)
add_loki_test(jsonrpc)
add_loki_test(renderer)
add_loki_test(window)
