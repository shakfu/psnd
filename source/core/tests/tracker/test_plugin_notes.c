/* test_plugin_notes.c - Unit tests for notes plugin
 *
 * Tests for:
 * - Note name parsing (C4, D#5, Bb3, etc.)
 * - Velocity and gate parsing
 * - Note to string conversion
 * - Expression evaluation
 * - Transform functions
 * - Validation
 */

#include "test_framework.h"
#include "tracker_plugin_notes.h"
#include "tracker_model.h"
#include <string.h>

/*============================================================================
 * Note Parsing Tests
 *============================================================================*/

TEST(parse_note_c4_returns_60) {
    uint8_t note;
    ASSERT_TRUE(tracker_notes_parse_note("C4", &note, NULL));
    ASSERT_EQ(note, 60);
}

TEST(parse_note_middle_c_variations) {
    uint8_t note;

    ASSERT_TRUE(tracker_notes_parse_note("C4", &note, NULL));
    ASSERT_EQ(note, 60);

    ASSERT_TRUE(tracker_notes_parse_note("c4", &note, NULL));
    ASSERT_EQ(note, 60);
}

TEST(parse_note_octave_range) {
    uint8_t note;

    /* C0 = 12 */
    ASSERT_TRUE(tracker_notes_parse_note("C0", &note, NULL));
    ASSERT_EQ(note, 12);

    /* C1 = 24 */
    ASSERT_TRUE(tracker_notes_parse_note("C1", &note, NULL));
    ASSERT_EQ(note, 24);

    /* C5 = 72 */
    ASSERT_TRUE(tracker_notes_parse_note("C5", &note, NULL));
    ASSERT_EQ(note, 72);

    /* C8 = 108 */
    ASSERT_TRUE(tracker_notes_parse_note("C8", &note, NULL));
    ASSERT_EQ(note, 108);
}

TEST(parse_note_all_natural_notes) {
    uint8_t note;

    /* Octave 4: C=60, D=62, E=64, F=65, G=67, A=69, B=71 */
    ASSERT_TRUE(tracker_notes_parse_note("C4", &note, NULL));
    ASSERT_EQ(note, 60);

    ASSERT_TRUE(tracker_notes_parse_note("D4", &note, NULL));
    ASSERT_EQ(note, 62);

    ASSERT_TRUE(tracker_notes_parse_note("E4", &note, NULL));
    ASSERT_EQ(note, 64);

    ASSERT_TRUE(tracker_notes_parse_note("F4", &note, NULL));
    ASSERT_EQ(note, 65);

    ASSERT_TRUE(tracker_notes_parse_note("G4", &note, NULL));
    ASSERT_EQ(note, 67);

    ASSERT_TRUE(tracker_notes_parse_note("A4", &note, NULL));
    ASSERT_EQ(note, 69);

    ASSERT_TRUE(tracker_notes_parse_note("B4", &note, NULL));
    ASSERT_EQ(note, 71);
}

TEST(parse_note_sharps) {
    uint8_t note;

    ASSERT_TRUE(tracker_notes_parse_note("C#4", &note, NULL));
    ASSERT_EQ(note, 61);

    ASSERT_TRUE(tracker_notes_parse_note("D#4", &note, NULL));
    ASSERT_EQ(note, 63);

    ASSERT_TRUE(tracker_notes_parse_note("F#4", &note, NULL));
    ASSERT_EQ(note, 66);

    ASSERT_TRUE(tracker_notes_parse_note("G#4", &note, NULL));
    ASSERT_EQ(note, 68);

    ASSERT_TRUE(tracker_notes_parse_note("A#4", &note, NULL));
    ASSERT_EQ(note, 70);
}

TEST(parse_note_flats) {
    uint8_t note;

    ASSERT_TRUE(tracker_notes_parse_note("Db4", &note, NULL));
    ASSERT_EQ(note, 61);

    ASSERT_TRUE(tracker_notes_parse_note("Eb4", &note, NULL));
    ASSERT_EQ(note, 63);

    ASSERT_TRUE(tracker_notes_parse_note("Gb4", &note, NULL));
    ASSERT_EQ(note, 66);

    ASSERT_TRUE(tracker_notes_parse_note("Ab4", &note, NULL));
    ASSERT_EQ(note, 68);

    ASSERT_TRUE(tracker_notes_parse_note("Bb4", &note, NULL));
    ASSERT_EQ(note, 70);
}

