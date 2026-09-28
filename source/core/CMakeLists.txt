# source/core/CMakeLists.txt - Core psnd libraries and binary

# Save core directory path for use in functions called from other directories
set(PSND_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Core source directory")

# ==============================================================================
# Music Theory Library (standalone, used by mhs standalone builds)
# ==============================================================================
add_library(music_theory STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/music/music_theory.c
)
target_include_directories(music_theory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/music
)

# ==============================================================================
# Shared Library
# ==============================================================================
set(LIBSHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/repl_commands.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/audio/tsf_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/audio/csound_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/audio/fluid_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/audio/minihost_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi/midi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi/midi_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi/events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/link/link.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/async/shared_async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/music/music_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/osc/osc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/param/param.c
)

add_library(shared STATIC ${LIBSHARED_SOURCES})
add_library(shared::shared ALIAS shared)

# Source tracking for playback visualization (stores source line in events)
option(SHARED_SOURCE_TRACKING "Enable source line tracking in shared async events" ON)
if(SHARED_SOURCE_TRACKING)
    target_compile_definitions(shared PUBLIC SHARED_SOURCE_TRACKING)
    message(STATUS "Shared: Source tracking enabled (for playback visualization)")
endif()

target_include_directories(shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/TinySoundFont
        ${PSND_ROOT_DIR}/source/thirdparty/miniaudio
        ${PSND_ROOT_DIR}/source/thirdparty/link-3.1.5/extensions/abl_link/include
)

target_link_libraries(shared PUBLIC libremidi abl_link uv_a Threads::Threads)

if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(shared PRIVATE BUILD_CSOUND_BACKEND)
    target_include_directories(shared PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/csound-6.18.1/include
        ${PSND_ROOT_DIR}/source/thirdparty/csound-6.18.1/H
        ${CMAKE_BINARY_DIR}/source/thirdparty/csound-6.18.1/include
    )
    if(APPLE)
        target_link_libraries(shared PRIVATE CsoundLib64-static)
    else()
        target_link_libraries(shared PRIVATE csound64-static)
    endif()
endif()

if(BUILD_FLUID_BACKEND)
    target_compile_definitions(shared PRIVATE BUILD_FLUID_BACKEND)
    target_include_directories(shared PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/fluidsynth/include
        ${CMAKE_BINARY_DIR}/source/thirdparty/fluidsynth/include
    )
    target_link_libraries(shared PRIVATE libfluidsynth)
endif()

if(BUILD_MINIHOST_BACKEND)
    target_compile_definitions(shared PRIVATE BUILD_MINIHOST_BACKEND)
    target_link_libraries(shared PRIVATE minihost)
    message(STATUS "Minihost plugin backend enabled")
endif()

if(BUILD_OSC)
    target_compile_definitions(shared PUBLIC PSND_OSC)
    target_include_directories(shared PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/liblo
        ${CMAKE_BINARY_DIR}/source/thirdparty/liblo
    )
    target_link_libraries(shared PRIVATE liblo_static)
endif()

# Platform-specific audio/MIDI frameworks (uses psnd_platform.cmake)
psnd_platform_link_audio_midi(shared PUBLIC)

# Linenoise support for shared library (theme command in REPL)
if(WITH_LINENOISE)
    target_compile_definitions(shared PUBLIC LOKI_USE_LINENOISE=1)
    target_include_directories(shared PUBLIC
        ${PSND_ROOT_DIR}/source/thirdparty/linenoise/include
    )
    target_link_libraries(shared PUBLIC linenoise-theme)
endif()

psnd_platform_add_warnings(shared)

# ==============================================================================
# Loki Library (built after languages are discovered)
# ==============================================================================
# Note: This function is called from source/langs/CMakeLists.txt after language discovery
function(psnd_build_loki)
    set(LOKI_LIBRARY_TYPE STATIC)
    if(LOKI_BUILD_SHARED)
        set(LOKI_LIBRARY_TYPE SHARED)
    endif()

    # Core loki sources
    set(LOKI_CORE_SOURCES
        ${PSND_CORE_DIR}/loki/core.c
        ${PSND_CORE_DIR}/loki/lua.c
        ${PSND_CORE_DIR}/loki/config.c
        ${PSND_CORE_DIR}/loki/theme_toml.c
        ${PSND_CORE_DIR}/loki/lang_toml.c
        ${PSND_CORE_DIR}/loki/keybind.c
        ${PSND_CORE_DIR}/loki/editor.c
        ${PSND_CORE_DIR}/loki/syntax.c
        ${PSND_CORE_DIR}/loki/indent.c
        ${PSND_CORE_DIR}/loki/languages.c
        ${PSND_CORE_DIR}/loki/selection.c
        ${PSND_CORE_DIR}/loki/search.c
        ${PSND_CORE_DIR}/loki/modal.c
        ${PSND_CORE_DIR}/loki/event.c
        ${PSND_CORE_DIR}/loki/renderer.c
        ${PSND_CORE_DIR}/loki/picker.c
        ${PSND_CORE_DIR}/loki/window.c
        ${PSND_CORE_DIR}/loki/command.c
        ${PSND_CORE_DIR}/loki/command/file.c
        ${PSND_CORE_DIR}/loki/command/basic.c
        ${PSND_CORE_DIR}/loki/command/goto.c
        ${PSND_CORE_DIR}/loki/command/substitute.c
        ${PSND_CORE_DIR}/loki/command/link.c
        ${PSND_CORE_DIR}/loki/command/csd.c
        ${PSND_CORE_DIR}/loki/command/export.c
        ${PSND_CORE_DIR}/loki/command/loop.c
        ${PSND_CORE_DIR}/loki/command/plugin.c
        ${PSND_CORE_DIR}/loki/command/theme.c
        ${PSND_CORE_DIR}/loki/command/metronome.c
        ${PSND_CORE_DIR}/loki/terminal_posix.c
        ${PSND_CORE_DIR}/loki/terminal_win.c
        ${PSND_CORE_DIR}/loki/undo.c
        ${PSND_CORE_DIR}/loki/buffers.c
        ${PSND_CORE_DIR}/loki/link.c
        ${PSND_CORE_DIR}/loki/csound.c
        ${PSND_CORE_DIR}/loki/export.c
        ${PSND_CORE_DIR}/loki/midi_export.cpp
        ${PSND_CORE_DIR}/loki/lang_bridge.c
        ${PSND_CORE_DIR}/loki/live_loop.c
        ${PSND_CORE_DIR}/loki/repl_launcher.c
        ${PSND_CORE_DIR}/loki/repl_helpers.c
        ${PSND_CORE_DIR}/loki/serialize.c
        ${PSND_CORE_DIR}/loki/session.c
        ${PSND_CORE_DIR}/loki/cli.c
        ${PSND_CORE_DIR}/loki/host.c
        ${PSND_CORE_DIR}/loki/json.c
        ${PSND_CORE_DIR}/loki/jsonrpc.c
        ${PSND_CORE_DIR}/loki/async_queue.c
    )

    # Web host sources (mongoose-based HTTP/WebSocket server)
    if(BUILD_WEB_HOST)
        list(APPEND LOKI_CORE_SOURCES
            ${PSND_CORE_DIR}/loki/host_web.c
            ${PSND_ROOT_DIR}/source/thirdparty/mongoose-7.20/mongoose.c
        )
    endif()

    # Webview host sources (native webview window)
    if(BUILD_WEBVIEW_HOST)
        list(APPEND LOKI_CORE_SOURCES
            ${PSND_CORE_DIR}/loki/host_webview.cpp
        )
    endif()

    # Linenoise REPL sources (tree-sitter syntax highlighting)
    if(WITH_LINENOISE)
        list(APPEND LOKI_CORE_SOURCES
            ${PSND_CORE_DIR}/loki/repl_line_editor.c
            ${PSND_CORE_DIR}/loki/repl_linenoise.c
            ${PSND_CORE_DIR}/loki/treesitter.c
        )
    endif()

    # Collect language-specific register sources
    psnd_collect_lang_sources()

    add_library(libloki ${LOKI_LIBRARY_TYPE}
        ${LOKI_CORE_SOURCES}
        ${PSND_ALL_LANG_REGISTER_SOURCES}
    )

    target_include_directories(libloki PUBLIC
        ${PSND_CORE_DIR}/include
        ${PSND_CORE_DIR}
        ${CMAKE_BINARY_DIR}/generated
        ${PSND_ROOT_DIR}/source/thirdparty/link-3.1.5/extensions/abl_link/include
        ${PSND_ROOT_DIR}/source/thirdparty/midifile/include
        ${PSND_ALL_LANG_INCLUDES}
    )

    # Both web hosts embed the vendored xterm.js
    if(BUILD_WEB_HOST OR BUILD_WEBVIEW_HOST)
        include(psnd_xterm)
        psnd_generate_xterm_header()
    endif()

    # Web host configuration (mongoose-based HTTP/WebSocket server)
    if(BUILD_WEB_HOST)
        target_compile_definitions(libloki PUBLIC LOKI_WEB_HOST)
        # Disable mongoose debug logging
        target_compile_definitions(libloki PRIVATE MG_ENABLE_LOG=0)
        target_include_directories(libloki PRIVATE
            ${PSND_ROOT_DIR}/source/thirdparty/mongoose-7.20
        )
    endif()

    # Webview host configuration (native webview window)
    # Uses amalgamated single-header webview library
    if(BUILD_WEBVIEW_HOST)
        target_compile_definitions(libloki PUBLIC LOKI_WEBVIEW_HOST)
        target_include_directories(libloki PRIVATE
            ${PSND_ROOT_DIR}/source/thirdparty/webview
        )
        # Platform-specific frameworks for webview
        if(APPLE)
            target_link_libraries(libloki PUBLIC "-framework WebKit")
        elseif(UNIX AND NOT APPLE)
            # Linux requires GTK and WebKitGTK
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
            pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.1)
            target_include_directories(libloki PRIVATE
                ${GTK3_INCLUDE_DIRS}
                ${WEBKIT2GTK_INCLUDE_DIRS}
            )
            target_link_libraries(libloki PUBLIC
                ${GTK3_LIBRARIES}
                ${WEBKIT2GTK_LIBRARIES}
            )
        endif()
    endif()

    # OSC support (propagate from shared library)
    if(BUILD_OSC)
        target_compile_definitions(libloki PUBLIC PSND_OSC)
        target_include_directories(libloki PRIVATE
            ${PSND_ROOT_DIR}/source/thirdparty/liblo
            ${CMAKE_BINARY_DIR}/source/thirdparty/liblo
        )
    endif()

    # Minihost plugin support (propagate from shared library)
    if(BUILD_MINIHOST_BACKEND)
        target_compile_definitions(libloki PUBLIC BUILD_MINIHOST_BACKEND)
    endif()

    # Lua sandboxing (disables os, io, debug, load* by default)
    if(LUA_SANDBOX)
        target_compile_definitions(libloki PRIVATE LUA_SANDBOX)
        message(STATUS "Lua sandbox enabled (os, io, debug, load* disabled)")
    else()
        message(STATUS "Lua sandbox disabled (full Lua access)")
    endif()

    psnd_platform_add_warnings(libloki)

    target_link_libraries(libloki PUBLIC
        lua
        tomlc99
        Threads::Threads
        ${CMAKE_DL_LIBS}
        abl_link
        midifile
        shared
        ${PSND_ALL_LANG_LINK_LIBRARIES}
    )

    # SQLite FTS5 search index plugin
    if(BUILD_PLUGIN_SQLITE)
        target_compile_definitions(libloki PUBLIC BUILD_PLUGIN_SQLITE)
        target_include_directories(libloki PRIVATE ${PSND_FTS_INCLUDE_DIR})
        target_link_libraries(libloki PUBLIC ${PSND_FTS_LIBRARY})
    endif()

    # Linenoise REPL support (tree-sitter syntax highlighting)
    if(WITH_LINENOISE)
        target_compile_definitions(libloki PUBLIC LOKI_USE_LINENOISE=1)
        target_include_directories(libloki PUBLIC
            ${PSND_ROOT_DIR}/source/thirdparty/linenoise/include
            ${PSND_ROOT_DIR}/source/thirdparty/tree-sitter/lib/include
        )
        target_link_libraries(libloki PUBLIC
            linenoise
            linenoise-theme
            linenoise-highlight-lua
            linenoise-highlight-alda
            linenoise-highlight-csound
            linenoise-highlight-joy
            linenoise-highlight-haskell
            linenoise-highlight-scheme
            tree-sitter
            tree-sitter-lua
            tree-sitter-alda
            tree-sitter-csound
            tree-sitter-joy
            tree-sitter-haskell
            tree-sitter-scheme
            tree-sitter-prolog
        )
    endif()

    psnd_apply_lang_definitions(libloki)
    set_target_properties(libloki PROPERTIES OUTPUT_NAME "loki")

    if(NOT MSVC)
        target_link_libraries(libloki PUBLIC m)
    endif()
