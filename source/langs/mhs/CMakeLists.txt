# CMakeLists.txt for mhs-midi
# MHS (Micro Haskell) language integration for psnd
#
# This builds:
# 1. MHS language integration for psnd (via psnd_register_language)
# 2. Standalone mhs-midi REPL and example executables
#
# The MicroHs runtime is compiled straight from source/thirdparty/MicroHs.
# For the in-process psnd build, mhs-patch-eval.py rewrites eval.c at build time:
# it renames mmalloc/mrealloc/mcalloc to mhs_* to avoid a clash with Csound, and
# redirects the file and directory FFI entries to the VFS overrides.

# MHS integration requires fmemopen() which is not available on Windows
# Disable on Windows - the embedding system doesn't work without fmemopen
if(WIN32)
    set(ENABLE_MHS_INTEGRATION OFF CACHE BOOL "Enable MHS psnd integration" FORCE)
    message(STATUS "MHS: Disabled on Windows (requires fmemopen)")
else()
    option(ENABLE_MHS_INTEGRATION "Enable MHS psnd integration" ON)
endif()

# If MHS integration is disabled, we still need to provide stub init functions
# because the language discovery system adds MHS to the generated headers
if(NOT ENABLE_MHS_INTEGRATION)
    message(STATUS "MHS: psnd integration disabled (set -DENABLE_MHS_INTEGRATION=ON to enable)")

    # Create stub source file with empty init functions
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/mhs_stubs.c
        "/* MHS stub functions when integration is disabled */\n"
        "void mhs_dispatch_init(void) { /* MHS disabled */ }\n"
        "void mhs_loki_lang_init(void) { /* MHS disabled */ }\n"
    )

    # Register a minimal language that just provides the stubs
    psnd_register_language(
        NAME mhs
        DISPLAY_NAME "MHS"
        DESCRIPTION "MHS disabled - enable with -DENABLE_MHS_INTEGRATION=ON"
        COMMANDS mhs
        EXTENSIONS .mhs
        SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mhs_stubs.c
        REPL_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mhs_stubs.c
        REGISTER_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mhs_stubs.c
    )
    return()
endif()

# ==============================================================================
# Utility Macros
# ==============================================================================
# Strip symbols from Release builds (no-op for now, can be extended later)
macro(add_strip_command TARGET)
    # No-op - strip in Release builds if needed
endmacro()

# ==============================================================================
# MicroHs Paths and Compiler Setup
# ==============================================================================
set(MHS_DIR ${CMAKE_SOURCE_DIR}/source/thirdparty/MicroHs)
if(WIN32)
    set(MHS_COMPILER ${MHS_DIR}/bin/mhs.exe)
else()
    set(MHS_COMPILER ${MHS_DIR}/bin/mhs)
endif()
set(MHS_RUNTIME ${MHS_DIR}/src/runtime)
set(MHS_LIB ${MHS_DIR}/lib)

# Single source of truth for the vendored MicroHs version. Package file names
# (base-<version>.pkg) and the mcabal install layout are derived from it, so a
# version bump must not require editing this file.
file(STRINGS ${MHS_DIR}/MicroHs.cabal MHS_VERSION_LINE REGEX "^version:")
string(REGEX REPLACE "^version:[ \t]*" "" MHS_VERSION "${MHS_VERSION_LINE}")
string(STRIP "${MHS_VERSION}" MHS_VERSION)
if(NOT MHS_VERSION MATCHES "^[0-9]+(\\.[0-9]+)+$")
    message(FATAL_ERROR "MHS: cannot parse version from ${MHS_DIR}/MicroHs.cabal (got '${MHS_VERSION}')")
endif()
message(STATUS "MHS: vendored MicroHs version ${MHS_VERSION}")
set(MIDI_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)

# Build MicroHs compiler if it doesn't exist
if(NOT EXISTS ${MHS_COMPILER})
    message(STATUS "Building MicroHs compiler...")
    if(WIN32)
        execute_process(
            COMMAND nmake -f Makefile.windows
            WORKING_DIRECTORY ${MHS_DIR}
            RESULT_VARIABLE MHS_BUILD_RESULT
        )
    else()
        execute_process(
            COMMAND make
            WORKING_DIRECTORY ${MHS_DIR}
            RESULT_VARIABLE MHS_BUILD_RESULT
        )
    endif()
    if(NOT MHS_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build MicroHs compiler")
    endif()
endif()

# Verify MicroHs compiler exists after build attempt
if(NOT EXISTS ${MHS_COMPILER})
    message(FATAL_ERROR "MicroHs compiler not found at ${MHS_COMPILER}")
endif()

# ==============================================================================
# Conditional: psnd Integration (disabled by default due to symbol conflicts)
# ==============================================================================
if(ENABLE_MHS_INTEGRATION)

# ==============================================================================
# Embedded MHS Configuration for psnd
# ==============================================================================
# psnd embeds MHS libraries using VFS (Virtual File System)
#
# Options:
#   MHS_EMBED_MODE - Choose embedding mode:
#     PKG_ZSTD (default) - Precompiled packages + zstd compression (~2s startup)
#     PKG                - Precompiled packages, no compression (~2s startup)
#     SRC_ZSTD           - Compressed .hs sources (~17s startup, smaller binary)
#     SRC                - Uncompressed .hs sources (~17s startup)
#
#   MHS_ENABLE_COMPILATION - Enable compilation to standalone executables:
#     ON (default)       - Embed libremidi/libmidi_ffi for -o support
#     OFF                - No library embedding, smaller binary, no -o support

set(MHS_EMBED_MODE "PKG_ZSTD" CACHE STRING "MHS embedding mode: PKG_ZSTD, PKG, SRC_ZSTD, SRC")
set_property(CACHE MHS_EMBED_MODE PROPERTY STRINGS "PKG_ZSTD" "PKG" "SRC_ZSTD" "SRC")

option(MHS_ENABLE_COMPILATION "Enable MHS compilation to executable (embeds MIDI libraries)" ON)

set(ZSTD_DIR ${CMAKE_SOURCE_DIR}/source/thirdparty/zstd-1.5.7)
set(MHS_SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)

# Parse embedding mode
if(MHS_EMBED_MODE STREQUAL "PKG_ZSTD")
    set(PSND_MHS_USE_PKG TRUE CACHE INTERNAL "MHS uses package mode")
    set(PSND_MHS_USE_ZSTD TRUE)
elseif(MHS_EMBED_MODE STREQUAL "PKG")
    set(PSND_MHS_USE_PKG TRUE CACHE INTERNAL "MHS uses package mode")
    set(PSND_MHS_USE_ZSTD FALSE)
elseif(MHS_EMBED_MODE STREQUAL "SRC_ZSTD")
    set(PSND_MHS_USE_PKG FALSE CACHE INTERNAL "MHS uses package mode")
    set(PSND_MHS_USE_ZSTD TRUE)
elseif(MHS_EMBED_MODE STREQUAL "SRC")
    set(PSND_MHS_USE_PKG FALSE CACHE INTERNAL "MHS uses package mode")
    set(PSND_MHS_USE_ZSTD FALSE)
else()
    message(FATAL_ERROR "Invalid MHS_EMBED_MODE: ${MHS_EMBED_MODE}. Use PKG_ZSTD, PKG, SRC_ZSTD, or SRC")
endif()

# Check for zstd availability if needed
if(PSND_MHS_USE_ZSTD AND NOT EXISTS "${ZSTD_DIR}/zstd.c")
    message(WARNING "MHS: zstd requested but not found at ${ZSTD_DIR}, falling back to uncompressed")
    set(PSND_MHS_USE_ZSTD FALSE)
    if(PSND_MHS_USE_PKG)
        set(MHS_EMBED_MODE "PKG")
    else()
        set(MHS_EMBED_MODE "SRC")
    endif()
endif()

# Report configuration
if(PSND_MHS_USE_PKG)
    if(PSND_MHS_USE_ZSTD)
        message(STATUS "MHS psnd: PKG_ZSTD mode (precompiled packages + zstd, ~2s startup)")
    else()
        message(STATUS "MHS psnd: PKG mode (precompiled packages, ~2s startup)")
    endif()
else()
    if(PSND_MHS_USE_ZSTD)
        message(STATUS "MHS psnd: SRC_ZSTD mode (compressed sources, ~17s startup)")
    else()
        message(STATUS "MHS psnd: SRC mode (uncompressed sources, ~17s startup)")
    endif()
endif()

if(MHS_ENABLE_COMPILATION)
    message(STATUS "MHS psnd: Compilation to executable enabled (embeds MIDI libraries)")
else()
    message(STATUS "MHS psnd: Compilation to executable disabled (smaller binary)")
endif()

# Platform-specific config.h location
if(WIN32)
    set(MHS_CONFIG_DIR ${MHS_RUNTIME}/windows)
else()
    set(MHS_CONFIG_DIR ${MHS_RUNTIME}/unix)
endif()

# ==============================================================================
# Generate patched sources for VFS-based psnd integration
# ==============================================================================

# Create patched mhs.c (removes xffi_table for custom FFI)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd.c
    COMMAND ${Python3_EXECUTABLE} ${MHS_SCRIPTS_DIR}/mhs-patch-xffi.py
            ${MHS_DIR}/generated/mhs.c
            ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd.c
    DEPENDS ${MHS_DIR}/generated/mhs.c ${MHS_SCRIPTS_DIR}/mhs-patch-xffi.py
    COMMENT "Creating mhs_psnd.c with xffi_table removed"
    VERBATIM
)

