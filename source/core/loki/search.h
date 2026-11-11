/* loki_search.h - Text search functionality
 *
 * This header declares the public API for incremental text search.
 * Search is interactive and updates as the user types, with visual
 * highlighting of matches and arrow key navigation between results.
 */

#ifndef LOKI_SEARCH_H
#define LOKI_SEARCH_H

#include "internal.h"

/* Incremental search, driven by keys from any frontend. ESC cancels and
 * restores the cursor; ENTER keeps it on the match; arrows step between
 * matches. Keys reach search_handle_key() through modal_process_event(). */
void search_begin(editor_ctx_t *ctx);
int search_active(const editor_ctx_t *ctx);
/* Returns 1 if the key was consumed (search was active). */
int search_handle_key(editor_ctx_t *ctx, int c);
/* Prompt for the message line while active, else NULL. Static buffer. */
const char *search_prompt(const editor_ctx_t *ctx);
/* n / N: jump to the next (direction 1) or previous (-1) match of the last
 * accepted query from the cursor, wrapping at the file ends. Returns 0 on a
 * match, -1 when there is none or no previous search. */
int search_repeat(editor_ctx_t *ctx, int direction);
/* Replace the last accepted query (tests; NULL clears it). */
void search_set_last_query(const char *query);
/* Drop saved highlight memory (context teardown). */
void search_free(editor_ctx_t *ctx);

#endif /* LOKI_SEARCH_H */
