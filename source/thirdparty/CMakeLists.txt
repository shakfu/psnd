# Thirdparty dependencies for Loki

# Save parent project's BUILD_TESTING before thirdparty libs override it
set(PSND_BUILD_TESTING ${BUILD_TESTING})

# Lua 5.5.0 - Embedded scripting language
add_subdirectory(lua-5.5.0)

# tomlc99 - TOML configuration file parser (MIT License)
# Used for declarative config files (.psnd/config.toml, themes/*.toml)
add_subdirectory(tomlc99)

# libremidi - Cross-platform MIDI I/O (C++ library)
# CMP0077: option() honors normal variables - ensures our settings aren't overwritten
cmake_policy(SET CMP0077 NEW)
set(LIBREMIDI_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBREMIDI_TESTS OFF CACHE BOOL "" FORCE)
# Disable JACK backend - it requires jack_get_version() which is missing in older
# JACK headers (pre-1.9.10). ALSA is the primary MIDI backend on Linux anyway.
set(LIBREMIDI_NO_JACK ON)
set(LIBREMIDI_NO_JACK ON CACHE BOOL "" FORCE)
# Disable WinUWP and WinMIDI backends - they require cppwinrt headers.
# WinMM is sufficient for MIDI I/O on Windows.
set(LIBREMIDI_NO_WINUWP ON CACHE BOOL "" FORCE)
set(LIBREMIDI_NO_WINMIDI ON CACHE BOOL "" FORCE)
add_subdirectory(libremidi)

# libuv - Async I/O (used by alda for async playback)
set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libuv)

# Note: libalda is now built from src/alda/ in main CMakeLists.txt

# Ableton Link - Music synchronization library (GPL v2+)
# Creates Ableton::Link interface target
include(${CMAKE_CURRENT_SOURCE_DIR}/link-3.1.5/AbletonLinkConfig.cmake)
# Creates abl_link static library (C wrapper for Link)
include(${CMAKE_CURRENT_SOURCE_DIR}/link-3.1.5/extensions/abl_link/abl_link.cmake)