# Create patched eval.c (VFS override plus the mmalloc rename Csound requires)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/eval_psnd.c
    COMMAND ${Python3_EXECUTABLE} ${MHS_SCRIPTS_DIR}/mhs-patch-eval.py
            --rename-malloc
            ${MHS_RUNTIME}/eval.c
            ${CMAKE_CURRENT_BINARY_DIR}/eval_psnd.c
    DEPENDS ${MHS_RUNTIME}/eval.c ${MHS_SCRIPTS_DIR}/mhs-patch-eval.py
    COMMENT "Creating eval_psnd.c with VFS function renames"
    VERBATIM
)

# ==============================================================================
# MHS Implementation Sources for psnd Integration
# ==============================================================================
set(MHS_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/mhs_context.c
)

# ==============================================================================
# MicroHs Runtime Library for psnd (with VFS embedding)
# ==============================================================================
set(MHS_RUNTIME_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd.c
    ${CMAKE_CURRENT_BINARY_DIR}/eval_psnd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mhs_ffi_override.c
    ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi_wrappers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs.c
)

# Add zstd decompression if available
if(PSND_MHS_USE_ZSTD)
    list(APPEND MHS_RUNTIME_SOURCES ${ZSTD_DIR}/zstddeclib.c)
endif()

add_library(mhs_runtime STATIC ${MHS_RUNTIME_SOURCES})

target_include_directories(mhs_runtime PRIVATE
    ${MHS_CONFIG_DIR}
    ${MHS_RUNTIME}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/source/thirdparty/libremidi/include
)

# VFS compile definitions based on MHS_EMBED_MODE
target_compile_definitions(mhs_runtime PRIVATE MHS_EMBEDDED_VFS)
if(PSND_MHS_USE_PKG)
    target_compile_definitions(mhs_runtime PRIVATE VFS_USE_PKG)
endif()
if(PSND_MHS_USE_ZSTD)
    target_compile_definitions(mhs_runtime PRIVATE VFS_USE_ZSTD)
    target_include_directories(mhs_runtime PRIVATE ${ZSTD_DIR})
endif()

# Pass compilation support flag to repl.c
if(NOT MHS_ENABLE_COMPILATION)
    target_compile_definitions(mhs_runtime PRIVATE MHS_NO_COMPILATION)

    # Use psnd-specific headers without libraries
    # Set header filename based on mode
    if(PSND_MHS_USE_PKG)
        if(PSND_MHS_USE_ZSTD)
            set(PSND_HEADER_FILE "mhs_psnd_pkg_zstd.h")
        else()
            set(PSND_HEADER_FILE "mhs_psnd_pkg.h")
        endif()
    else()
        if(PSND_MHS_USE_ZSTD)
            set(PSND_HEADER_FILE "mhs_psnd_src_zstd.h")
        else()
            set(PSND_HEADER_FILE "mhs_psnd_src.h")
        endif()
    endif()
    target_compile_definitions(mhs_runtime PRIVATE MHS_EMBEDDED_HEADER="${PSND_HEADER_FILE}")
endif()

target_link_libraries(mhs_runtime PUBLIC midi_ffi_psnd)

# ==============================================================================
# Embedded Header Generation for psnd
# ==============================================================================
# Generate the embedded library header that vfs.c includes
# Header selection based on MHS_EMBED_MODE:
#   PKG_ZSTD -> mhs_embedded_pkgs_zstd.h
#   PKG      -> mhs_embedded_pkgs.h
#   SRC_ZSTD -> mhs_embedded_zstd.h
#   SRC      -> mhs_embedded_libs.h

if(PSND_MHS_USE_PKG)
    if(PSND_MHS_USE_ZSTD)
        set(PSND_EMBEDDED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_pkgs_zstd.h)
    else()
        set(PSND_EMBEDDED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_pkgs.h)
    endif()
else()
    if(PSND_MHS_USE_ZSTD)
        set(PSND_EMBEDDED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_zstd.h)
    else()
        set(PSND_EMBEDDED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_libs.h)
    endif()
