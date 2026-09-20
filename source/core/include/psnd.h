/**
 * @file psnd.h
 * @brief Central constants for the psnd project.
 *
 * This header provides program-wide constants that should be used instead of
 * hardcoded strings throughout the codebase. Module-specific limits remain
 * in their respective headers.
 */

#ifndef PSND_H
#define PSND_H

/* Program identity */
#define PSND_NAME           "psnd"
/* PSND_VERSION is normally injected by CMake from the project() VERSION (the
 * single source of truth). This literal is only a fallback for non-CMake or
 * IDE builds; keep it in sync with CMakeLists.txt. */
#ifndef PSND_VERSION
#define PSND_VERSION        "0.3.0"
#endif

/* Configuration */
#define PSND_CONFIG_DIR     ".psnd"

/* Default MIDI port name for virtual ports */
#define PSND_MIDI_PORT_NAME "PSND_MIDI"

#endif /* PSND_H */