# midifile - Standard MIDI File library (BSD-2-Clause)
set(BUILD_MIDILIBRARY_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory(midifile)

# Csound - Sound synthesis engine (optional)
# Provides advanced synthesis beyond TinySoundFont's sample playback
option(BUILD_CSOUND_BACKEND "Build with Csound synthesis backend" OFF)
if(BUILD_CSOUND_BACKEND)
    message(STATUS "Building with Csound backend (minimal configuration)")

    # Build libsndfile from source with minimal configuration
    message(STATUS "Building libsndfile (minimal, no external codecs)")
    set(ENABLE_EXTERNAL_LIBS OFF CACHE BOOL "" FORCE)  # No FLAC, Vorbis, Opus
    set(ENABLE_MPEG OFF CACHE BOOL "" FORCE)           # No MP3
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_BOW_DOCS OFF CACHE BOOL "" FORCE)
    set(ENABLE_PACKAGE_CONFIG OFF CACHE BOOL "" FORCE)
    set(INSTALL_PKGCONFIG_MODULE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)     # Static library
    # Linux needs _GNU_SOURCE for S_ISSOCK macro in file_io.c
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_compile_definitions(_GNU_SOURCE)
    endif()
    # Build with PIC in case Csound shared library is built
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
    add_subdirectory(libsndfile)

    # Configure Csound to find our libsndfile
    # Set paths before Csound's find_library/find_path calls
    set(SNDFILE_H_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libsndfile/include CACHE PATH "" FORCE)
    # For the library, we'll use the target directly via target_link_libraries later
    # But Csound needs a path for find_library - point to build output
    if(MSVC)
        # MSVC uses .lib extension; for multi-config generators (Visual Studio), use Release path
        set(LIBSNDFILE_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/libsndfile/Release/sndfile.lib" CACHE FILEPATH "" FORCE)
    else()
        set(LIBSNDFILE_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/libsndfile/libsndfile.a" CACHE FILEPATH "" FORCE)
    endif()
    # Also set the include directory for Csound's include_directories call
    set(LIBSNDFILE_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libsndfile/include CACHE PATH "" FORCE)

    # Skip external codec libraries on Windows - they're built into libsndfile
    if(WIN32)
        set(FLAC_LIB "" CACHE STRING "" FORCE)
        set(OGG_LIB "" CACHE STRING "" FORCE)
        set(VORBIS_LIB "" CACHE STRING "" FORCE)
        set(VORBISENC_LIB "" CACHE STRING "" FORCE)
        set(SPEEX_LIB "" CACHE STRING "" FORCE)
    endif()

    # Minimal Csound build configuration
    # Set these before add_subdirectory so they take effect
    set(BUILD_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    # set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_UTILITIES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_INSTALLER OFF CACHE BOOL "" FORCE)
    set(USE_CURL OFF CACHE BOOL "" FORCE)
    set(USE_GETTEXT OFF CACHE BOOL "" FORCE)
    set(USE_GIT_COMMIT OFF CACHE BOOL "" FORCE)
    set(BUILD_DSSI_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_OSC_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_DEPRECATED_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_JACK_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_PYTHON_INTERFACE OFF CACHE BOOL "" FORCE)
    set(BUILD_PYTHON_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_JAVA_INTERFACE OFF CACHE BOOL "" FORCE)
    set(BUILD_LUA_INTERFACE OFF CACHE BOOL "" FORCE)
    set(BUILD_LUA_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_FLTK_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_VIRTUAL_KEYBOARD OFF CACHE BOOL "" FORCE)
    set(BUILD_FLUID_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_STK_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_WEBSOCKET_OPCODE OFF CACHE BOOL "" FORCE)
    set(BUILD_HDF5_OPCODES OFF CACHE BOOL "" FORCE)
    set(BUILD_CSBEATS OFF CACHE BOOL "" FORCE)
    set(USE_DOUBLE ON CACHE BOOL "" FORCE)
    set(BUILD_MULTI_CORE ON CACHE BOOL "" FORCE)
    set(USE_PORTMIDI OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_MODULES ON CACHE BOOL "" FORCE)
    set(USE_MP3 OFF CACHE BOOL "" FORCE)  # libsndfile built without MP3
    # set(DEFAULT_USER_PLUGINDIR "${CMAKE_SOURCE_DIR}/.psnd/csound/plugins" CACHE STRING "" FORCE)

    # Fix for modern Clang: NaN/Infinity warnings are errors with fast-math
    # Csound uses NaN in a few places, so we need to allow it
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-nan-infinity-disabled)
    endif()

    # Windows: provide dirent.h and POSIX compatibility headers for Csound
    if(MSVC)
        set(DIRENT_H "${CMAKE_CURRENT_SOURCE_DIR}/dirent/dirent.h" CACHE FILEPATH "" FORCE)
        list(APPEND CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/dirent")
        include_directories("${CMAKE_CURRENT_SOURCE_DIR}/dirent")

        # Windows: use winflexbison for Csound parser generation
        set(WINFLEXBISON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/winflexbison")
        set(WINFLEX_EXE "${WINFLEXBISON_DIR}/win_flex.exe")
        set(WINBISON_EXE "${WINFLEXBISON_DIR}/win_bison.exe")

        # Download winflexbison if not present
        if(NOT EXISTS "${WINFLEX_EXE}" OR NOT EXISTS "${WINBISON_EXE}")
            set(WINFLEXBISON_URL "https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip")
            set(WINFLEXBISON_ZIP "${CMAKE_CURRENT_BINARY_DIR}/winflexbison.zip")
            message(STATUS "Downloading winflexbison...")
            file(DOWNLOAD "${WINFLEXBISON_URL}" "${WINFLEXBISON_ZIP}" STATUS download_status)
            list(GET download_status 0 status_code)
            if(NOT status_code EQUAL 0)
                message(FATAL_ERROR "Failed to download winflexbison: ${download_status}")
            endif()
            file(MAKE_DIRECTORY "${WINFLEXBISON_DIR}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xf "${WINFLEXBISON_ZIP}"
                WORKING_DIRECTORY "${WINFLEXBISON_DIR}"
                RESULT_VARIABLE extract_result
            )
            if(NOT extract_result EQUAL 0)
                message(FATAL_ERROR "Failed to extract winflexbison")
            endif()
            file(REMOVE "${WINFLEXBISON_ZIP}")
            message(STATUS "winflexbison downloaded and extracted")
        endif()

        set(FLEX_EXECUTABLE "${WINFLEX_EXE}" CACHE FILEPATH "" FORCE)
        set(BISON_EXECUTABLE "${WINBISON_EXE}" CACHE FILEPATH "" FORCE)

        # Windows: force-include POSIX compatibility header for S_ISDIR etc.
        add_compile_options(/FI"${CMAKE_CURRENT_SOURCE_DIR}/dirent/posix_compat.h")
    endif()

    add_subdirectory(csound-6.18.1)

    # Ensure sndfile is built before Csound links against it
    if(TARGET CsoundLib64-static)
        add_dependencies(CsoundLib64-static sndfile)
    endif()
    if(TARGET csound64-static)
        add_dependencies(csound64-static sndfile)
    endif()

    # Export Csound include directories for the main project
    set(CSOUND_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/csound-6.18.1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/csound-6.18.1/H
        CACHE INTERNAL "Csound include directories"
    )
else()
    message(STATUS "Csound backend disabled (use -DBUILD_CSOUND_BACKEND=ON to enable)")
endif()

# TR7 Scheme interpreter
# Always build tr7 - it's small and needed for the tr7 language module
add_subdirectory(tr7)

# FluidSynth - Software SoundFont synthesizer (optional)
# Alternative to TinySoundFont with more features and better quality
option(BUILD_FLUID_BACKEND "Build with FluidSynth synthesis backend" OFF)
if(BUILD_FLUID_BACKEND)
    message(STATUS "Building with FluidSynth backend (lean configuration)")

    # Disable all audio/midi drivers - we use miniaudio for output
    set(enable-alsa OFF CACHE BOOL "" FORCE)
    set(enable-aufile OFF CACHE BOOL "" FORCE)
    set(enable-coreaudio OFF CACHE BOOL "" FORCE)
    set(enable-coremidi OFF CACHE BOOL "" FORCE)
    set(enable-dbus OFF CACHE BOOL "" FORCE)
    set(enable-dsound OFF CACHE BOOL "" FORCE)
    set(enable-ipv6 OFF CACHE BOOL "" FORCE)
    set(enable-jack OFF CACHE BOOL "" FORCE)
    set(enable-ladspa OFF CACHE BOOL "" FORCE)
    set(enable-libinstpatch OFF CACHE BOOL "" FORCE)
    set(enable-libsndfile OFF CACHE BOOL "" FORCE)
    set(enable-midishare OFF CACHE BOOL "" FORCE)
    set(enable-network OFF CACHE BOOL "" FORCE)
    set(enable-oboe OFF CACHE BOOL "" FORCE)
    set(enable-opensles OFF CACHE BOOL "" FORCE)
    set(enable-oss OFF CACHE BOOL "" FORCE)
    set(enable-pipewire OFF CACHE BOOL "" FORCE)
    set(enable-portaudio OFF CACHE BOOL "" FORCE)
    set(enable-pulseaudio OFF CACHE BOOL "" FORCE)
    set(enable-readline OFF CACHE BOOL "" FORCE)
    set(enable-sdl3 OFF CACHE BOOL "" FORCE)
    set(enable-wasapi OFF CACHE BOOL "" FORCE)
    set(enable-waveout OFF CACHE BOOL "" FORCE)
    set(enable-winmidi OFF CACHE BOOL "" FORCE)

    # Build as static library
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # Use C++11 for thread-safe locale instead of glib
    # This avoids the glib dependency for a leaner build
    set(osal "cpp11" CACHE STRING "" FORCE)

    # FindGCEM looks under ${CMAKE_SOURCE_DIR}/gcem (the psnd root), misses the
    # vendored copy, and downloads it from GitHub at configure time.
    set(GCEM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fluidsynth/gcem/include" CACHE PATH "" FORCE)

    add_subdirectory(fluidsynth)

    # Export FluidSynth include directories for the main project
    set(FLUIDSYNTH_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/fluidsynth/include
        ${CMAKE_CURRENT_BINARY_DIR}/fluidsynth/include
        CACHE INTERNAL "FluidSynth include directories"
    )
else()
    message(STATUS "FluidSynth backend disabled (use -DBUILD_FLUID_BACKEND=ON to enable)")
endif()

# Linenoise - Cross-platform line editing with tree-sitter syntax highlighting
# Provides REPL line editing with syntax highlighting for Lua
# This is the default REPL implementation (replaces readline/editline)
option(WITH_LINENOISE "Build with linenoise REPL support" ON)
if(WITH_LINENOISE)
    message(STATUS "Building with linenoise REPL (Lua syntax highlighting)")
    add_subdirectory(linenoise)
else()
    message(STATUS "Linenoise disabled - REPL will have no line editing")
endif()

# Webview - Cross-platform webview library (optional)
# Uses amalgamated single-header version at webview/webview_amalgamation.h
option(BUILD_WEBVIEW_HOST "Build with native webview host" OFF)
if(BUILD_WEBVIEW_HOST)
    message(STATUS "Building with webview native UI host (amalgamated header)")

    # Export webview include directory for the main project
    set(WEBVIEW_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/webview
        CACHE INTERNAL "Webview include directories"
    )
else()
    message(STATUS "Webview host disabled (use -DBUILD_WEBVIEW_HOST=ON to enable)")
endif()

# liblo - Open Sound Control library (optional)
# Enables OSC communication for remote control and inter-application messaging
option(BUILD_OSC "Build with OSC support via liblo" OFF)
if(BUILD_OSC)
    message(STATUS "Building with OSC support (liblo)")

    # Minimal liblo build - static library, no tools/tests/examples
    set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
    set(WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WITH_CPP_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_STATIC ON CACHE BOOL "" FORCE)
    set(THREADING ON CACHE BOOL "" FORCE)

    add_subdirectory(liblo/cmake liblo)

    # Export liblo include directories for the main project
    set(LIBLO_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/liblo
        ${CMAKE_CURRENT_BINARY_DIR}/liblo
        CACHE INTERNAL "liblo include directories"
    )
else()
    message(STATUS "OSC support disabled (use -DBUILD_OSC=ON to enable)")
endif()

# minihost - Minimal audio plugin host (VST3/AU) using JUCE (optional)
# Enables loading software instruments and effects plugins
option(BUILD_MINIHOST_BACKEND "Build with minihost VST/AU plugin support" OFF)
if(BUILD_MINIHOST_BACKEND)
    message(STATUS "Building with minihost plugin host (fetching JUCE...)")
    add_subdirectory(minihost)
else()
    message(STATUS "Minihost plugin backend disabled (use -DBUILD_MINIHOST_BACKEND=ON to enable)")
endif()

# Restore parent project's BUILD_TESTING
set(BUILD_TESTING ${PSND_BUILD_TESTING} CACHE BOOL "Build tests" FORCE)