endif()

# The mhs_runtime needs the header to exist before compilation
# We'll add a dependency after the header generation target is created
# (see mhs-psnd-embedded-header target below)

# ==============================================================================
# psnd-specific Header Generation (respects MHS_ENABLE_COMPILATION)
# ==============================================================================
# When MHS_ENABLE_COMPILATION=OFF, we generate headers without libraries
# to reduce binary size (no libremidi.a, ~1.3MB savings)

if(NOT MHS_ENABLE_COMPILATION)
    # Generate psnd-specific headers without library embedding
    # These are generated by mhs-embed without --lib flags

    set(PSND_SRC_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_src.h)
    set(PSND_SRC_ZSTD_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_src_zstd.h)
    set(PSND_PKG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_pkg.h)
    set(PSND_PKG_ZSTD_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_pkg_zstd.h)

    # Override the header selection to use psnd-specific headers (no libs)
    if(PSND_MHS_USE_PKG)
        if(PSND_MHS_USE_ZSTD)
            set(PSND_EMBEDDED_HEADER ${PSND_PKG_ZSTD_HEADER})
        else()
            set(PSND_EMBEDDED_HEADER ${PSND_PKG_HEADER})
        endif()
    else()
        if(PSND_MHS_USE_ZSTD)
            set(PSND_EMBEDDED_HEADER ${PSND_SRC_ZSTD_HEADER})
        else()
            set(PSND_EMBEDDED_HEADER ${PSND_SRC_HEADER})
        endif()
    endif()

    message(STATUS "MHS psnd: No library embedding (MHS_ENABLE_COMPILATION=OFF)")
    message(STATUS "MHS psnd: Using ${PSND_EMBEDDED_HEADER}")
endif()

# ==============================================================================
# MIDI FFI Library for psnd (with SharedContext routing)
# ==============================================================================
add_library(midi_ffi_psnd STATIC midi_ffi.c)
target_include_directories(midi_ffi_psnd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PSND_ROOT_DIR}/source/core/include
        ${PSND_ROOT_DIR}/source/core/shared
        ${PSND_ROOT_DIR}/source/core/shared/music
        ${CMAKE_SOURCE_DIR}/source/thirdparty/libremidi/include
)
target_compile_definitions(midi_ffi_psnd PRIVATE PSND_SHARED_CONTEXT)
# Note: music_theory is part of the shared library, midi_file is not available
target_link_libraries(midi_ffi_psnd PUBLIC shared)

if(APPLE)
    target_link_libraries(midi_ffi_psnd PRIVATE
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
    )
elseif(UNIX)
    if(ALSA_FOUND)
        target_link_libraries(midi_ffi_psnd PRIVATE ${ALSA_LIBRARIES})
    endif()
endif()

# ==============================================================================
# MHS Language Library for psnd
# ==============================================================================
add_library(mhs STATIC ${MHS_IMPL_SOURCES})
add_library(mhs::mhs ALIAS mhs)

target_include_directories(mhs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${MHS_RUNTIME}
        ${PSND_ROOT_DIR}/source/core
        ${PSND_ROOT_DIR}/source/core/include
        ${PSND_ROOT_DIR}/source/core/shared
        ${PSND_ROOT_DIR}/source/thirdparty/lua-5.5.0/src
        ${CMAKE_BINARY_DIR}/generated
)

target_compile_definitions(mhs PRIVATE PSND_SHARED_CONTEXT LANG_MHS=1)
target_link_libraries(mhs PUBLIC mhs_runtime midi_ffi_psnd shared)

if(APPLE)
    target_link_libraries(mhs PUBLIC
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(mhs PRIVATE _GNU_SOURCE)
    if(ALSA_FOUND)
        target_link_libraries(mhs PUBLIC ${ALSA_LIBRARIES})
    endif()
    target_link_libraries(mhs PUBLIC pthread dl m)
endif()

# ==============================================================================
# Register with psnd Language System
# ==============================================================================
psnd_register_language(
    NAME mhs
    DISPLAY_NAME "MHS"
    DESCRIPTION "Micro Haskell MIDI language"
    COMMANDS mhs haskell
    EXTENSIONS .hs .mhs
    SOURCES ${MHS_IMPL_SOURCES}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MHS_RUNTIME}
    REPL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/repl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.c
    REGISTER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/register.c
    LINK_LIBRARIES mhs
)

# ==============================================================================
# Force package-based builds for psnd embedding (Unix only)
# ==============================================================================
# psnd uses VFS with pkg-zstd for fast startup, which requires the embedded
# header generated by the package-based build process
if(NOT WIN32)
    set(BUILD_MHS_PKG_VARIANTS ON CACHE BOOL "Required for psnd MHS embedding" FORCE)
    message(STATUS "MHS psnd: Forcing BUILD_MHS_PKG_VARIANTS=ON for embedded VFS")
endif()

endif() # ENABLE_MHS_INTEGRATION

# ==============================================================================
# MIDI FFI Library (standalone - no SharedContext)
# ==============================================================================
add_library(midi_ffi STATIC midi_ffi.c)
target_include_directories(midi_ffi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/source/thirdparty/libremidi/include
    ${PSND_ROOT_DIR}/source/core/shared/music
)
# Note: music_theory is part of the shared library

# Platform-specific dependencies for midi_ffi
if(APPLE)
    target_link_libraries(midi_ffi PRIVATE
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
    )
elseif(UNIX)
    find_package(ALSA)
    if(ALSA_FOUND)
        target_link_libraries(midi_ffi PRIVATE ${ALSA_LIBRARIES})
    endif()
endif()

