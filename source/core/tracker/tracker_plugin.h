/**
 * tracker_plugin.h - Plugin interface for language integration
 *
 * Plugins allow different languages (Alda, Joy, etc.) to integrate with the tracker.
 * Each plugin provides:
 *   - Expression evaluation (source string -> Phrase)
 *   - Transform functions (FX that modify Phrases)
 *   - Generator detection (static vs generative phrases)
 *
 * Processing flow:
 *   1. Cell expression is compiled (parsed, validated, cached)
 *   2. At trigger time: evaluate expression -> Phrase
 *   3. Apply cell FX chain
 *   4. Apply track FX chain
 *   5. Apply master FX chain
 *   6. Schedule resulting events
 */

#ifndef TRACKER_PLUGIN_H
#define TRACKER_PLUGIN_H

#include "tracker_model.h"

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct TrackerPlugin TrackerPlugin;
typedef struct TrackerContext TrackerContext;

/*============================================================================
 * Plugin Capabilities
 *============================================================================*/

typedef enum {
    TRACKER_CAP_NONE              = 0,
    TRACKER_CAP_EVALUATE          = 1 << 0,   /* can evaluate expressions */
    TRACKER_CAP_GENERATORS        = 1 << 1,   /* supports generative expressions */
    TRACKER_CAP_COMPILATION       = 1 << 2,   /* supports pre-compilation */
    TRACKER_CAP_TRANSFORMS        = 1 << 3,   /* provides transform functions */
    TRACKER_CAP_VALIDATION        = 1 << 4,   /* supports syntax validation */
    TRACKER_CAP_CROSS_TRACK       = 1 << 5,   /* uses cross-track queries */
    TRACKER_CAP_RECENT_EVENTS     = 1 << 6,   /* uses recent event history */
    TRACKER_CAP_RANDOM            = 1 << 7,   /* uses randomness (respects seed) */
    TRACKER_CAP_STATEFUL          = 1 << 8,   /* maintains state across triggers */
} TrackerPluginCapabilities;

/*============================================================================
 * Function Signatures
 *============================================================================*/

/**
 * Transform function: takes a phrase, returns a transformed phrase.
 *
 * @param input     The input phrase (caller owns, do not free)
 * @param params    Parameter string from FX entry, may be NULL
 * @param ctx       Current tracker context
 * @return          New phrase (caller takes ownership), or NULL on error
 */
typedef TrackerPhrase* (*TrackerTransformFn)(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
);

/**
 * Generator function: produces a phrase at trigger time.
 * Called each time the cell triggers (for non-deterministic/generative content).
 *
 * @param ctx       Current tracker context
 * @param user_data Plugin-specific data from compilation
 * @return          New phrase (caller takes ownership), or NULL on error
 */
typedef TrackerPhrase* (*TrackerGeneratorFn)(
    TrackerContext* ctx,
    void* user_data
);

/**
 * Cross-track query callback: allows plugins to query other tracks' events.
 * Provided in TrackerContext for reactive/cross-track composition.
 *
 * @param ctx           The context (for user_data access)
 * @param track_index   Track to query (-1 for all tracks)
 * @param rows_back     How many rows of history (0 = current row only)
 * @param out_phrase    Output: phrase containing queried events (caller owns)
 * @return              true on success, false if track doesn't exist or no data
 */
typedef bool (*TrackerCrossTrackQueryFn)(
    TrackerContext* ctx,
    int track_index,
    int rows_back,
    TrackerPhrase** out_phrase
);

/**
 * Track info query callback: get metadata about a track.
 *
 * @param ctx           The context
 * @param track_index   Track to query
 * @param out_name      Output: track name (do not free)
 * @param out_channel   Output: track's default channel
 * @param out_muted     Output: whether track is muted
 * @return              true on success, false if track doesn't exist
 */
typedef bool (*TrackerTrackInfoQueryFn)(
    TrackerContext* ctx,
    int track_index,
    const char** out_name,
    uint8_t* out_channel,
    bool* out_muted
);

