/**
 * tracker_plugin.c - Plugin registry and compilation
 */

#include "tracker_plugin.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/*============================================================================
 * Plugin Registry - Internal State
 *============================================================================*/

#define MAX_PLUGINS 64

typedef struct {
    TrackerPlugin* plugins[MAX_PLUGINS];
    int count;
    TrackerPlugin* default_plugin;
    bool initialized;
} PluginRegistry;

static PluginRegistry g_registry = {0};

/*============================================================================
 * Plugin Registry Functions
 *============================================================================*/

bool tracker_plugin_registry_init(void) {
    if (g_registry.initialized) return true;

    memset(&g_registry, 0, sizeof(g_registry));
    g_registry.initialized = true;

    return true;
}

void tracker_plugin_registry_cleanup(void) {
    if (!g_registry.initialized) return;

    /* Call cleanup on all registered plugins */
    for (int i = 0; i < g_registry.count; i++) {
        TrackerPlugin* plugin = g_registry.plugins[i];
        if (plugin && plugin->cleanup) {
            plugin->cleanup();
        }
    }

    memset(&g_registry, 0, sizeof(g_registry));
}

bool tracker_plugin_register(TrackerPlugin* plugin) {
    if (!g_registry.initialized) {
        tracker_plugin_registry_init();
    }

    if (!plugin || !plugin->language_id) return false;
    if (g_registry.count >= MAX_PLUGINS) return false;

    /* Check for duplicate language_id */
    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.plugins[i]->language_id, plugin->language_id) == 0) {
            return false;  /* already registered */
        }
    }

    /* Initialize the plugin */
    if (plugin->init && !plugin->init()) {
        return false;
    }

    g_registry.plugins[g_registry.count++] = plugin;

    /* Set as default if it's the first one */
    if (g_registry.default_plugin == NULL) {
        g_registry.default_plugin = plugin;
    }

    return true;
}

bool tracker_plugin_unregister(const char* language_id) {
    if (!g_registry.initialized || !language_id) return false;

    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.plugins[i]->language_id, language_id) == 0) {
            TrackerPlugin* plugin = g_registry.plugins[i];

            /* Call cleanup */
            if (plugin->cleanup) {
                plugin->cleanup();
            }

            /* Update default if this was it */
            if (g_registry.default_plugin == plugin) {
                g_registry.default_plugin = (g_registry.count > 1) ?
                    g_registry.plugins[i == 0 ? 1 : 0] : NULL;
            }

            /* Shift remaining plugins */
            if (i < g_registry.count - 1) {
                memmove(&g_registry.plugins[i],
                        &g_registry.plugins[i + 1],
                        (g_registry.count - i - 1) * sizeof(TrackerPlugin*));
            }

            g_registry.count--;
            return true;
        }
    }

    return false;
}

TrackerPlugin* tracker_plugin_find(const char* language_id) {
    if (!g_registry.initialized) return NULL;

    if (!language_id) {
        return g_registry.default_plugin;
    }

    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.plugins[i]->language_id, language_id) == 0) {
            return g_registry.plugins[i];
        }
    }

    return NULL;
}

TrackerTransformFn tracker_plugin_find_transform(const char* fx_name,
                                                  TrackerPlugin** out_plugin) {
    if (!g_registry.initialized || !fx_name) return NULL;

    TrackerTransformFn best_fn = NULL;
    TrackerPlugin* best_plugin = NULL;
    int best_priority = -1;

    for (int i = 0; i < g_registry.count; i++) {
        TrackerPlugin* plugin = g_registry.plugins[i];

        if (!tracker_plugin_has_cap(plugin, TRACKER_CAP_TRANSFORMS)) continue;
        if (!plugin->get_transform) continue;

        TrackerTransformFn fn = plugin->get_transform(fx_name);
        if (fn && plugin->priority > best_priority) {
            best_fn = fn;
            best_plugin = plugin;
            best_priority = plugin->priority;
        }
    }

    if (out_plugin) *out_plugin = best_plugin;
    return best_fn;
}