# Function to create an mhs-midi executable target
# NAME: target name for the executable
# MODULE: Haskell module name (e.g., HelloMidi)
function(add_mhs_midi_example NAME MODULE)
    set(HS_SOURCE ${EXAMPLES_DIR}/${MODULE}.hs)
    set(C_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.c)
    set(EXE_OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${NAME})

    # Custom command to compile Haskell to C using MicroHs
    add_custom_command(
        OUTPUT ${C_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E env "MHSDIR=${MHS_DIR}"
            ${MHS_COMPILER}
            -i${MIDI_LIB_DIR}
            -i${EXAMPLES_DIR}
            ${MODULE}
            -C
            -o${C_OUTPUT}
        DEPENDS ${HS_SOURCE} ${MIDI_LIB_DIR}/Midi.hs ${MIDI_LIB_DIR}/Music.hs ${MIDI_LIB_DIR}/MusicPerform.hs ${MIDI_LIB_DIR}/Async.hs
        WORKING_DIRECTORY ${MHS_DIR}
        COMMENT "Compiling ${MODULE}.hs with MicroHs"
        VERBATIM
    )

    # Create the executable from generated C code
    add_executable(${NAME} ${C_OUTPUT})

    # Include directories for MicroHs runtime
    target_include_directories(${NAME} PRIVATE
        ${MHS_RUNTIME}
        ${MHS_RUNTIME}/unix
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Link with MicroHs runtime, FFI, and libremidi
    target_sources(${NAME} PRIVATE
        ${MHS_RUNTIME}/eval.c
        ${MHS_RUNTIME}/main.c
    )

    target_link_libraries(${NAME} PRIVATE
        midi_ffi
        m
    )

    # Platform-specific
    if(APPLE)
        target_link_libraries(${NAME} PRIVATE
            "-framework CoreMIDI"
            "-framework CoreFoundation"
            "-framework CoreAudio"
        )
    elseif(UNIX)
        find_package(ALSA)
        if(ALSA_FOUND)
            target_link_libraries(${NAME} PRIVATE ${ALSA_LIBRARIES})
        endif()
    endif()

    # Ensure midi_ffi is built before we try to link
    add_dependencies(${NAME} midi_ffi)

    # Strip in Release mode
    add_strip_command(${NAME})
endfunction()

# Build example executables (target_name, ModuleName)
# NOTE: Dependencies are added between targets to prevent parallel MicroHs
# compilations, which corrupt the shared .mhscache file
if(BUILD_EXAMPLES)
add_mhs_midi_example(hello_midi HelloMidi)
add_mhs_midi_example(chords Chords)
add_mhs_midi_example(melody_example Melody)
add_mhs_midi_example(arpeggio Arpeggio)
add_mhs_midi_example(list_ports ListPorts)
add_mhs_midi_example(async_test AsyncTest)

# Serialize MicroHs compilations to avoid .mhscache corruption
# Each target depends on the previous one
add_dependencies(chords hello_midi)
add_dependencies(melody_example chords)
add_dependencies(arpeggio melody_example)
add_dependencies(list_ports arpeggio)
add_dependencies(async_test list_ports)

# Custom target to build all mhs-midi examples
add_custom_target(mhs-midi-examples
    DEPENDS hello_midi chords melody_example arpeggio list_ports async_test
)
endif()

# ============================================
# mhs-midi: Interactive REPL with MIDI support
# ============================================

# Standalone mhs-midi builds
option(BUILD_MHS_STANDALONE "Build standalone mhs-midi executables" ON)
option(BUILD_MHS_PKG_VARIANTS "Build package-based mhs-midi variants (requires mcabal)" OFF)
if(NOT BUILD_MHS_STANDALONE)
    message(STATUS "MHS: Standalone builds disabled (set -DBUILD_MHS_STANDALONE=ON to enable)")
    return()
endif()
if(BUILD_MHS_PKG_VARIANTS)
    message(STATUS "MHS: Package-based standalone builds enabled")
else()
    message(STATUS "MHS: Package-based standalone builds disabled (set -DBUILD_MHS_PKG_VARIANTS=ON to enable)")
endif()

# Scripts directory for mhs build tools
set(MHS_SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)

# Python is needed for cross-platform patching scripts
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Strip xffi_table definition from mhs.c to avoid multiple definition error
# Our midi_ffi_wrappers.c provides its own xffi_table with MIDI FFI functions
# Use Python for cross-platform compatibility (sed not available on Windows)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mhs_repl.c
    COMMAND ${Python3_EXECUTABLE} ${MHS_SCRIPTS_DIR}/mhs-patch-xffi.py
        ${MHS_DIR}/generated/mhs.c
        ${CMAKE_CURRENT_BINARY_DIR}/mhs_repl.c
    DEPENDS ${MHS_DIR}/generated/mhs.c ${MHS_SCRIPTS_DIR}/mhs-patch-xffi.py
    COMMENT "Creating mhs_repl.c with xffi_table removed"
    VERBATIM
)

# Build the mhs-midi REPL executable
add_executable(mhs-midi
    ${CMAKE_CURRENT_BINARY_DIR}/mhs_repl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mhs_midi_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi_wrappers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.c
    ${MHS_RUNTIME}/eval.c
)

target_include_directories(mhs-midi PRIVATE
    ${MHS_RUNTIME}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/source/thirdparty/libremidi/include
    ${PSND_ROOT_DIR}/source/core/shared/music
)

# Platform-specific runtime includes
if(WIN32)
    target_include_directories(mhs-midi PRIVATE ${MHS_RUNTIME}/windows)
else()
    target_include_directories(mhs-midi PRIVATE ${MHS_RUNTIME}/unix)
endif()

target_link_libraries(mhs-midi PRIVATE
    music_theory
    libremidi
)

# Platform-specific dependencies
if(APPLE)
    target_link_libraries(mhs-midi PRIVATE
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
        m
    )
elseif(WIN32)
    target_link_libraries(mhs-midi PRIVATE winmm)
elseif(UNIX)
    target_link_libraries(mhs-midi PRIVATE m)
    find_package(ALSA)
    if(ALSA_FOUND)
        target_link_libraries(mhs-midi PRIVATE ${ALSA_LIBRARIES})
    endif()
endif()

add_strip_command(mhs-midi)

# ============================================
# mhs-midi standalone variants (Unix only - requires fmemopen)
# ============================================
if(NOT WIN32)
#
# Builds 3 standalone variants:
#   - mhs-midi-src:       source embedding (default)
#   - mhs-midi-src-zstd:  source embedding + zstd compression
#   - mhs-midi-pkg:       precompiled .pkg embedding (fast startup)

set(ZSTD_DIR ${CMAKE_SOURCE_DIR}/source/thirdparty/zstd-1.5.7)

# Check if zstd is available for compressed standalone builds
if(EXISTS "${ZSTD_DIR}/zstd.c")
    set(HAVE_ZSTD TRUE)
else()
    set(HAVE_ZSTD FALSE)
    message(STATUS "MHS: zstd not found, skipping compressed standalone builds")
endif()

# ============================================
# Shared generated files for all standalone variants
# ============================================

# Create modified mhs.c for standalone (removes xffi_table definition)
# Use Python for cross-platform compatibility (sed not available on Windows)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mhs_standalone.c
    COMMAND ${Python3_EXECUTABLE} ${MHS_SCRIPTS_DIR}/mhs-patch-xffi.py
            ${MHS_DIR}/generated/mhs.c
            ${CMAKE_CURRENT_BINARY_DIR}/mhs_standalone.c
    DEPENDS ${MHS_DIR}/generated/mhs.c ${MHS_SCRIPTS_DIR}/mhs-patch-xffi.py
    COMMENT "Creating mhs_standalone.c with xffi_table removed"
    VERBATIM
)

