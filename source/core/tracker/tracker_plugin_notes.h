/**
 * tracker_plugin_notes.h - Simple note parser plugin for the tracker
 *
 * Parses note expressions in a simple, readable format:
 *
 *   Notes:      C4, D#5, Bb3, F##2
 *   Chords:     C4 E4 G4  or  C4,E4,G4
 *   Velocity:   C4@100  or  C4 v100
 *   Duration:   C4~2  (gate = 2 rows)
 *   Rest:       r  or  -
 *   Note-off:   x  or  off
 *
 * Examples:
 *   "C4"           - Middle C, default velocity
 *   "C4@80"        - Middle C, velocity 80
 *   "C4 E4 G4"     - C major chord
 *   "D#5@100~2"    - D#5, velocity 100, 2-row gate
 *   "r"            - Rest (no output)
 *   "x"            - Explicit note-off
 */

#ifndef TRACKER_PLUGIN_NOTES_H
#define TRACKER_PLUGIN_NOTES_H

#include "tracker_plugin.h"

/*============================================================================
 * Plugin Registration
 *============================================================================*/

/**
 * Get the notes plugin definition.
 * Call tracker_plugin_register() with this to register.
 */
const TrackerPlugin* tracker_plugin_notes_get(void);

/**
 * Register the notes plugin with the global registry.
 * Convenience function that calls tracker_plugin_register().
 */
bool tracker_plugin_notes_register(void);

/*============================================================================
 * Direct Parsing API (for use outside tracker context)
 *============================================================================*/

/**
 * Parse a single note string to MIDI note number.
 *
 * @param str         Note string (e.g., "C4", "D#5", "Bb3")
 * @param out_note    Output: MIDI note number (0-127)
 * @param out_end     Output: pointer to character after parsed note (optional)
 * @return            true if valid note, false otherwise
 */
bool tracker_notes_parse_note(const char* str, uint8_t* out_note, const char** out_end);

/**
 * Parse velocity from string.
 * Handles both "@100" and "v100" formats.
 *
 * @param str         String starting with @ or v
 * @param out_vel     Output: velocity (0-127)
 * @param out_end     Output: pointer after parsed velocity
 * @return            true if valid velocity, false otherwise
 */
bool tracker_notes_parse_velocity(const char* str, uint8_t* out_vel, const char** out_end);

/**
 * Parse gate/duration from string.
 * Handles "~N" format where N is rows.
 *
 * @param str         String starting with ~
 * @param out_rows    Output: gate in rows, clamped to 0..INT16_MAX
 * @param out_end     Output: pointer after parsed gate
 * @return            true if valid gate, false otherwise
 */
bool tracker_notes_parse_gate(const char* str, int16_t* out_rows, const char** out_end);

/**
 * Convert MIDI note number to string.
 *
 * @param note        MIDI note (0-127)
 * @param buffer      Output buffer (at least 5 bytes)
 * @param use_sharps  true for sharps (C#), false for flats (Db)
 */
void tracker_notes_to_string(uint8_t note, char* buffer, bool use_sharps);

/*============================================================================
 * Constants
 *============================================================================*/

#define TRACKER_NOTES_DEFAULT_VELOCITY  80
#define TRACKER_NOTES_DEFAULT_OCTAVE    4
#define TRACKER_NOTES_DEFAULT_GATE      1   /* 1 row */

#endif /* TRACKER_PLUGIN_NOTES_H */
