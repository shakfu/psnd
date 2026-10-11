/* loki_modal.h - Modal editing (vim-like modes)
 *
 * This header declares the public API for modal editing functionality.
 * Modal editing provides vim-like modes (NORMAL, INSERT, VISUAL) for
 * efficient keyboard-only text editing.
 */

#ifndef LOKI_MODAL_H
#define LOKI_MODAL_H

#include "internal.h"

/* Process a single keypress with modal editing support.
 * This is the main entry point for all keyboard input.
 * Handles mode switching and dispatches to appropriate mode handler. */
/* Returns -1 if the input is closed or failed, 0 otherwise. */
int modal_process_keypress(editor_ctx_t *ctx, int fd);

/* ============================================================================
 * Keybind Command Handlers
 * ============================================================================
 * These command handlers can be registered with keybind_register() to allow
 * remapping via TOML configuration.
 */

/* Evaluate selection or current part */
int cmd_eval_line(editor_ctx_t *ctx, int fd);

/* Play entire file */
int cmd_play_file(editor_ctx_t *ctx, int fd);

/* Request quit via keybind (multi-press confirmation handled in modal_process_keypress) */
int cmd_quit_keybind(editor_ctx_t *ctx, int fd);

#endif /* LOKI_MODAL_H */
