; Alda syntax highlighting queries
;
; Only leaf nodes are captured: psnd colours each character by the first
; capture that covers it, so a capture on an enclosing node (a chord, a rest)
; would hide everything inside it.

(comment) @comment

; Parts: piano:, violin/viola "strings":
(instrument_call (identifier) @type)
(instrument_call (string) @string)
(instrument_call ":" @punctuation.delimiter)
(instrument_call "/" @punctuation.delimiter)

; Variables
(variable_definition (identifier) @function)
(variable_reference (identifier) @function.call)
"=" @operator

; Notes and rests
(note_letter) @variable
(accidental) @operator
(rest_letter) @constant.builtin
(slur) @operator

; Durations
(note_length) @number
(duration_ms) @number
(duration_s) @number
(dot) @number
(tie_duration) @number

; Octaves
(octave_set) @keyword
(octave_up) @keyword
(octave_down) @keyword

; Structure
(barline) @punctuation.delimiter
(voice_marker) @keyword.control
(marker) @label
(at_marker) @label
(repeat_count) @keyword.operator
(on_repetitions) @keyword.operator
(cram ["{" "}"] @punctuation.bracket)
(bracket_seq ["[" "]"] @punctuation.bracket)
(chord "/" @punctuation.delimiter)

; Attributes: (tempo 120), (key-sig '(e (flat)))
; Quoted data before the attribute name pattern, which would otherwise win
(quoted_symbol (sexp_symbol) @constant)
(quoted_list (sexp (sexp_symbol) @constant))
(quoted_list (sexp (sexp (sexp_symbol) @constant)))
(sexp . "(" . (sexp_symbol) @preprocessor)
(sexp_symbol) @variable.parameter
(sexp_number) @number
(string) @string
(sexp ["(" ")"] @punctuation.bracket)
"'" @punctuation.special