TEST(parse_note_double_accidentals) {
    uint8_t note;

    /* C## = D */
    ASSERT_TRUE(tracker_notes_parse_note("C##4", &note, NULL));
    ASSERT_EQ(note, 62);

    /* Dbb = C */
    ASSERT_TRUE(tracker_notes_parse_note("Dbb4", &note, NULL));
    ASSERT_EQ(note, 60);
}

TEST(parse_note_enharmonic_equivalents) {
    uint8_t note1, note2;

    /* C# = Db */
    ASSERT_TRUE(tracker_notes_parse_note("C#4", &note1, NULL));
    ASSERT_TRUE(tracker_notes_parse_note("Db4", &note2, NULL));
    ASSERT_EQ(note1, note2);

    /* E# = F */
    ASSERT_TRUE(tracker_notes_parse_note("E#4", &note1, NULL));
    ASSERT_TRUE(tracker_notes_parse_note("F4", &note2, NULL));
    ASSERT_EQ(note1, note2);

    /* Fb = E */
    ASSERT_TRUE(tracker_notes_parse_note("Fb4", &note1, NULL));
    ASSERT_TRUE(tracker_notes_parse_note("E4", &note2, NULL));
    ASSERT_EQ(note1, note2);
}

TEST(parse_note_default_octave) {
    uint8_t note;

    /* No octave specified = default octave 4 */
    ASSERT_TRUE(tracker_notes_parse_note("C", &note, NULL));
    ASSERT_EQ(note, 60);

    ASSERT_TRUE(tracker_notes_parse_note("G#", &note, NULL));
    ASSERT_EQ(note, 68);
}

TEST(parse_note_with_whitespace) {
    uint8_t note;

    ASSERT_TRUE(tracker_notes_parse_note("  C4", &note, NULL));
    ASSERT_EQ(note, 60);

    ASSERT_TRUE(tracker_notes_parse_note("\tD#5", &note, NULL));
    ASSERT_EQ(note, 75);
}

TEST(parse_note_returns_end_position) {
    uint8_t note;
    const char* end;
    const char* input = "C4@100";

    ASSERT_TRUE(tracker_notes_parse_note(input, &note, &end));
    ASSERT_EQ(note, 60);
    ASSERT_EQ(*end, '@');
}

TEST(parse_note_invalid_returns_false) {
    uint8_t note;

    ASSERT_FALSE(tracker_notes_parse_note("X4", &note, NULL));
    ASSERT_FALSE(tracker_notes_parse_note("4C", &note, NULL));
    ASSERT_FALSE(tracker_notes_parse_note("", &note, NULL));
    ASSERT_FALSE(tracker_notes_parse_note("123", &note, NULL));
}

TEST(parse_note_clamps_to_valid_range) {
    uint8_t note;

    /* Very high octave should clamp to 127 */
    ASSERT_TRUE(tracker_notes_parse_note("G10", &note, NULL));
    ASSERT_EQ(note, 127);
}

/*============================================================================
 * Velocity Parsing Tests
 *============================================================================*/

TEST(parse_velocity_at_symbol) {
    uint8_t vel;

    ASSERT_TRUE(tracker_notes_parse_velocity("@100", &vel, NULL));
    ASSERT_EQ(vel, 100);

    ASSERT_TRUE(tracker_notes_parse_velocity("@0", &vel, NULL));
    ASSERT_EQ(vel, 0);

    ASSERT_TRUE(tracker_notes_parse_velocity("@127", &vel, NULL));
    ASSERT_EQ(vel, 127);
}

TEST(parse_velocity_v_prefix) {
    uint8_t vel;

    ASSERT_TRUE(tracker_notes_parse_velocity("v80", &vel, NULL));
    ASSERT_EQ(vel, 80);

    ASSERT_TRUE(tracker_notes_parse_velocity("V64", &vel, NULL));
    ASSERT_EQ(vel, 64);
}