# Create patched eval.c that renames mhs_fopen to mhs_fopen_orig for VFS override
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/eval_vfs.c
    COMMAND ${Python3_EXECUTABLE} ${MHS_SCRIPTS_DIR}/mhs-patch-eval.py
            ${MHS_RUNTIME}/eval.c
            ${CMAKE_CURRENT_BINARY_DIR}/eval_vfs.c
    DEPENDS ${MHS_RUNTIME}/eval.c ${MHS_SCRIPTS_DIR}/mhs-patch-eval.py
    COMMENT "Creating eval_vfs.c with mhs_fopen renamed for VFS override"
    VERBATIM
)

# Build mhs-embed tool (with or without zstd support)
set(MHS_EMBED ${CMAKE_BINARY_DIR}/mhs-embed)
if(HAVE_ZSTD)
    add_custom_command(
        OUTPUT ${MHS_EMBED}
        COMMAND ${CMAKE_C_COMPILER} -O2
            -o ${MHS_EMBED}
            ${MHS_SCRIPTS_DIR}/mhs-embed.c
            ${ZSTD_DIR}/zstd.c
            -I${ZSTD_DIR}
            -lpthread
        DEPENDS ${MHS_SCRIPTS_DIR}/mhs-embed.c
        COMMENT "Building mhs-embed tool (with zstd)"
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT ${MHS_EMBED}
        COMMAND ${CMAKE_C_COMPILER} -O2
            -DMHS_EMBED_NO_ZSTD
            -o ${MHS_EMBED}
            ${MHS_SCRIPTS_DIR}/mhs-embed.c
            -lpthread
        DEPENDS ${MHS_SCRIPTS_DIR}/mhs-embed.c
        COMMENT "Building mhs-embed tool (no zstd)"
        VERBATIM
    )
endif()

# mhs-embed is used by every embedded-header generator. Without an owning
# target the Makefile generator duplicates the compile rule into each consuming
# target, and `make -j` then compiles several copies over the same output path
# at once - a consumer exec'ing the half-written binary fails with error 126.
add_custom_target(mhs-embed-tool DEPENDS ${MHS_EMBED})

# ============================================
# Generate embedded headers for source-based variants
# ============================================

# mhs-midi-src: uncompressed source embedding
# Uses mhs_embedded_libs.h (expected by vfs.c default mode)
set(EMBEDDED_SRC_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_libs.h)
add_custom_command(
    OUTPUT ${EMBEDDED_SRC_HEADER}
    COMMAND ${MHS_EMBED}
        ${EMBEDDED_SRC_HEADER}
        ${MHS_LIB}
        ${MIDI_LIB_DIR}
        --no-compress
        --runtime ${MHS_RUNTIME}
        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        --conf ${MHS_DIR}/mhs.conf
        --lib ${CMAKE_BINARY_DIR}/source/thirdparty/libremidi/liblibremidi.a
        --lib ${CMAKE_BINARY_DIR}/langs/mhs/libmidi_ffi.a
        --lib ${CMAKE_BINARY_DIR}/source/core/libmusic_theory.a
    DEPENDS
        ${MHS_EMBED}
        ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        libremidi
        midi_ffi
        music_theory
    COMMENT "Generating mhs_embedded_src.h (uncompressed source)"
    VERBATIM
)

# One target owns the rule. Consumers depend on the target, never on the file
# directly: a custom command output listed by two independent targets is
# duplicated into both by the Makefile generator, and a parallel build then
# runs the generator twice at once, leaving a half-written header behind.
add_custom_target(mhs-embedded-src-header DEPENDS ${EMBEDDED_SRC_HEADER})

# mhs-midi-src-zstd: compressed source embedding
# Uses mhs_embedded_zstd.h (expected by vfs.c VFS_USE_ZSTD mode)
set(EMBEDDED_SRC_ZSTD_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_zstd.h)
add_custom_command(
    OUTPUT ${EMBEDDED_SRC_ZSTD_HEADER}
    COMMAND ${MHS_EMBED}
        ${EMBEDDED_SRC_ZSTD_HEADER}
        ${MHS_LIB}
        ${MIDI_LIB_DIR}
        --runtime ${MHS_RUNTIME}
        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        --conf ${MHS_DIR}/mhs.conf
        --lib ${CMAKE_BINARY_DIR}/source/thirdparty/libremidi/liblibremidi.a
        --lib ${CMAKE_BINARY_DIR}/langs/mhs/libmidi_ffi.a
        --lib ${CMAKE_BINARY_DIR}/source/core/libmusic_theory.a
    DEPENDS
        ${MHS_EMBED}
        ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        libremidi
        midi_ffi
        music_theory
    COMMENT "Generating mhs_embedded_src_zstd.h (compressed source)"
    VERBATIM
)

add_custom_target(mhs-embedded-src-zstd-header DEPENDS ${EMBEDDED_SRC_ZSTD_HEADER})

# ============================================
# Generate embedded headers for package-based variants
# ============================================
if(BUILD_MHS_PKG_VARIANTS)

# Build base.pkg locally (keeps source tree clean)
set(LOCAL_MCABAL "${CMAKE_BINARY_DIR}/mcabal")
set(BASE_DIR "${LOCAL_MCABAL}/mhs-${MHS_VERSION}")
set(BASE_PKG "${BASE_DIR}/packages/base-${MHS_VERSION}.pkg")
set(LOCAL_MCABAL_STAMP "${LOCAL_MCABAL}/.installed")
set(MHS_BASE_BUILD "${CMAKE_BINARY_DIR}/mhs-base-src")
set(DIST_BASE_PKG "${MHS_BASE_BUILD}/dist-mcabal/base-${MHS_VERSION}.pkg")

add_custom_command(
    OUTPUT ${DIST_BASE_PKG}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MHS_BASE_BUILD}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MHS_LIB} ${MHS_BASE_BUILD}
    COMMAND ${CMAKE_COMMAND} -E chdir ${MHS_BASE_BUILD}
        ${CMAKE_COMMAND} -E env
        "PATH=${MHS_DIR}/bin:$ENV{PATH}"
        "MHSDIR=${MHS_DIR}"
        ${MHS_DIR}/bin/mcabal build
    DEPENDS ${MHS_COMPILER}
    COMMENT "Building base-${MHS_VERSION}.pkg in build/mhs-base-src"
    VERBATIM
)

# The packages below are consumed by several targets (the standalone variants
# and the psnd embedded headers). The Makefile generator copies a shared custom
# command into every consuming target's build.make, so a consumer that names the
# output file gets its own copy of the recipe and `make -j` can run it a second
# time -- GNU make caches the stat from before the owning target wrote the file,
# so ordering the consumer later does not stop it. A consumer must therefore
# depend on the owning target and on the package's own inputs, never on the .pkg
# path. v0.2.2 built music-0.1.0.pkg twice; the zstd header read the file mid
# rewrite and embedded 0 bytes, and every MHS test failed at run time with
# "/mhs-embedded/packages/music-0.1.0.pkg: openBinaryFile: does not exist".
add_custom_target(mhs-base-pkg DEPENDS ${DIST_BASE_PKG})