/**
 * Phrase lookup callback: look up a named phrase from the song's library.
 *
 * @param ctx           The context
 * @param name          Phrase name (without @ prefix)
 * @param out_expr      Output: phrase expression string (do not free)
 * @param out_lang_id   Output: language ID for evaluation (do not free), may be NULL
 * @return              true if phrase found, false otherwise
 */
typedef bool (*TrackerPhraseLookupFn)(
    TrackerContext* ctx,
    const char* name,
    const char** out_expr,
    const char** out_lang_id
);

/*============================================================================
 * Tracker Context
 *============================================================================*/

/**
 * Context passed to evaluation and transform functions.
 * Provides read-only access to current playback state.
 */
struct TrackerContext {
    /*------------------------------------------------------------------------
     * Position
     *------------------------------------------------------------------------*/
    int current_row;
    int current_track;
    int current_pattern;
    int sequence_position;        /* position in song sequence */
    int total_tracks;             /* number of tracks in current pattern */
    int total_rows;               /* number of rows in current pattern */

    /*------------------------------------------------------------------------
     * Timing
     *------------------------------------------------------------------------*/
    int bpm;
    int rows_per_beat;
    int ticks_per_row;
    int64_t absolute_tick;        /* ticks since song start */
    double absolute_time_ms;      /* milliseconds since song start */

    /*------------------------------------------------------------------------
     * Track Info
     *------------------------------------------------------------------------*/
    uint8_t channel;              /* track's default channel */
    const char* track_name;       /* may be NULL */
    bool track_muted;
    bool track_solo;

    /*------------------------------------------------------------------------
     * Song Info
     *------------------------------------------------------------------------*/
    const char* song_name;
    TrackerSpilloverMode spillover_mode;
    const TrackerSong* song;              /* for phrase library access (do not modify) */

    /*------------------------------------------------------------------------
     * Phrase Library
     *------------------------------------------------------------------------*/
    TrackerPhraseLookupFn lookup_phrase;  /* look up named phrase */
    int phrase_recursion_depth;           /* for detecting infinite recursion */

    /*------------------------------------------------------------------------
     * Randomness
     *------------------------------------------------------------------------*/
    uint32_t random_seed;         /* for reproducible generative content */
    uint32_t random_state;        /* current PRNG state (plugin can advance) */

    /*------------------------------------------------------------------------
     * Recent Events (for reactive composition)
     *------------------------------------------------------------------------*/
    const TrackerPhrase* recent_events;  /* events from last N rows on this track */
    int recent_events_rows;               /* how many rows of history */

    /*------------------------------------------------------------------------
     * Cross-Track Queries
     *------------------------------------------------------------------------*/
    TrackerCrossTrackQueryFn query_track_events;  /* query other tracks' events */
    TrackerTrackInfoQueryFn query_track_info;     /* query track metadata */

    /*------------------------------------------------------------------------
     * User Data
     *------------------------------------------------------------------------*/
    void* engine_data;            /* engine's private data (do not touch) */
    void* plugin_data;            /* plugin can store state here during playback */
};

/*============================================================================
 * Plugin Error Codes
 *============================================================================*/

typedef enum {
    TRACKER_PLUGIN_OK = 0,
    TRACKER_PLUGIN_ERR_SYNTAX,        /* expression syntax error */
    TRACKER_PLUGIN_ERR_UNKNOWN_FX,    /* unknown transform name */
    TRACKER_PLUGIN_ERR_INVALID_PARAMS,/* invalid FX parameters */
    TRACKER_PLUGIN_ERR_EVAL_FAILED,   /* evaluation failed at runtime */
    TRACKER_PLUGIN_ERR_OUT_OF_MEMORY,
    TRACKER_PLUGIN_ERR_NOT_INITIALIZED,
    TRACKER_PLUGIN_ERR_UNSUPPORTED,   /* capability not supported */
} TrackerPluginError;

/*============================================================================
 * Plugin Interface
 *============================================================================*/

/**
 * Plugin interface. Each language implements this to integrate with the tracker.
 */
struct TrackerPlugin {
    /*------------------------------------------------------------------------
     * Identity
     *------------------------------------------------------------------------*/
    const char* name;             /* display name, e.g., "Alda" */
    const char* language_id;      /* identifier used in cells, e.g., "alda" */
    const char* version;          /* plugin version string */
    const char* description;      /* brief description of the language */