TEST(parse_velocity_clamps) {
    uint8_t vel;

    /* Above 127 clamps to 127 */
    ASSERT_TRUE(tracker_notes_parse_velocity("@200", &vel, NULL));
    ASSERT_EQ(vel, 127);
}

TEST(parse_velocity_returns_end) {
    uint8_t vel;
    const char* end;

    ASSERT_TRUE(tracker_notes_parse_velocity("@100~2", &vel, &end));
    ASSERT_EQ(vel, 100);
    ASSERT_EQ(*end, '~');
}

TEST(parse_velocity_invalid) {
    uint8_t vel;

    ASSERT_FALSE(tracker_notes_parse_velocity("100", &vel, NULL));
    ASSERT_FALSE(tracker_notes_parse_velocity("x100", &vel, NULL));
}

/*============================================================================
 * Gate Parsing Tests
 *============================================================================*/

TEST(parse_gate_basic) {
    int16_t rows;

    ASSERT_TRUE(tracker_notes_parse_gate("~1", &rows, NULL));
    ASSERT_EQ(rows, 1);

    ASSERT_TRUE(tracker_notes_parse_gate("~4", &rows, NULL));
    ASSERT_EQ(rows, 4);

    ASSERT_TRUE(tracker_notes_parse_gate("~16", &rows, NULL));
    ASSERT_EQ(rows, 16);
}

TEST(parse_gate_zero) {
    int16_t rows;

    ASSERT_TRUE(tracker_notes_parse_gate("~0", &rows, NULL));
    ASSERT_EQ(rows, 0);
}

TEST(parse_gate_returns_end) {
    int16_t rows;
    const char* end;

    ASSERT_TRUE(tracker_notes_parse_gate("~2 ", &rows, &end));
    ASSERT_EQ(rows, 2);
    ASSERT_EQ(*end, ' ');
}

TEST(parse_gate_clamps_to_int16_max) {
    int16_t rows;

    ASSERT_TRUE(tracker_notes_parse_gate("~32767", &rows, NULL));
    ASSERT_EQ(rows, 32767);

    ASSERT_TRUE(tracker_notes_parse_gate("~32768", &rows, NULL));
    ASSERT_EQ(rows, 32767);

    ASSERT_TRUE(tracker_notes_parse_gate("~65536", &rows, NULL));
    ASSERT_EQ(rows, 32767);

    /* More digits than an int holds: saturates instead of wrapping. */
    ASSERT_TRUE(tracker_notes_parse_gate("~99999999999999999999", &rows, NULL));
    ASSERT_EQ(rows, 32767);
}

TEST(parse_gate_invalid) {
    int16_t rows;

    ASSERT_FALSE(tracker_notes_parse_gate("2", &rows, NULL));
    ASSERT_FALSE(tracker_notes_parse_gate("~", &rows, NULL));
    ASSERT_FALSE(tracker_notes_parse_gate("~abc", &rows, NULL));
}

/*============================================================================
 * Note to String Tests
 *============================================================================*/

TEST(note_to_string_sharps) {
    char buf[8];

    tracker_notes_to_string(60, buf, true);
    ASSERT_STR_EQ(buf, "C4");

    tracker_notes_to_string(61, buf, true);
    ASSERT_STR_EQ(buf, "C#4");

    tracker_notes_to_string(69, buf, true);
    ASSERT_STR_EQ(buf, "A4");
}

TEST(note_to_string_flats) {
    char buf[8];

    tracker_notes_to_string(61, buf, false);
    ASSERT_STR_EQ(buf, "Db4");

    tracker_notes_to_string(63, buf, false);
    ASSERT_STR_EQ(buf, "Eb4");

    tracker_notes_to_string(70, buf, false);
    ASSERT_STR_EQ(buf, "Bb4");
}

TEST(note_to_string_octave_range) {
    char buf[8];

    tracker_notes_to_string(12, buf, true);  /* C0 */
    ASSERT_STR_EQ(buf, "C0");

    tracker_notes_to_string(24, buf, true);  /* C1 */
    ASSERT_STR_EQ(buf, "C1");

    tracker_notes_to_string(108, buf, true); /* C8 */
    ASSERT_STR_EQ(buf, "C8");
}