# Install base.pkg locally and copy runtime files
set(MHS_DATA_DIR "${BASE_DIR}/packages/mhs-${MHS_VERSION}/data")
add_custom_command(
    OUTPUT ${LOCAL_MCABAL_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LOCAL_MCABAL}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BASE_DIR}/packages
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MHS_DATA_DIR}/src/runtime
    COMMAND ${CMAKE_COMMAND} -E copy ${MHS_DIR}/bin/mhs ${LOCAL_MCABAL}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy ${MHS_DIR}/bin/cpphs ${LOCAL_MCABAL}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy ${MHS_DIR}/bin/mcabal ${LOCAL_MCABAL}/bin/
    COMMAND ${CMAKE_COMMAND} -E env "MHSDIR=${MHS_DIR}"
        ${MHS_COMPILER} -Q ${DIST_BASE_PKG} ${BASE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MHS_RUNTIME} ${MHS_DATA_DIR}/src/runtime
    COMMAND ${CMAKE_COMMAND} -E copy ${MHS_DIR}/mhs.conf ${MHS_DATA_DIR}/
    COMMAND ${CMAKE_COMMAND} -E touch ${LOCAL_MCABAL_STAMP}
    DEPENDS ${MHS_COMPILER}
    COMMENT "Installing MicroHs packages to build/mcabal"
    VERBATIM
)
add_custom_target(mhs-local-install DEPENDS ${LOCAL_MCABAL_STAMP})
add_dependencies(mhs-local-install mhs-base-pkg)

# Generate music.pkg
set(MUSIC_PKG ${CMAKE_CURRENT_BINARY_DIR}/music-0.1.0.pkg)
set(MUSIC_PKG_SOURCES
    ${MIDI_LIB_DIR}/Async.hs
    ${MIDI_LIB_DIR}/Midi.hs
    ${MIDI_LIB_DIR}/MidiPerform.hs
    ${MIDI_LIB_DIR}/Music.hs
    ${MIDI_LIB_DIR}/MusicPerform.hs
)
add_custom_command(
    OUTPUT ${MUSIC_PKG}
    COMMAND ${CMAKE_COMMAND} -E env "MHSDIR=${MHS_DIR}"
        ${MHS_COMPILER} -Pmusic-0.1.0 -i${MIDI_LIB_DIR}
        Async Midi MidiPerform Music MusicPerform
        -o ${MUSIC_PKG}
    DEPENDS ${MUSIC_PKG_SOURCES}
    WORKING_DIRECTORY ${MHS_DIR}
    COMMENT "Building music-0.1.0.pkg"
    VERBATIM
)
add_custom_target(mhs-music-pkg DEPENDS ${MUSIC_PKG})

