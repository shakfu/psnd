; Alda syntax highlighting queries

; Comments
(comment) @comment

; Strings
(string) @string

; Numbers and durations
(note_length) @number
(duration_ms) @number
(duration_s) @number
(sexp_number) @number

; Notes and pitches
(note_letter) @constant
(pitch) @constant

; Accidentals and slurs
(accidental) @operator
(slur) @operator

; Rests
(rest) @constant.builtin

; Octave control
(octave_set) @keyword
(octave_up) @operator
(octave_down) @operator

; Chords
(chord) @constant

; Instruments and parts
(instrument_call) @function
(identifier) @variable

; Markers
(marker) @label
(at_marker) @label
(voice_marker) @keyword

; Grouping
(cram ["{" "}"] @punctuation.bracket)
(bracket_seq ["[" "]"] @punctuation.bracket)

; Repetition
(repeat_count) @number
(on_repetitions) @number

; S-expressions (Lisp-like)
(sexp ["(" ")"] @punctuation.bracket)
(sexp_symbol) @function.builtin
(quoted_list) @constant
(quoted_symbol) @constant

; Operators and delimiters
(barline) @punctuation.delimiter
(dot) @operator
(tie_duration) @operator
"=" @operator
"/" @operator
":" @punctuation.delimiter