/*============================================================================
 * Plugin Registration Tests
 *============================================================================*/

TEST(plugin_get_returns_valid_plugin) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    ASSERT_NOT_NULL(plugin);
    ASSERT_NOT_NULL(plugin->name);
    ASSERT_NOT_NULL(plugin->language_id);
    ASSERT_STR_EQ(plugin->language_id, "notes");
}

TEST(plugin_has_required_capabilities) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    ASSERT_TRUE(plugin->capabilities & TRACKER_CAP_EVALUATE);
    ASSERT_TRUE(plugin->capabilities & TRACKER_CAP_VALIDATION);
    ASSERT_TRUE(plugin->capabilities & TRACKER_CAP_TRANSFORMS);
}

TEST(plugin_init_succeeds) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    ASSERT_NOT_NULL(plugin->init);
    ASSERT_TRUE(plugin->init());
}

/*============================================================================
 * Validation Tests
 *============================================================================*/

TEST(validate_accepts_valid_notes) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    ASSERT_TRUE(plugin->validate("C4", NULL, NULL));
    ASSERT_TRUE(plugin->validate("D#5", NULL, NULL));
    ASSERT_TRUE(plugin->validate("Bb3", NULL, NULL));
    ASSERT_TRUE(plugin->validate("F##2", NULL, NULL));
}

TEST(validate_accepts_rest) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    ASSERT_TRUE(plugin->validate("r", NULL, NULL));
    ASSERT_TRUE(plugin->validate("-", NULL, NULL));
}

TEST(validate_accepts_note_off) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    ASSERT_TRUE(plugin->validate("x", NULL, NULL));
    ASSERT_TRUE(plugin->validate("X", NULL, NULL));
    ASSERT_TRUE(plugin->validate("off", NULL, NULL));
}

TEST(validate_rejects_empty) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    const char* error_msg = NULL;

    ASSERT_FALSE(plugin->validate("", &error_msg, NULL));
    ASSERT_NOT_NULL(error_msg);
}

TEST(validate_rejects_invalid) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    const char* error_msg = NULL;
    int error_pos = -1;

    /* "xyz" starts with 'x' which is valid note-off, use something else */
    ASSERT_FALSE(plugin->validate("123", &error_msg, &error_pos));
    ASSERT_NOT_NULL(error_msg);

    ASSERT_FALSE(plugin->validate("zzz", &error_msg, &error_pos));
}

/*============================================================================
 * Evaluation Tests
 *============================================================================*/

TEST(evaluate_single_note) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};
    ctx.channel = 0;

    TrackerPhrase* phrase = plugin->evaluate("C4", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 1);
    ASSERT_EQ(phrase->events[0].type, TRACKER_EVENT_NOTE_ON);
    ASSERT_EQ(phrase->events[0].data1, 60);  /* C4 */
    ASSERT_EQ(phrase->events[0].data2, 80);  /* default velocity */

    tracker_phrase_free(phrase);
}

TEST(evaluate_note_with_velocity) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("C4@100", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 1);
    ASSERT_EQ(phrase->events[0].data1, 60);
    ASSERT_EQ(phrase->events[0].data2, 100);

    tracker_phrase_free(phrase);
}

TEST(evaluate_note_with_gate) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("C4~4", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 1);
    ASSERT_EQ(phrase->events[0].gate_rows, 4);

    tracker_phrase_free(phrase);
}

TEST(evaluate_note_with_velocity_and_gate) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("D#5@100~2", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 1);
    ASSERT_EQ(phrase->events[0].data1, 75);  /* D#5 */
    ASSERT_EQ(phrase->events[0].data2, 100);
    ASSERT_EQ(phrase->events[0].gate_rows, 2);

    tracker_phrase_free(phrase);
}

TEST(evaluate_chord_space_separated) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("C4 E4 G4", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 3);
    ASSERT_EQ(phrase->events[0].data1, 60);  /* C4 */
    ASSERT_EQ(phrase->events[1].data1, 64);  /* E4 */
    ASSERT_EQ(phrase->events[2].data1, 67);  /* G4 */

    tracker_phrase_free(phrase);
}

TEST(evaluate_chord_comma_separated) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("C4,E4,G4", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 3);

    tracker_phrase_free(phrase);
}

