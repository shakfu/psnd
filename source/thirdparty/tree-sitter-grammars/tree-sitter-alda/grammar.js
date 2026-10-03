/**
 * Tree-sitter grammar for Alda music notation language
 * https://alda.io/
 *
 * Used for highlighting. Token rules follow Alda's scanner
 * (client/parser/scanner.go in alda-lang/alda).
 *
 * After editing, regenerate src/ from this directory with
 *   npx tree-sitter-cli@0.25.10 generate --abi 14
 * and keep the highlight query in source/core/loki/treesitter.c in step with
 * queries/highlights.scm. test_treesitter_alda parses every shipped score.
 */

module.exports = grammar({
  name: 'alda',

  // A newline is whitespace: parts, brackets, crams and s-expressions all
  // continue across lines.
  extras: $ => [
    /[ \t\r\n]+/,
    $.comment,
  ],

  // A name after a part's events may start the next part (piano:), a variable
  // definition (riff =) or be a reference within the part; the token after it
  // decides, so these are resolved by exploring both.
  conflicts: $ => [
    [$.part_declaration],
    [$.variable_definition],
  ],

  rules: {
    source_file: $ => repeat($._toplevel),

    _toplevel: $ => choice(
      $.part_declaration,
      $.variable_definition,
      $._event,
    ),

    comment: $ => /#[^\n]*/,

    // Part declaration: piano: or violin/viola "strings":
    part_declaration: $ => seq(
      $.instrument_call,
      repeat($._event),
    ),

    instrument_call: $ => seq(
      $.identifier,
      repeat(seq('/', $.identifier)),
      optional($.string),
      ':',
    ),

    // Variable definition: notes = c d e f
    variable_definition: $ => seq(
      $.identifier,
      '=',
      repeat1($._event),
    ),

    variable_reference: $ => prec.right(seq(
      $.identifier,
      optional($.repeat_count),
      optional($.on_repetitions),
    )),

    _event: $ => choice(
      $.note,
      $.rest,
      $.chord,
      $.sexp,
      $.octave_set,
      $.octave_up,
      $.octave_down,
      $.barline,
      $.marker,
      $.at_marker,
      $.voice_marker,
      $.cram,
      $.bracket_seq,
      $.variable_reference,
      // Postfix forms written after any event, as in (key-sig ...)*5
      $.repeat_count,
      $.on_repetitions,
      // A tie continuing a note across a barline: g2 | ~2
      $.tie_duration,
    ),

    // Notes: c, c4, c+4, c4., c4~4, c4~ (slurred), c*3, c'1
    note: $ => prec.right(seq(
      $.pitch,
      optional($.duration),
      optional($.slur),
      optional($.repeat_count),
      optional($.on_repetitions),
    )),

    pitch: $ => seq(
      $.note_letter,
      repeat($.accidental),
    ),

    note_letter: $ => /[a-g]/,

    accidental: $ => choice('+', '-', '_'),

    slur: $ => '~',

    // Rest: r, r4, r4., etc.
    rest: $ => prec.right(seq(
      /r/,
      optional($.duration),
      optional($.repeat_count),
      optional($.on_repetitions),
    )),

    // Chord: c/e/g, c4./e4, c1/e/g/r4, c/e/>c. Each member keeps its own
    // duration.
    chord: $ => prec.right(1, seq(
      $.note,
      repeat1(seq(
        '/',
        repeat(choice($.octave_up, $.octave_down)),
        choice($.note, $.rest),
      )),
    )),

    // Duration: 4, 4., 4.., 500ms, 2s, 4~4, 2.~500ms
    duration: $ => prec.right(seq(
      $._duration_value,
      repeat($.dot),
      repeat($.tie_duration),
    )),

    _duration_value: $ => choice(
      $.note_length,
      $.duration_ms,
      $.duration_s,
    ),

    note_length: $ => /[0-9]+(\.[0-9]+)?/,
    duration_ms: $ => /[0-9]+(\.[0-9]+)?ms/,
    duration_s: $ => /[0-9]+(\.[0-9]+)?s/,
    dot: $ => '.',
    // A tie is one token, so that a '~' with no length after it is a slur
    tie_duration: $ => token(seq(
      '~',
      /[ \t]*(\|[ \t]*)?/,  // A tie may cross a barline: a-8~|2.
      /[0-9]+(\.[0-9]+)?(ms|s)?/,
      /\.*/,
    )),

    // Octave control
    octave_set: $ => /o-?[0-9]+/,
    octave_up: $ => '>',
    octave_down: $ => '<',

    // Barline
    barline: $ => '|',

    // Markers
    marker: $ => /%[a-zA-Z0-9_\-+.]+/,
    at_marker: $ => /@[a-zA-Z0-9_\-+.]+/,

    // Voice markers: V1:, V2:, V0:
    voice_marker: $ => /V[0-9]+:/,

    // Cram: {c d e f}4
    cram: $ => prec.right(seq(
      '{',
      repeat($._event),
      '}',
      optional($.duration),
      optional($.repeat_count),
      optional($.on_repetitions),
    )),

    // Bracket sequence: [c d e f]*2
    bracket_seq: $ => prec.right(seq(
      '[',
      repeat($._event),
      ']',
      optional($.repeat_count),
      optional($.on_repetitions),
    )),

    // Repetition: *3
    repeat_count: $ => /\*[0-9]+/,

    // On repetitions: '1-3,5
    on_repetitions: $ => /'[0-9][0-9,\-]*/,

    // S-expression (Lisp-like): (tempo 120), (key-sig '(e (flat)))
    sexp: $ => seq(
      '(',
      repeat($._sexp_item),
      ')',
    ),

    _sexp_item: $ => choice(
      $.sexp_symbol,
      $.sexp_number,
      $.string,
      $.quoted_list,
      $.quoted_symbol,
      $.sexp,  // nested
    ),

    sexp_symbol: $ => /[a-zA-Z_!?+\-*/<>=.:#][a-zA-Z0-9_!?+\-*/<>=.:#]*/,

    sexp_number: $ => /-?[0-9]+(\.[0-9]+)?/,

    quoted_list: $ => seq("'", $.sexp),

    quoted_symbol: $ => seq("'", $.sexp_symbol),

    // Strings: "hello"
    string: $ => /"[^"]*"/,

    // Names start with two letters (alda-language/scores-and-parts.md), so
    // that c4, o4 and V1 are not names. '+' as in midi-bass+lead, '.' for
    // a group member such as strings.cello.
    identifier: $ => /[a-zA-Z][a-zA-Z][a-zA-Z0-9_\-+.]*/,
  },
});