    /*------------------------------------------------------------------------
     * Capabilities & Priority
     *------------------------------------------------------------------------*/
    uint32_t capabilities;        /* TrackerPluginCapabilities bitfield */
    int priority;                 /* higher = preferred for FX name conflicts (default 0) */

    /*------------------------------------------------------------------------
     * Lifecycle
     *------------------------------------------------------------------------*/

    /**
     * Initialize the plugin. Called once when plugin is registered.
     * @return true on success, false on failure
     */
    bool (*init)(void);

    /**
     * Cleanup the plugin. Called when plugin is unregistered or tracker shuts down.
     */
    void (*cleanup)(void);

    /**
     * Reset plugin state. Called when playback stops or song changes.
     * Only relevant for TRACKER_CAP_STATEFUL plugins.
     */
    void (*reset)(void);

    /*------------------------------------------------------------------------
     * Expression Handling
     *------------------------------------------------------------------------*/

    /**
     * Validate an expression without evaluating it.
     * Used for syntax checking during editing.
     * Only called if TRACKER_CAP_VALIDATION is set.
     *
     * @param expression  The expression to validate
     * @param error_msg   Output: error message if invalid (plugin owns string)
     * @param error_pos   Output: character position of error, or -1
     * @return            true if valid, false if invalid
     */
    bool (*validate)(const char* expression, const char** error_msg, int* error_pos);

    /**
     * Check if an expression is a generator (produces different output each call).
     * Static expressions can be cached; generators must be re-evaluated each trigger.
     * Only called if TRACKER_CAP_GENERATORS is set; otherwise all expressions
     * are assumed static.
     *
     * @param expression  The expression to check
     * @return            true if generator, false if static
     */
    bool (*is_generator)(const char* expression);

    /**
     * Evaluate an expression and return a phrase.
     * For static expressions, result may be cached by the engine.
     * For generators, this is called each trigger.
     * Required if TRACKER_CAP_EVALUATE is set.
     *
     * @param expression  The expression to evaluate
     * @param ctx         Current tracker context
     * @return            New phrase (caller takes ownership), or NULL on error
     */
    TrackerPhrase* (*evaluate)(const char* expression, TrackerContext* ctx);

    /**
     * Compile an expression for faster repeated evaluation.
     * Returns opaque handle that can be passed to evaluate_compiled.
     * Only called if TRACKER_CAP_COMPILATION is set.
     *
     * @param expression  The expression to compile
     * @param error_msg   Output: error message if compilation fails
     * @return            Opaque compiled handle, or NULL on error
     */
    void* (*compile)(const char* expression, const char** error_msg);

    /**
     * Evaluate a pre-compiled expression.
     * Only called if TRACKER_CAP_COMPILATION is set and compile() returned non-NULL.
     */
    TrackerPhrase* (*evaluate_compiled)(void* compiled, TrackerContext* ctx);

    /**
     * Free a compiled expression handle.
     */
    void (*free_compiled)(void* compiled);

    /*------------------------------------------------------------------------
     * Transform (FX) Handling
     *------------------------------------------------------------------------*/

    /**
     * Get a transform function by name.
     * Only called if TRACKER_CAP_TRANSFORMS is set.
     *
     * @param fx_name  The transform name, e.g., "transpose", "ratchet"
     * @return         Transform function pointer, or NULL if not found
     */
    TrackerTransformFn (*get_transform)(const char* fx_name);

    /**
     * List all available transforms.
     *
     * @param count  Output: number of transforms
     * @return       Array of transform names (plugin owns array and strings)
     */
    const char** (*list_transforms)(int* count);

    /**
     * Get human-readable description of a transform.
     *
     * @param fx_name  The transform name
     * @return         Description string (plugin owns), or NULL if not found
     */
    const char* (*describe_transform)(const char* fx_name);

    /**
     * Get parameter documentation for a transform.
     *
     * @param fx_name  The transform name
     * @return         Parameter doc string (plugin owns), or NULL
     */
    const char* (*get_transform_params_doc)(const char* fx_name);