TEST(evaluate_rest_returns_empty) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("r", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 0);

    tracker_phrase_free(phrase);

    phrase = plugin->evaluate("-", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 0);

    tracker_phrase_free(phrase);
}

TEST(evaluate_note_off) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};

    TrackerPhrase* phrase = plugin->evaluate("x", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->count, 1);
    ASSERT_EQ(phrase->events[0].type, TRACKER_EVENT_NOTE_OFF);

    tracker_phrase_free(phrase);
}

TEST(evaluate_uses_context_channel) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerContext ctx = {0};
    ctx.channel = 5;

    TrackerPhrase* phrase = plugin->evaluate("C4", &ctx);
    ASSERT_NOT_NULL(phrase);
    ASSERT_EQ(phrase->events[0].channel, 5);

    tracker_phrase_free(phrase);
}

/*============================================================================
 * Transform Tests
 *============================================================================*/

TEST(transform_transpose_up) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn transpose = plugin->get_transform("transpose");
    ASSERT_NOT_NULL(transpose);

    /* Create input phrase with C4 */
    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 60;  /* C4 */
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = transpose(input, "7", NULL);  /* up a fifth */
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->count, 1);
    ASSERT_EQ(output->events[0].data1, 67);  /* G4 */

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_transpose_down) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn transpose = plugin->get_transform("tr");  /* alias */
    ASSERT_NOT_NULL(transpose);

    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 60;
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = transpose(input, "-12", NULL);  /* down an octave */
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data1, 48);  /* C3 */

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_transpose_clamps) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn transpose = plugin->get_transform("transpose");

    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 120;  /* high note */
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = transpose(input, "20", NULL);
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data1, 127);  /* clamped */

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_velocity) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn velocity = plugin->get_transform("velocity");
    ASSERT_NOT_NULL(velocity);

    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 60;
    event.data2 = 80;  /* original velocity */
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = velocity(input, "100", NULL);
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data2, 100);

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_velocity_alias) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn vel = plugin->get_transform("vel");
    ASSERT_NOT_NULL(vel);
}

TEST(transform_octave_up) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn octave = plugin->get_transform("octave");
    ASSERT_NOT_NULL(octave);

    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 60;  /* C4 */
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = octave(input, "1", NULL);
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data1, 72);  /* C5 */

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_octave_down) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn octave = plugin->get_transform("oct");  /* alias */

    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 60;
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = octave(input, "-2", NULL);
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data1, 36);  /* C2 */

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_invert_around_c4) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn invert = plugin->get_transform("invert");
    ASSERT_NOT_NULL(invert);

    /* E4 (64) inverted around C4 (60) = G#3 (56) */
    /* pivot - (note - pivot) = 60 - (64 - 60) = 60 - 4 = 56 */
    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 64;  /* E4 */
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = invert(input, "60", NULL);  /* pivot = C4 */
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data1, 56);  /* G#3 */

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_invert_with_note_name) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn invert = plugin->get_transform("inv");  /* alias */

    TrackerPhrase* input = tracker_phrase_new(1);
    TrackerEvent event = {0};
    event.type = TRACKER_EVENT_NOTE_ON;
    event.data1 = 64;  /* E4 */
    tracker_phrase_add_event(input, &event);

    TrackerPhrase* output = invert(input, "C4", NULL);  /* pivot as note name */
    ASSERT_NOT_NULL(output);
    ASSERT_EQ(output->events[0].data1, 56);

    tracker_phrase_free(input);
    tracker_phrase_free(output);
}

TEST(transform_unknown_returns_null) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    TrackerTransformFn unknown = plugin->get_transform("nonexistent");
    ASSERT_NULL(unknown);
}

TEST(transform_list_returns_names) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();
    int count = 0;
    const char** names = plugin->list_transforms(&count);

    ASSERT_NOT_NULL(names);
    ASSERT_TRUE(count > 0);

    /* Check some known names are present */
    bool found_transpose = false;
    bool found_velocity = false;
    for (int i = 0; i < count; i++) {
        if (names[i] && strcmp(names[i], "transpose") == 0) found_transpose = true;
        if (names[i] && strcmp(names[i], "velocity") == 0) found_velocity = true;
    }
    ASSERT_TRUE(found_transpose);
    ASSERT_TRUE(found_velocity);
}