TrackerTransformInfo* tracker_plugin_list_all_transforms(int* count) {
    if (!g_registry.initialized || !count) {
        if (count) *count = 0;
        return NULL;
    }

    /* First pass: count total transforms */
    int total = 0;
    for (int i = 0; i < g_registry.count; i++) {
        TrackerPlugin* plugin = g_registry.plugins[i];
        if (!tracker_plugin_has_cap(plugin, TRACKER_CAP_TRANSFORMS)) continue;
        if (!plugin->list_transforms) continue;

        int plugin_count = 0;
        plugin->list_transforms(&plugin_count);
        total += plugin_count;
    }

    if (total == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate result array */
    TrackerTransformInfo* result = malloc(total * sizeof(TrackerTransformInfo));
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* Second pass: collect transforms, sorted by priority (simple insertion) */
    int idx = 0;

    /* Create temporary array for sorting by priority */
    typedef struct {
        TrackerPlugin* plugin;
        int priority;
    } PluginPriority;

    PluginPriority* sorted = malloc(g_registry.count * sizeof(PluginPriority));
    if (!sorted) {
        free(result);
        *count = 0;
        return NULL;
    }

    int sorted_count = 0;
    for (int i = 0; i < g_registry.count; i++) {
        TrackerPlugin* plugin = g_registry.plugins[i];
        if (!tracker_plugin_has_cap(plugin, TRACKER_CAP_TRANSFORMS)) continue;
        if (!plugin->list_transforms) continue;

        /* Insert sorted by priority (descending) */
        int j = sorted_count;
        while (j > 0 && sorted[j-1].priority < plugin->priority) {
            sorted[j] = sorted[j-1];
            j--;
        }
        sorted[j].plugin = plugin;
        sorted[j].priority = plugin->priority;
        sorted_count++;
    }

    /* Collect transforms in priority order */
    for (int i = 0; i < sorted_count; i++) {
        TrackerPlugin* plugin = sorted[i].plugin;
        int plugin_count = 0;
        const char** names = plugin->list_transforms(&plugin_count);

        for (int j = 0; j < plugin_count && idx < total; j++) {
            result[idx].plugin = plugin;
            result[idx].fx_name = names[j];
            idx++;
        }
    }

    free(sorted);
    *count = idx;
    return result;
}

TrackerPlugin* tracker_plugin_get_default(void) {
    if (!g_registry.initialized) return NULL;
    return g_registry.default_plugin;
}

bool tracker_plugin_set_default(const char* language_id) {
    TrackerPlugin* plugin = tracker_plugin_find(language_id);
    if (!plugin) return false;

    g_registry.default_plugin = plugin;
    return true;
}

TrackerPlugin** tracker_plugin_list_all(int* count) {
    if (!g_registry.initialized || !count) {
        if (count) *count = 0;
        return NULL;
    }

    *count = g_registry.count;
    return g_registry.plugins;
}

/*============================================================================
 * Compilation Functions
 *============================================================================*/

CompiledCell* tracker_compile_cell(const TrackerCell* cell,
                                   const char* default_lang_id,
                                   const char** error_msg) {
    if (!cell) {
        if (error_msg) *error_msg = "NULL cell";
        return NULL;
    }

    /* Empty cells don't need compilation */
    if (cell->type == TRACKER_CELL_EMPTY ||
        cell->type == TRACKER_CELL_CONTINUATION) {
        return NULL;  /* not an error, just nothing to compile */
    }

    /* Note-off cells are simple */
    if (cell->type == TRACKER_CELL_NOTE_OFF) {
        CompiledCell* compiled = calloc(1, sizeof(CompiledCell));
        if (!compiled) {
            if (error_msg) *error_msg = "Out of memory";
            return NULL;
        }
        compiled->is_generator = false;
        compiled->content.cached_phrase = NULL;  /* engine handles note-off */
        return compiled;
    }

    /* Find the plugin */
    const char* lang_id = cell->language_id ? cell->language_id : default_lang_id;
    TrackerPlugin* plugin = tracker_plugin_find(lang_id);

    if (!plugin) {
        if (error_msg) *error_msg = "Unknown language";
        return NULL;
    }

    if (!tracker_plugin_has_cap(plugin, TRACKER_CAP_EVALUATE)) {
        if (error_msg) *error_msg = "Plugin cannot evaluate expressions";
        return NULL;
    }

    /* Validate if supported */
    if (tracker_plugin_has_cap(plugin, TRACKER_CAP_VALIDATION) && plugin->validate) {
        const char* val_error = NULL;
        int error_pos = -1;
        if (!plugin->validate(cell->expression, &val_error, &error_pos)) {
            if (error_msg) *error_msg = val_error ? val_error : "Syntax error";
            return NULL;
        }
    }

    /* Create compiled cell */
    CompiledCell* compiled = calloc(1, sizeof(CompiledCell));
    if (!compiled) {
        if (error_msg) *error_msg = "Out of memory";
        return NULL;
    }

    compiled->plugin = plugin;

    /* Check if it's a generator */
    if (tracker_plugin_has_cap(plugin, TRACKER_CAP_GENERATORS) && plugin->is_generator) {
        compiled->is_generator = plugin->is_generator(cell->expression);
    } else {
        compiled->is_generator = false;
    }

    if (compiled->is_generator) {
        /* Store source for later evaluation */
        compiled->content.dynamic.source_expr = str_dup(cell->expression);
        if (!compiled->content.dynamic.source_expr) {
            free(compiled);
            if (error_msg) *error_msg = "Out of memory";
            return NULL;
        }

        /* Try to pre-compile if supported */
        if (tracker_plugin_has_cap(plugin, TRACKER_CAP_COMPILATION) && plugin->compile) {
            const char* comp_error = NULL;
            compiled->content.dynamic.compiled_expr =
                plugin->compile(cell->expression, &comp_error);
            /* Compilation failure is not fatal for generators */
        }
    } else {
        /* Static expression: try to pre-compile or we'll evaluate lazily */
        if (tracker_plugin_has_cap(plugin, TRACKER_CAP_COMPILATION) && plugin->compile) {
            const char* comp_error = NULL;
            compiled->content.dynamic.compiled_expr =
                plugin->compile(cell->expression, &comp_error);
            compiled->content.dynamic.source_expr = str_dup(cell->expression);

            if (!compiled->content.dynamic.compiled_expr &&
                !compiled->content.dynamic.source_expr) {
                free(compiled);
                if (error_msg) *error_msg = comp_error ? comp_error : "Compilation failed";
                return NULL;
            }
        } else {
            /* No compilation support - store source for lazy evaluation */
            compiled->content.dynamic.source_expr = str_dup(cell->expression);
            compiled->content.dynamic.compiled_expr = NULL;
        }
    }

    /* Compile FX chain */
    if (cell->fx_chain.count > 0) {
        const char* fx_error = NULL;
        CompiledFxChain* fx = tracker_compile_fx_chain(&cell->fx_chain,
                                                        default_lang_id, &fx_error);
        if (!fx && fx_error) {
            /* FX chain compilation failed - cleanup and return error */
            tracker_compiled_cell_free(compiled);
            if (error_msg) *error_msg = fx_error;
            return NULL;
        }
        if (fx) {
            compiled->fx_chain = *fx;
            free(fx);  /* shallow free - we took ownership of contents */
        }
    }

    if (error_msg) *error_msg = NULL;
    return compiled;
}

void tracker_compiled_cell_free(CompiledCell* compiled) {
    if (!compiled) return;

    tracker_phrase_free(compiled->content.cached_phrase);

    /* Free compiled expression if plugin supports it */
    if (compiled->plugin &&
        compiled->content.dynamic.compiled_expr &&
        compiled->plugin->free_compiled) {
        compiled->plugin->free_compiled(compiled->content.dynamic.compiled_expr);
    }
    free(compiled->content.dynamic.source_expr);

    /* Free FX chain */
    tracker_compiled_fx_chain_free(&compiled->fx_chain);

    free(compiled);
}

CompiledFxChain* tracker_compile_fx_chain(const TrackerFxChain* chain,
                                          const char* default_lang_id,
                                          const char** error_msg) {
    if (!chain || chain->count == 0) {
        return NULL;  /* not an error */
    }

    CompiledFxChain* compiled = calloc(1, sizeof(CompiledFxChain));
    if (!compiled) {
        if (error_msg) *error_msg = "Out of memory";
        return NULL;
    }

    compiled->entries = calloc(chain->count, sizeof(CompiledFxEntry));
    if (!compiled->entries) {
        free(compiled);
        if (error_msg) *error_msg = "Out of memory";
        return NULL;
    }

    compiled->capacity = chain->count;

    for (int i = 0; i < chain->count; i++) {
        const TrackerFxEntry* src = &chain->entries[i];
        CompiledFxEntry* dest = &compiled->entries[i];

        /* Find the transform function */
        const char* lang_id = src->language_id ? src->language_id : default_lang_id;
        TrackerPlugin* plugin = NULL;

        /* First try specific language if specified */
        if (lang_id) {
            plugin = tracker_plugin_find(lang_id);
            if (plugin && tracker_plugin_has_cap(plugin, TRACKER_CAP_TRANSFORMS) &&
                plugin->get_transform) {
                dest->fn = plugin->get_transform(src->name);
            }
        }

        /* If not found, search all plugins */
        if (!dest->fn) {
            dest->fn = tracker_plugin_find_transform(src->name, &plugin);
        }

        if (!dest->fn) {
            /* Transform not found - error */
            tracker_compiled_fx_chain_free(compiled);
            free(compiled);
            if (error_msg) *error_msg = "Unknown transform";
            return NULL;
        }

        dest->plugin = plugin;
        dest->enabled = src->enabled;
        dest->raw_params = str_dup(src->params);

        /* Pre-parse params if supported */
        if (plugin && plugin->parse_transform_params) {
            dest->parsed_params = plugin->parse_transform_params(src->name, src->params);
        }

        compiled->count++;
    }

    if (error_msg) *error_msg = NULL;
    return compiled;
}

void tracker_compiled_fx_chain_free(CompiledFxChain* compiled) {
    if (!compiled) return;

    for (int i = 0; i < compiled->count; i++) {
        CompiledFxEntry* entry = &compiled->entries[i];

        /* Free parsed params if plugin supports it */
        if (entry->plugin && entry->parsed_params && entry->plugin->free_transform_params) {
            entry->plugin->free_transform_params(entry->parsed_params);
        }

        free(entry->raw_params);
    }

    free(compiled->entries);

    /* Don't free compiled itself if it's embedded - let caller decide */
    compiled->entries = NULL;
    compiled->count = 0;
    compiled->capacity = 0;
}

void tracker_invalidate_pattern(TrackerPattern* pattern) {
    if (!pattern) return;

    for (int t = 0; t < pattern->num_tracks; t++) {
        TrackerTrack* track = &pattern->tracks[t];

        /* Invalidate track FX */
        if (track->compiled_fx) {
            tracker_compiled_fx_chain_free(track->compiled_fx);
            free(track->compiled_fx);
            track->compiled_fx = NULL;
        }

        /* Invalidate cells */
        for (int r = 0; r < pattern->num_rows; r++) {
            TrackerCell* cell = &track->cells[r];
            if (cell->compiled) {
                tracker_compiled_cell_free(cell->compiled);
                cell->compiled = NULL;
            }
            cell->dirty = true;
        }
    }
}

void tracker_invalidate_song(TrackerSong* song) {
    if (!song) return;

    /* Invalidate master FX */
    if (song->compiled_master_fx) {
        tracker_compiled_fx_chain_free(song->compiled_master_fx);
        free(song->compiled_master_fx);
        song->compiled_master_fx = NULL;
    }

    /* Invalidate all patterns */
    for (int i = 0; i < song->num_patterns; i++) {
        tracker_invalidate_pattern(song->patterns[i]);
    }
}

/*============================================================================
 * Evaluation Functions
 *============================================================================*/

TrackerPhrase* tracker_evaluate_cell(CompiledCell* compiled, TrackerContext* ctx) {
    if (!compiled || !ctx) return NULL;

    TrackerPlugin* plugin = compiled->plugin;
    if (!plugin) return NULL;

    TrackerPhrase* phrase = NULL;

    if (compiled->is_generator ||
        (!compiled->content.cached_phrase && compiled->content.dynamic.source_expr)) {
        /* Need to evaluate */
        if (compiled->content.dynamic.compiled_expr &&
            plugin->evaluate_compiled) {
            phrase = plugin->evaluate_compiled(compiled->content.dynamic.compiled_expr, ctx);
        } else if (plugin->evaluate) {
            phrase = plugin->evaluate(compiled->content.dynamic.source_expr, ctx);
        }

        /* Cache result for static expressions */
        if (!compiled->is_generator && phrase) {
            compiled->content.cached_phrase = phrase;
            /* Return a clone so caller owns it */
            return tracker_phrase_clone(phrase);
        }
    } else if (compiled->content.cached_phrase) {
        /* Return a clone of cached phrase */
        return tracker_phrase_clone(compiled->content.cached_phrase);
    }

    return phrase;
}

TrackerPhrase* tracker_apply_fx_chain(CompiledFxChain* chain,
                                      TrackerPhrase* phrase,
                                      TrackerContext* ctx) {
    if (!phrase) return NULL;
    if (!chain || chain->count == 0) return phrase;

    TrackerPhrase* current = phrase;

    for (int i = 0; i < chain->count; i++) {
        CompiledFxEntry* entry = &chain->entries[i];

        if (!entry->enabled || !entry->fn) continue;

        /* Apply transform */
        const char* params = entry->raw_params;  /* TODO: use parsed_params if available */
        TrackerPhrase* transformed = entry->fn(current, params, ctx);

        if (!transformed) {
            /* Transform failed - keep current and continue or abort? */
            /* For now, abort on failure */
            if (current != phrase) {
                tracker_phrase_free(current);
            }
            tracker_phrase_free(phrase);
            return NULL;
        }

        /* Free previous (unless it's the original) */
        if (current != phrase) {
            tracker_phrase_free(current);
        }

        current = transformed;
    }

    /* If we modified the phrase, free the original */
    if (current != phrase) {
        tracker_phrase_free(phrase);
    }

    return current;
}

/*============================================================================
 * Context Helpers
 *============================================================================*/

/**
 * Phrase lookup callback implementation.
 * Looks up a named phrase in the song's phrase library.
 */
static bool context_lookup_phrase(TrackerContext* ctx, const char* name,
                                   const char** out_expr, const char** out_lang_id) {
    if (!ctx || !ctx->song || !name) return false;

    /* Cast away const for lookup (we're not modifying) */
    TrackerPhraseLibrary* lib = (TrackerPhraseLibrary*)&ctx->song->phrase_library;
    TrackerPhraseEntry* entry = tracker_phrase_library_get(lib, name);

    if (!entry) return false;

    if (out_expr) *out_expr = entry->expression;
    if (out_lang_id) *out_lang_id = entry->language_id;

    return true;
}

void tracker_context_init(TrackerContext* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(TrackerContext));

    /* Set sensible defaults */
    ctx->bpm = TRACKER_DEFAULT_BPM;
    ctx->rows_per_beat = TRACKER_DEFAULT_RPB;
    ctx->ticks_per_row = TRACKER_DEFAULT_TPR;
    ctx->spillover_mode = TRACKER_SPILLOVER_LAYER;
}