    /**
     * Validate transform parameters.
     *
     * @param fx_name    The transform name
     * @param params     Parameter string to validate
     * @param error_msg  Output: error message if invalid
     * @return           true if valid, false if invalid
     */
    bool (*validate_transform_params)(const char* fx_name, const char* params,
                                      const char** error_msg);

    /**
     * Pre-parse transform parameters for faster application.
     * Returns opaque handle passed to transform function via context.
     *
     * @param fx_name  The transform name
     * @param params   Parameter string
     * @return         Parsed params handle, or NULL (use raw string)
     */
    void* (*parse_transform_params)(const char* fx_name, const char* params);

    /**
     * Free parsed transform parameters.
     */
    void (*free_transform_params)(void* parsed);

    /*------------------------------------------------------------------------
     * Error Handling
     *------------------------------------------------------------------------*/

    /**
     * Get the last error code.
     */
    TrackerPluginError (*get_last_error)(void);

    /**
     * Get the last error message.
     * @return  Error message (plugin owns string), or NULL if no error
     */
    const char* (*get_last_error_message)(void);

    /**
     * Clear the last error.
     */
    void (*clear_error)(void);
};

/*============================================================================
 * Compiled Structures (used by engine, defined here for completeness)
 *============================================================================*/

/**
 * A compiled FX entry - transform function with pre-parsed params.
 */
typedef struct {
    TrackerTransformFn fn;        /* transform function */
    void* parsed_params;          /* pre-parsed params (plugin owns), may be NULL */
    char* raw_params;             /* original params string (owned) */
    TrackerPlugin* plugin;        /* plugin that owns this transform */
    bool enabled;                 /* can be toggled without recompiling */
} CompiledFxEntry;

/**
 * A compiled FX chain - ready for fast application.
 */
struct CompiledFxChain {
    CompiledFxEntry* entries;
    int count;
    int capacity;
};

/**
 * A compiled cell - ready for fast evaluation.
 */
struct CompiledCell {
    TrackerPlugin* plugin;        /* plugin that handles this cell */
    bool is_generator;            /* true if must re-evaluate each trigger */

    /* A struct, not a union: a static cell caches its phrase while keeping its source */
    struct {
        /* For static expressions: cached phrase */
        TrackerPhrase* cached_phrase;

        /* For generators or compiled expressions */
        struct {
            void* compiled_expr;  /* from plugin->compile(), may be NULL */
            char* source_expr;    /* original expression (owned) */
        } dynamic;
    } content;

    CompiledFxChain fx_chain;     /* compiled cell-level FX */
};

/*============================================================================
 * Plugin Registry Functions
 *============================================================================*/

/**
 * Initialize the plugin registry. Call once at startup.
 */
bool tracker_plugin_registry_init(void);

/**
 * Cleanup the plugin registry. Call at shutdown.
 */
void tracker_plugin_registry_cleanup(void);

/**
 * Register a plugin.
 *
 * @param plugin  Plugin to register (registry does not take ownership)
 * @return        true on success, false if language_id already registered
 */
bool tracker_plugin_register(TrackerPlugin* plugin);

/**
 * Unregister a plugin.
 *
 * @param language_id  Language identifier to unregister
 * @return             true if found and unregistered, false if not found
 */
bool tracker_plugin_unregister(const char* language_id);

/**
 * Find a plugin by language ID.
 *
 * @param language_id  Language identifier, or NULL for default
 * @return             Plugin pointer, or NULL if not found
 */
TrackerPlugin* tracker_plugin_find(const char* language_id);

/**
 * Find a transform across all plugins.
 * Uses plugin priority to resolve conflicts.
 *
 * @param fx_name      Transform name to find
 * @param out_plugin   Output: plugin that provides the transform
 * @return             Transform function, or NULL if not found
 */
TrackerTransformFn tracker_plugin_find_transform(
    const char* fx_name,
    TrackerPlugin** out_plugin
);

/**
 * List all available transforms across all plugins.
 * Higher priority plugins' transforms listed first.
 *
 * @param count  Output: total number of transforms
 * @return       Array of {plugin, fx_name} pairs (caller must free array, not contents)
 */