TEST(transform_describe_returns_description) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    const char* desc = plugin->describe_transform("transpose");
    ASSERT_NOT_NULL(desc);
    ASSERT_TRUE(strlen(desc) > 0);
}

/*============================================================================
 * Is Generator Tests
 *============================================================================*/

TEST(is_generator_returns_false) {
    const TrackerPlugin* plugin = tracker_plugin_notes_get();

    /* Notes are never generators */
    ASSERT_FALSE(plugin->is_generator("C4"));
    ASSERT_FALSE(plugin->is_generator("C4 E4 G4"));
    ASSERT_FALSE(plugin->is_generator("r"));
}

/*============================================================================
 * Test Suite
 *============================================================================*/

BEGIN_TEST_SUITE("Notes Plugin")
    /* Note parsing */
    RUN_TEST(parse_note_c4_returns_60);
    RUN_TEST(parse_note_middle_c_variations);
    RUN_TEST(parse_note_octave_range);
    RUN_TEST(parse_note_all_natural_notes);
    RUN_TEST(parse_note_sharps);
    RUN_TEST(parse_note_flats);
    RUN_TEST(parse_note_double_accidentals);
    RUN_TEST(parse_note_enharmonic_equivalents);
    RUN_TEST(parse_note_default_octave);
    RUN_TEST(parse_note_with_whitespace);
    RUN_TEST(parse_note_returns_end_position);
    RUN_TEST(parse_note_invalid_returns_false);
    RUN_TEST(parse_note_clamps_to_valid_range);

    /* Velocity parsing */
    RUN_TEST(parse_velocity_at_symbol);
    RUN_TEST(parse_velocity_v_prefix);
    RUN_TEST(parse_velocity_clamps);
    RUN_TEST(parse_velocity_returns_end);
    RUN_TEST(parse_velocity_invalid);

    /* Gate parsing */
    RUN_TEST(parse_gate_basic);
    RUN_TEST(parse_gate_zero);
    RUN_TEST(parse_gate_returns_end);
    RUN_TEST(parse_gate_clamps_to_int16_max);
    RUN_TEST(parse_gate_invalid);

    /* Note to string */
    RUN_TEST(note_to_string_sharps);
    RUN_TEST(note_to_string_flats);
    RUN_TEST(note_to_string_octave_range);

    /* Plugin registration */
    RUN_TEST(plugin_get_returns_valid_plugin);
    RUN_TEST(plugin_has_required_capabilities);
    RUN_TEST(plugin_init_succeeds);

    /* Validation */
    RUN_TEST(validate_accepts_valid_notes);
    RUN_TEST(validate_accepts_rest);
    RUN_TEST(validate_accepts_note_off);
    RUN_TEST(validate_rejects_empty);
    RUN_TEST(validate_rejects_invalid);

    /* Evaluation */
    RUN_TEST(evaluate_single_note);
    RUN_TEST(evaluate_note_with_velocity);
    RUN_TEST(evaluate_note_with_gate);
    RUN_TEST(evaluate_note_with_velocity_and_gate);
    RUN_TEST(evaluate_chord_space_separated);
    RUN_TEST(evaluate_chord_comma_separated);
    RUN_TEST(evaluate_rest_returns_empty);
    RUN_TEST(evaluate_note_off);
    RUN_TEST(evaluate_uses_context_channel);

    /* Transforms */
    RUN_TEST(transform_transpose_up);
    RUN_TEST(transform_transpose_down);
    RUN_TEST(transform_transpose_clamps);
    RUN_TEST(transform_velocity);
    RUN_TEST(transform_velocity_alias);
    RUN_TEST(transform_octave_up);
    RUN_TEST(transform_octave_down);
    RUN_TEST(transform_invert_around_c4);
    RUN_TEST(transform_invert_with_note_name);
    RUN_TEST(transform_unknown_returns_null);
    RUN_TEST(transform_list_returns_names);
    RUN_TEST(transform_describe_returns_description);

    /* Is generator */
    RUN_TEST(is_generator_returns_false);
END_TEST_SUITE()