endfunction()

# ==============================================================================
# psnd Binary (built after languages are discovered)
# ==============================================================================
function(psnd_build_binary)
    psnd_collect_lang_sources()

    # REPL implementation: use linenoise from libloki when enabled,
    # otherwise use the fallback repl.c
    set(PSND_BIN_REPL_SOURCES)
    if(NOT WITH_LINENOISE)
        list(APPEND PSND_BIN_REPL_SOURCES ${PSND_CORE_DIR}/repl.c)
    endif()

    add_executable(psnd_bin
        ${PSND_CORE_DIR}/main.c
        ${PSND_CORE_DIR}/lang_dispatch.c
        ${PSND_BIN_REPL_SOURCES}
        ${PSND_ALL_LANG_REPL_SOURCES}
    )

    set_target_properties(psnd_bin PROPERTIES 
        OUTPUT_NAME "psnd"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    if(NOT MSVC)
        add_custom_command(TARGET psnd_bin POST_BUILD
            COMMAND strip $<TARGET_FILE:psnd_bin>
            COMMENT "Stripping psnd binary"
        )
    endif()

    target_include_directories(psnd_bin PRIVATE
        ${PSND_ROOT_DIR}
        ${PSND_CORE_DIR}/include
        ${PSND_CORE_DIR}
        ${CMAKE_BINARY_DIR}/generated
        ${PSND_ALL_LANG_INCLUDES}
    )

    psnd_platform_add_warnings(psnd_bin)

    target_link_libraries(psnd_bin PRIVATE libloki)
    psnd_apply_lang_definitions(psnd_bin)

    # Web host configuration for the binary
    if(BUILD_WEB_HOST)
        target_compile_definitions(psnd_bin PRIVATE LOKI_WEB_HOST)
    endif()

    # Webview host configuration for the binary
    if(BUILD_WEBVIEW_HOST)
        target_compile_definitions(psnd_bin PRIVATE LOKI_WEBVIEW_HOST)
    endif()

    # Pass through FluidSynth backend flag for help text
    if(BUILD_FLUID_BACKEND)
        target_compile_definitions(psnd_bin PRIVATE BUILD_FLUID_BACKEND)
    endif()

    # Pass through Minihost backend flag for help text
    if(BUILD_MINIHOST_BACKEND)
        target_compile_definitions(psnd_bin PRIVATE BUILD_MINIHOST_BACKEND)
    endif()
endfunction()

# ==============================================================================
# Tests (built after psnd binary)
# ==============================================================================
function(psnd_build_core_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()

    add_test(NAME psnd_version COMMAND $<TARGET_FILE:psnd_bin> --version)
    add_test(NAME psnd_help COMMAND $<TARGET_FILE:psnd_bin> --help)

    add_subdirectory(${PSND_CORE_DIR}/tests/loki ${CMAKE_BINARY_DIR}/tests/loki)
    add_subdirectory(${PSND_CORE_DIR}/tests/shared ${CMAKE_BINARY_DIR}/tests/shared)
    add_subdirectory(${PSND_CORE_DIR}/tests/cli ${CMAKE_BINARY_DIR}/tests/cli)
    add_subdirectory(${PSND_CORE_DIR}/tests/tracker ${CMAKE_BINARY_DIR}/tests/tracker)
endfunction()