typedef struct {
    TrackerPlugin* plugin;
    const char* fx_name;
} TrackerTransformInfo;

TrackerTransformInfo* tracker_plugin_list_all_transforms(int* count);

/**
 * Get the default plugin (used when cell has no explicit language).
 */
TrackerPlugin* tracker_plugin_get_default(void);

/**
 * Set the default plugin.
 *
 * @param language_id  Language identifier to make default
 * @return             true on success, false if not found
 */
bool tracker_plugin_set_default(const char* language_id);

/**
 * List all registered plugins.
 *
 * @param count  Output: number of plugins
 * @return       Array of plugin pointers (caller does not own)
 */
TrackerPlugin** tracker_plugin_list_all(int* count);

/**
 * Check if a plugin has a capability.
 */
static inline bool tracker_plugin_has_cap(
    const TrackerPlugin* plugin,
    TrackerPluginCapabilities cap
) {
    return (plugin->capabilities & cap) != 0;
}

/*============================================================================
 * Compilation Functions
 *============================================================================*/

/**
 * Compile a cell for playback.
 * Resolves language, evaluates/compiles expression, compiles FX chain.
 *
 * @param cell            Cell to compile
 * @param default_lang_id Default language if cell has none
 * @param error_msg       Output: error message on failure
 * @return                Compiled cell (caller owns), or NULL on error
 */
CompiledCell* tracker_compile_cell(
    const TrackerCell* cell,
    const char* default_lang_id,
    const char** error_msg
);

/**
 * Free a compiled cell.
 */
void tracker_compiled_cell_free(CompiledCell* compiled);

/**
 * Compile an FX chain.
 *
 * @param chain           Source FX chain
 * @param default_lang_id Default language for FX lookup
 * @param error_msg       Output: error message on failure
 * @return                Compiled chain (caller owns), or NULL on error
 */
CompiledFxChain* tracker_compile_fx_chain(
    const TrackerFxChain* chain,
    const char* default_lang_id,
    const char** error_msg
);

/**
 * Free a compiled FX chain.
 */
void tracker_compiled_fx_chain_free(CompiledFxChain* compiled);

/**
 * Invalidate all compiled data for a pattern.
 * Call when pattern is edited.
 */
void tracker_invalidate_pattern(TrackerPattern* pattern);

/**
 * Invalidate all compiled data for a song.
 * Call when global settings change.
 */
void tracker_invalidate_song(TrackerSong* song);

/*============================================================================
 * Evaluation Functions
 *============================================================================*/

/**
 * Evaluate a compiled cell to produce a phrase.
 *
 * @param compiled  Compiled cell
 * @param ctx       Current tracker context
 * @return          New phrase (caller owns), or NULL on error
 */
TrackerPhrase* tracker_evaluate_cell(CompiledCell* compiled, TrackerContext* ctx);

/**
 * Apply a compiled FX chain to a phrase.
 *
 * @param chain   Compiled FX chain
 * @param phrase  Input phrase (consumed - do not use after call)
 * @param ctx     Current tracker context
 * @return        Transformed phrase (caller owns), or NULL on error
 */
TrackerPhrase* tracker_apply_fx_chain(
    CompiledFxChain* chain,
    TrackerPhrase* phrase,
    TrackerContext* ctx
);

/*============================================================================
 * Context Helpers
 *============================================================================*/

/**
 * Initialize a context with default values.
 */
void tracker_context_init(TrackerContext* ctx);

/**
 * Create a context from song/pattern state.
 */
void tracker_context_from_song(
    TrackerContext* ctx,
    const TrackerSong* song,
    int pattern_index,
    int row,
    int track
);

/**
 * Get a random number from context (advances random_state).
 * Returns value in range [0, max).
 */
uint32_t tracker_context_random(TrackerContext* ctx, uint32_t max);

/**
 * Get a random float from context in range [0.0, 1.0).
 */
float tracker_context_random_float(TrackerContext* ctx);

/**
 * Reseed the context's random state (for reproducibility).
 */
void tracker_context_reseed(TrackerContext* ctx, uint32_t seed);

#endif /* TRACKER_PLUGIN_H */