# mhs-midi-pkg: uncompressed package embedding
# Uses mhs_embedded_pkgs.h (expected by vfs.c VFS_USE_PKG mode)
set(EMBEDDED_PKG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_pkgs.h)
add_custom_command(
    OUTPUT ${EMBEDDED_PKG_HEADER}
    COMMAND ${MHS_EMBED}
        ${EMBEDDED_PKG_HEADER}
        --no-compress
        --pkg-mode
        --pkg packages/base-${MHS_VERSION}.pkg=${BASE_PKG}
        --pkg packages/music-0.1.0.pkg=${MUSIC_PKG}
        --txt-dir ${BASE_DIR}
        --music-modules music-0.1.0.pkg:Async,Midi,MidiPerform,Music,MusicPerform
        --runtime ${MHS_DATA_DIR}/src/runtime
        --lib ${CMAKE_BINARY_DIR}/source/thirdparty/libremidi/liblibremidi.a
        --lib ${CMAKE_BINARY_DIR}/langs/mhs/libmidi_ffi.a
        --lib ${CMAKE_BINARY_DIR}/source/core/libmusic_theory.a
        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        --conf ${MHS_DIR}/mhs.conf
    DEPENDS
        ${MHS_EMBED}
        ${MHS_COMPILER}
        ${MUSIC_PKG_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        libremidi
        midi_ffi
        music_theory
    COMMENT "Generating mhs_embedded_pkg.h (uncompressed packages)"
    VERBATIM
)

add_custom_target(mhs-embedded-pkg-header DEPENDS ${EMBEDDED_PKG_HEADER})

# mhs-midi-pkg-zstd: compressed package embedding (smallest + fast startup)
# Uses mhs_embedded_pkgs_zstd.h (expected by vfs.c VFS_USE_PKG + VFS_USE_ZSTD mode)
set(EMBEDDED_PKG_ZSTD_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mhs_embedded_pkgs_zstd.h)
add_custom_command(
    OUTPUT ${EMBEDDED_PKG_ZSTD_HEADER}
    COMMAND ${MHS_EMBED}
        ${EMBEDDED_PKG_ZSTD_HEADER}
        --pkg-mode
        --pkg packages/base-${MHS_VERSION}.pkg=${BASE_PKG}
        --pkg packages/music-0.1.0.pkg=${MUSIC_PKG}
        --txt-dir ${BASE_DIR}
        --music-modules music-0.1.0.pkg:Async,Midi,MidiPerform,Music,MusicPerform
        --runtime ${MHS_DATA_DIR}/src/runtime
        --lib ${CMAKE_BINARY_DIR}/source/thirdparty/libremidi/liblibremidi.a
        --lib ${CMAKE_BINARY_DIR}/langs/mhs/libmidi_ffi.a
        --lib ${CMAKE_BINARY_DIR}/source/core/libmusic_theory.a
        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        --conf ${MHS_DIR}/mhs.conf
    DEPENDS
        ${MHS_EMBED}
        ${MHS_COMPILER}
        ${MUSIC_PKG_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
        libremidi
        midi_ffi
        music_theory
    COMMENT "Generating mhs_embedded_pkgs_zstd.h (compressed packages)"
    VERBATIM
)

add_custom_target(mhs-embedded-pkg-zstd-header DEPENDS ${EMBEDDED_PKG_ZSTD_HEADER})

# Single handle for everything the embedded-header generators need. Consumers
# depend on this target so the shared steps above run exactly once.
add_custom_target(mhs-embed-prereqs)
add_dependencies(mhs-embed-prereqs mhs-embed-tool mhs-local-install mhs-music-pkg)
add_dependencies(mhs-embedded-pkg-header mhs-embed-prereqs)
add_dependencies(mhs-embedded-pkg-zstd-header mhs-embed-prereqs)

endif() # BUILD_MHS_PKG_VARIANTS

# ============================================
# Function to create a standalone variant target
# ============================================

function(add_mhs_standalone_variant TARGET_NAME HEADER_TARGET)
    set(options USE_ZSTD USE_PKG)
    cmake_parse_arguments(VARIANT "${options}" "" "" ${ARGN})

    set(VARIANT_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/mhs_standalone.c
        ${CMAKE_CURRENT_BINARY_DIR}/eval_vfs.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mhs_midi_standalone_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi_wrappers.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mhs_ffi_override.c
        ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.c
        ${CMAKE_CURRENT_SOURCE_DIR}/vfs.c
    )

    if(VARIANT_USE_ZSTD)
        list(APPEND VARIANT_SOURCES ${ZSTD_DIR}/zstddeclib.c)
    endif()

    add_executable(${TARGET_NAME} ${VARIANT_SOURCES})

    # Ordering only - the header's own rule lives in HEADER_TARGET. Rebuilds on
    # header change still come from CMake's dependency scan of vfs.c.
    add_dependencies(${TARGET_NAME} ${HEADER_TARGET})
    add_dependencies(${TARGET_NAME} mhs-embed-tool)
    if(TARGET mhs-embed-prereqs)
        add_dependencies(${TARGET_NAME} mhs-embed-prereqs)
    endif()

    target_include_directories(${TARGET_NAME} PRIVATE
        ${MHS_RUNTIME}
        ${MHS_RUNTIME}/unix
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/source/thirdparty/libremidi/include
        ${PSND_ROOT_DIR}/source/core/shared/music
    )

    if(VARIANT_USE_PKG)
        target_compile_definitions(${TARGET_NAME} PRIVATE VFS_USE_PKG)
    endif()

    if(VARIANT_USE_ZSTD)
        target_compile_definitions(${TARGET_NAME} PRIVATE VFS_USE_ZSTD)
        target_include_directories(${TARGET_NAME} PRIVATE ${ZSTD_DIR})
    endif()

    target_link_libraries(${TARGET_NAME} PRIVATE
        music_theory
        libremidi
        m
    )

    # Platform-specific dependencies
    if(APPLE)
        target_link_libraries(${TARGET_NAME} PRIVATE
            "-framework CoreMIDI"
            "-framework CoreFoundation"
            "-framework CoreAudio"
        )
    elseif(UNIX)
        if(ALSA_FOUND)
            target_link_libraries(${TARGET_NAME} PRIVATE ${ALSA_LIBRARIES})
        endif()
    endif()

    add_strip_command(${TARGET_NAME})
endfunction()

# ============================================
# Create all standalone variant targets
# ============================================

# mhs-midi-src: source embedding (default standalone behavior)
add_mhs_standalone_variant(mhs-midi-src mhs-embedded-src-header)

# Backward compatibility: mhs-midi-standalone as alias to mhs-midi-src
add_custom_target(mhs-midi-standalone DEPENDS mhs-midi-src)

# Package-based variants (only if enabled)
if(BUILD_MHS_PKG_VARIANTS)
    # mhs-midi-pkg: package embedding (fast startup)
    add_mhs_standalone_variant(mhs-midi-pkg mhs-embedded-pkg-header USE_PKG)
endif()

# Zstd-compressed variants (only if zstd available)
if(HAVE_ZSTD)
    # mhs-midi-src-zstd: source embedding + zstd compression
    add_mhs_standalone_variant(mhs-midi-src-zstd mhs-embedded-src-zstd-header USE_ZSTD)

    if(BUILD_MHS_PKG_VARIANTS)
        # mhs-midi-pkg-zstd: compressed package embedding (smallest + fast startup)
        add_mhs_standalone_variant(mhs-midi-pkg-zstd mhs-embedded-pkg-zstd-header USE_PKG USE_ZSTD)
    endif()
endif()

# Convenience target to build all variants
set(MHS_ALL_TARGETS mhs-midi mhs-midi-src)
if(HAVE_ZSTD)
    list(APPEND MHS_ALL_TARGETS mhs-midi-src-zstd)
endif()
if(BUILD_MHS_PKG_VARIANTS)
    list(APPEND MHS_ALL_TARGETS mhs-midi-pkg)
    if(HAVE_ZSTD)
        list(APPEND MHS_ALL_TARGETS mhs-midi-pkg-zstd)
    endif()
endif()
add_custom_target(mhs-midi-all DEPENDS ${MHS_ALL_TARGETS})

# ============================================
# Installation rules (standalone variants)
# ============================================

# Install standalone source-based binary (Unix only)
install(TARGETS mhs-midi-src
    RUNTIME DESTINATION bin
    COMPONENT mhs-midi
)

if(BUILD_MHS_PKG_VARIANTS)
    install(TARGETS mhs-midi-pkg
        RUNTIME DESTINATION bin
        COMPONENT mhs-midi
    )
endif()

if(HAVE_ZSTD)
    install(TARGETS mhs-midi-src-zstd
        RUNTIME DESTINATION bin
        COMPONENT mhs-midi
    )
    if(BUILD_MHS_PKG_VARIANTS)
        install(TARGETS mhs-midi-pkg-zstd
            RUNTIME DESTINATION bin
            COMPONENT mhs-midi
        )
    endif()
endif()

endif() # NOT WIN32

# ============================================
# Installation rules (all platforms)
# ============================================

# Install mhs-midi REPL (all platforms)
install(TARGETS mhs-midi
    RUNTIME DESTINATION bin
    COMPONENT mhs-midi
)

# Install MicroHs library files (needed for compilation)
install(DIRECTORY ${MHS_LIB}/
    DESTINATION share/mhs-midi/MicroHs/lib
    COMPONENT mhs-midi
    FILES_MATCHING PATTERN "*.hs" PATTERN "*.hs-boot"
)

# Install MicroHs runtime files (needed for compilation to executable)
install(DIRECTORY ${MHS_RUNTIME}/
    DESTINATION share/mhs-midi/MicroHs/src/runtime
    COMPONENT mhs-midi
    FILES_MATCHING PATTERN "*.c" PATTERN "*.h"
)

# Install MIDI Haskell library files
install(DIRECTORY ${MIDI_LIB_DIR}/
    DESTINATION share/mhs-midi/lib
    COMPONENT mhs-midi
    FILES_MATCHING PATTERN "*.hs"
)

# ==============================================================================
# psnd MHS Embedding Dependencies
# ==============================================================================
# When ENABLE_MHS_INTEGRATION is ON, mhs_runtime needs the embedded header
# before it can compile. Add dependency from mhs_runtime to the header.
#
# Header selection based on MHS_EMBED_MODE and MHS_ENABLE_COMPILATION:
#   MHS_ENABLE_COMPILATION=ON uses standard headers (with libs):
#     PKG_ZSTD -> mhs_embedded_pkgs_zstd.h
#     PKG      -> mhs_embedded_pkgs.h
#     SRC_ZSTD -> mhs_embedded_zstd.h
#     SRC      -> mhs_embedded_libs.h
#
#   MHS_ENABLE_COMPILATION=OFF uses psnd-specific headers (no libs):
#     PKG_ZSTD -> mhs_psnd_pkg_zstd.h
#     PKG      -> mhs_psnd_pkg.h
#     SRC_ZSTD -> mhs_psnd_src_zstd.h
#     SRC      -> mhs_psnd_src.h

if(ENABLE_MHS_INTEGRATION AND NOT WIN32)
    if(MHS_ENABLE_COMPILATION)
        # Use standard headers with libraries
        if(PSND_MHS_USE_PKG)
            if(PSND_MHS_USE_ZSTD)
                set(PSND_HEADER_TARGET ${EMBEDDED_PKG_ZSTD_HEADER})
                set(PSND_HEADER_GEN_TARGET mhs-embedded-pkg-zstd-header)
            else()
                set(PSND_HEADER_TARGET ${EMBEDDED_PKG_HEADER})
                set(PSND_HEADER_GEN_TARGET mhs-embedded-pkg-header)
            endif()
        else()
            if(PSND_MHS_USE_ZSTD)
                set(PSND_HEADER_TARGET ${EMBEDDED_SRC_ZSTD_HEADER})
                set(PSND_HEADER_GEN_TARGET mhs-embedded-src-zstd-header)
            else()
                set(PSND_HEADER_TARGET ${EMBEDDED_SRC_HEADER})
                set(PSND_HEADER_GEN_TARGET mhs-embedded-src-header)
            endif()
        endif()

        # Chain to the target that owns the generator rule rather than naming
        # the file again - the standalone variants consume the same header, and
        # two independent targets listing one custom command output is what
        # makes a parallel build run the generator twice.
        add_custom_target(mhs-psnd-embedded-header)
        add_dependencies(mhs-psnd-embedded-header ${PSND_HEADER_GEN_TARGET})
        add_dependencies(mhs-psnd-embedded-header mhs-embed-tool)
        if(TARGET mhs-embed-prereqs)
            add_dependencies(mhs-psnd-embedded-header mhs-embed-prereqs)
        endif()
    else()
        # Generate psnd-specific headers without libraries
        # This reduces binary size by ~1.3MB (no libremidi.a)

        if(PSND_MHS_USE_PKG)
            # Package mode (fast startup) without libs
            if(PSND_MHS_USE_ZSTD)
                set(PSND_HEADER_TARGET ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_pkg_zstd.h)
                add_custom_command(
                    OUTPUT ${PSND_HEADER_TARGET}
                    COMMAND ${MHS_EMBED}
                        ${PSND_HEADER_TARGET}
                        --pkg-mode
                        --pkg packages/base-${MHS_VERSION}.pkg=${BASE_PKG}
                        --pkg packages/music-0.1.0.pkg=${MUSIC_PKG}
                        --txt-dir ${BASE_DIR}
                        --music-modules music-0.1.0.pkg:Async,Midi,MidiPerform,Music,MusicPerform
                        --runtime ${MHS_DATA_DIR}/src/runtime
                        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                        --conf ${MHS_DIR}/mhs.conf
                    DEPENDS ${MHS_EMBED} ${MHS_COMPILER} ${MUSIC_PKG_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                    COMMENT "Generating mhs_psnd_pkg_zstd.h (packages, no libs)"
                    VERBATIM
                )
            else()
                set(PSND_HEADER_TARGET ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_pkg.h)
                add_custom_command(
                    OUTPUT ${PSND_HEADER_TARGET}
                    COMMAND ${MHS_EMBED}
                        ${PSND_HEADER_TARGET}
                        --no-compress
                        --pkg-mode
                        --pkg packages/base-${MHS_VERSION}.pkg=${BASE_PKG}
                        --pkg packages/music-0.1.0.pkg=${MUSIC_PKG}
                        --txt-dir ${BASE_DIR}
                        --music-modules music-0.1.0.pkg:Async,Midi,MidiPerform,Music,MusicPerform
                        --runtime ${MHS_DATA_DIR}/src/runtime
                        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                        --conf ${MHS_DIR}/mhs.conf
                    DEPENDS ${MHS_EMBED} ${MHS_COMPILER} ${MUSIC_PKG_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                    COMMENT "Generating mhs_psnd_pkg.h (packages, no libs)"
                    VERBATIM
                )
            endif()
        else()
            # Source mode (slower startup) without libs
            if(PSND_MHS_USE_ZSTD)
                set(PSND_HEADER_TARGET ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_src_zstd.h)
                add_custom_command(
                    OUTPUT ${PSND_HEADER_TARGET}
                    COMMAND ${MHS_EMBED}
                        ${PSND_HEADER_TARGET}
                        ${MHS_LIB}
                        ${MIDI_LIB_DIR}
                        --runtime ${MHS_RUNTIME}
                        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                        --conf ${MHS_DIR}/mhs.conf
                    DEPENDS ${MHS_EMBED} ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                    COMMENT "Generating mhs_psnd_src_zstd.h (sources, no libs)"
                    VERBATIM
                )
            else()
                set(PSND_HEADER_TARGET ${CMAKE_CURRENT_BINARY_DIR}/mhs_psnd_src.h)
                add_custom_command(
                    OUTPUT ${PSND_HEADER_TARGET}
                    COMMAND ${MHS_EMBED}
                        ${PSND_HEADER_TARGET}
                        ${MHS_LIB}
                        ${MIDI_LIB_DIR}
                        --no-compress
                        --runtime ${MHS_RUNTIME}
                        --header ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                        --conf ${MHS_DIR}/mhs.conf
                    DEPENDS ${MHS_EMBED} ${CMAKE_CURRENT_SOURCE_DIR}/midi_ffi.h
                    COMMENT "Generating mhs_psnd_src.h (sources, no libs)"
                    VERBATIM
                )
            endif()
        endif()

        # Create a custom target for the embedded header
        add_custom_target(mhs-psnd-embedded-header DEPENDS ${PSND_HEADER_TARGET})
        add_dependencies(mhs-psnd-embedded-header mhs-embed-tool)
        if(TARGET mhs-embed-prereqs)
            add_dependencies(mhs-psnd-embedded-header mhs-embed-prereqs)
        endif()
    endif()

    # mhs_runtime depends on the embedded header being generated
    add_dependencies(mhs_runtime mhs-psnd-embedded-header)
    message(STATUS "MHS psnd: mhs_runtime depends on ${PSND_HEADER_TARGET}")
endif()