void tracker_context_from_song(TrackerContext* ctx,
                               const TrackerSong* song,
                               int pattern_index,
                               int row,
                               int track) {
    if (!ctx) return;
    tracker_context_init(ctx);

    if (!song) return;

    ctx->song = song;
    ctx->song_name = song->name;
    ctx->bpm = song->bpm;
    ctx->rows_per_beat = song->rows_per_beat;
    ctx->ticks_per_row = song->ticks_per_row;
    ctx->spillover_mode = song->spillover_mode;
    ctx->lookup_phrase = context_lookup_phrase;

    ctx->current_pattern = pattern_index;
    ctx->current_row = row;
    ctx->current_track = track;

    if (pattern_index >= 0 && pattern_index < song->num_patterns) {
        TrackerPattern* pattern = song->patterns[pattern_index];
        ctx->total_tracks = pattern->num_tracks;
        ctx->total_rows = pattern->num_rows;

        if (track >= 0 && track < pattern->num_tracks) {
            TrackerTrack* t = &pattern->tracks[track];
            ctx->channel = t->default_channel;
            ctx->track_name = t->name;
            ctx->track_muted = t->muted;
            ctx->track_solo = t->solo;
        }
    }

    /* Calculate absolute tick */
    ctx->absolute_tick = tracker_calc_absolute_tick(row, 0, song->ticks_per_row);

    /* Calculate absolute time */
    ctx->absolute_time_ms = tracker_tick_to_ms(ctx->absolute_tick,
                                                song->bpm,
                                                song->rows_per_beat,
                                                song->ticks_per_row);
}

/*============================================================================
 * Random Number Generation (xorshift32)
 *============================================================================*/

static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

uint32_t tracker_context_random(TrackerContext* ctx, uint32_t max) {
    if (!ctx || max == 0) return 0;

    /* Initialize state from seed if needed */
    if (ctx->random_state == 0) {
        ctx->random_state = ctx->random_seed ? ctx->random_seed : 1;
    }

    uint32_t r = xorshift32(&ctx->random_state);
    return r % max;
}

float tracker_context_random_float(TrackerContext* ctx) {
    if (!ctx) return 0.0f;

    if (ctx->random_state == 0) {
        ctx->random_state = ctx->random_seed ? ctx->random_seed : 1;
    }

    uint32_t r = xorshift32(&ctx->random_state);
    return (float)r / (float)UINT32_MAX;
}

void tracker_context_reseed(TrackerContext* ctx, uint32_t seed) {
    if (!ctx) return;
    ctx->random_seed = seed;
    ctx->random_state = seed ? seed : 1;
}
