/**
 * @file minihost_backend.c
 * @brief VST3/AU plugin host backend implementation.
 *
 * Uses libminihost for plugin hosting. When BUILD_MINIHOST_BACKEND is not
 * defined, all functions return stub values indicating unavailability.
 */

#include "minihost_backend.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define dup _dup
#define dup2 _dup2
#define close _close
#define open _open
#define O_WRONLY _O_WRONLY
#define O_CREAT _O_CREAT
#define O_TRUNC _O_TRUNC
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#define DEV_NULL "NUL"
#else
#include <unistd.h>
#include <fcntl.h>
#define DEV_NULL "/dev/null"
#endif

#ifdef BUILD_MINIHOST_BACKEND

#include "minihost.h"

/* miniaudio for audio output (implementation is in tsf_backend.c) */
#include "miniaudio.h"

/* ============================================================================
 * Stderr Redirection (suppress plugin debug output)
 * ============================================================================ */

static int g_original_stderr = -1;
static int g_redirect_fd = -1;
static char g_log_path[512] = {0};

static void redirect_stderr_to_null(void) {
    if (g_original_stderr != -1) return;  /* Already redirected */

    /* Save original stderr */
    g_original_stderr = dup(STDERR_FILENO);

    /* Open /dev/null or log file */
    if (g_log_path[0] != '\0') {
        g_redirect_fd = open(g_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    } else {
        g_redirect_fd = open(DEV_NULL, O_WRONLY);
    }

    if (g_redirect_fd != -1) {
        dup2(g_redirect_fd, STDERR_FILENO);
    }
}

static void restore_stderr(void) {
    if (g_original_stderr == -1) return;

    dup2(g_original_stderr, STDERR_FILENO);
    close(g_original_stderr);
    g_original_stderr = -1;

    if (g_redirect_fd != -1) {
        close(g_redirect_fd);
        g_redirect_fd = -1;
    }
}

void shared_minihost_set_log_file(const char* path) {
    if (path) {
        strncpy(g_log_path, path, sizeof(g_log_path) - 1);
        g_log_path[sizeof(g_log_path) - 1] = '\0';
    } else {
        g_log_path[0] = '\0';
    }
}

/* Cross-platform mutex */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef CRITICAL_SECTION mh_mutex_t;
static inline int mh_mutex_init(mh_mutex_t* m) { InitializeCriticalSection(m); return 0; }
static inline void mh_mutex_destroy(mh_mutex_t* m) { DeleteCriticalSection(m); }
static inline void mh_mutex_lock(mh_mutex_t* m) { EnterCriticalSection(m); }
static inline void mh_mutex_unlock(mh_mutex_t* m) { LeaveCriticalSection(m); }
#else
#include <pthread.h>
typedef pthread_mutex_t mh_mutex_t;
static inline int mh_mutex_init(mh_mutex_t* m) { return pthread_mutex_init(m, NULL); }
static inline void mh_mutex_destroy(mh_mutex_t* m) { pthread_mutex_destroy(m); }
static inline void mh_mutex_lock(mh_mutex_t* m) { pthread_mutex_lock(m); }
static inline void mh_mutex_unlock(mh_mutex_t* m) { pthread_mutex_unlock(m); }
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MH_SAMPLE_RATE      44100
#define MH_CHANNELS         2
#define MH_BLOCK_SIZE       512
#define MH_MIDI_QUEUE_SIZE  256

/* ============================================================================
 * Backend State (global singleton)
 * ============================================================================ */

typedef struct {
    MH_Plugin* plugins[MINIHOST_MAX_PLUGINS];
    char* plugin_names[MINIHOST_MAX_PLUGINS];
    int enabled;
    int initialized;
    int ref_count;
    double sample_rate;
    int block_size;

    /* Audio buffers (non-interleaved) */
    float* input_buf[MH_CHANNELS];
    float* output_buf[MH_CHANNELS];

    /* MIDI event queue for audio callback */
    MH_MidiEvent midi_queue[MH_MIDI_QUEUE_SIZE];
    int midi_queue_count;
    mh_mutex_t midi_mutex;
    mh_mutex_t process_mutex;

    /* miniaudio device for audio output */
    ma_device audio_device;
    int audio_device_initialized;
} MinihostBackend;

static MinihostBackend g_mh = {0};

/* ============================================================================
 * Audio Callback
 * ============================================================================ */

static void minihost_audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    float* out = (float*)output;

    /* Clear output buffer */
    memset(out, 0, frame_count * MH_CHANNELS * sizeof(float));

    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    /* Process in blocks */
    ma_uint32 frames_remaining = frame_count;
    ma_uint32 offset = 0;

    while (frames_remaining > 0) {
        int frames_to_process = (int)(frames_remaining > (ma_uint32)g_mh.block_size ?
                                      (ma_uint32)g_mh.block_size : frames_remaining);

        mh_mutex_lock(&g_mh.process_mutex);

        /* Drain MIDI queue */
        mh_mutex_lock(&g_mh.midi_mutex);
        MH_MidiEvent events[MH_MIDI_QUEUE_SIZE];
        int event_count = g_mh.midi_queue_count;
        if (event_count > 0) {
            memcpy(events, g_mh.midi_queue, event_count * sizeof(MH_MidiEvent));
            g_mh.midi_queue_count = 0;
        }
        mh_mutex_unlock(&g_mh.midi_mutex);

        /* Clear non-interleaved output buffers */
        memset(g_mh.output_buf[0], 0, frames_to_process * sizeof(float));
        memset(g_mh.output_buf[1], 0, frames_to_process * sizeof(float));

        /* Process instrument (slot 0) with MIDI */
        if (g_mh.plugins[0]) {
            mh_process_midi(g_mh.plugins[0],
                (const float* const*)g_mh.input_buf,
                g_mh.output_buf,
                frames_to_process,
                events, event_count);
        }

        /* Process effects chain (slots 1-7) */
        for (int i = 1; i < MINIHOST_MAX_PLUGINS; i++) {
            if (g_mh.plugins[i]) {
                /* Copy output to input for next plugin */
                memcpy(g_mh.input_buf[0], g_mh.output_buf[0], frames_to_process * sizeof(float));
                memcpy(g_mh.input_buf[1], g_mh.output_buf[1], frames_to_process * sizeof(float));

                /* Process */
                mh_process(g_mh.plugins[i],
                    (const float* const*)g_mh.input_buf,
                    g_mh.output_buf,
                    frames_to_process);
            }
        }

        /* Interleave output */
        float* left = g_mh.output_buf[0];
        float* right = g_mh.output_buf[1];
        for (int i = 0; i < frames_to_process; i++) {
            out[(offset + i) * 2] = left[i];
            out[(offset + i) * 2 + 1] = right[i];
        }

        mh_mutex_unlock(&g_mh.process_mutex);

        offset += frames_to_process;
        frames_remaining -= frames_to_process;
    }
}

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int shared_minihost_init(void) {
    if (g_mh.initialized) {
        return 0;
    }

    memset(&g_mh, 0, sizeof(g_mh));

    g_mh.sample_rate = MH_SAMPLE_RATE;
    g_mh.block_size = MH_BLOCK_SIZE;

    /* Initialize mutexes */
    if (mh_mutex_init(&g_mh.midi_mutex) != 0) {
        fprintf(stderr, "Minihost: Failed to create MIDI mutex\n");
        return -1;
    }

    if (mh_mutex_init(&g_mh.process_mutex) != 0) {
        mh_mutex_destroy(&g_mh.midi_mutex);
        fprintf(stderr, "Minihost: Failed to create process mutex\n");
        return -1;
    }

    /* Allocate audio buffers (non-interleaved for minihost) */
    for (int i = 0; i < MH_CHANNELS; i++) {
        g_mh.input_buf[i] = (float*)calloc(g_mh.block_size, sizeof(float));
        g_mh.output_buf[i] = (float*)calloc(g_mh.block_size, sizeof(float));
        if (!g_mh.input_buf[i] || !g_mh.output_buf[i]) {
            fprintf(stderr, "Minihost: Failed to allocate audio buffers\n");
            shared_minihost_cleanup();
            return -1;
        }
    }

    g_mh.initialized = 1;
    return 0;
}

void shared_minihost_cleanup(void) {
    if (!g_mh.initialized) {
        return;
    }

    /* Disable and stop audio */
    shared_minihost_disable();

    /* Uninitialize audio device */
    if (g_mh.audio_device_initialized) {
        ma_device_uninit(&g_mh.audio_device);
        g_mh.audio_device_initialized = 0;
    }

    /* Unload all plugins */
    for (int i = 0; i < MINIHOST_MAX_PLUGINS; i++) {
        shared_minihost_unload(i);
    }

    /* Free audio buffers */
    for (int i = 0; i < MH_CHANNELS; i++) {
        free(g_mh.input_buf[i]);
        free(g_mh.output_buf[i]);
        g_mh.input_buf[i] = NULL;
        g_mh.output_buf[i] = NULL;
    }

    mh_mutex_destroy(&g_mh.midi_mutex);
    mh_mutex_destroy(&g_mh.process_mutex);

    /* Restore stderr */
    restore_stderr();

    g_mh.initialized = 0;
}

int shared_minihost_is_available(void) {
    return 1;
}

/* ============================================================================
 * Plugin Loading
 * ============================================================================ */

int shared_minihost_load(int slot, const char* path) {
    if (!g_mh.initialized) {
        fprintf(stderr, "Minihost: Backend not initialized\n");
        return -1;
    }

    if (slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        fprintf(stderr, "Minihost: Invalid slot %d\n", slot);
        return -1;
    }

    if (!path) {
        fprintf(stderr, "Minihost: NULL path\n");
        return -1;
    }

    /* Unload existing plugin in this slot */
    shared_minihost_unload(slot);

    mh_mutex_lock(&g_mh.process_mutex);

    /* Redirect stderr to suppress plugin debug output */
    redirect_stderr_to_null();

    /* Load the plugin */
    char err_buf[256] = {0};
    MH_Plugin* plugin = mh_open(path,
                                g_mh.sample_rate,
                                g_mh.block_size,
                                MH_CHANNELS,  /* input channels */
                                MH_CHANNELS,  /* output channels */
                                err_buf,
                                sizeof(err_buf));

    if (!plugin) {
        mh_mutex_unlock(&g_mh.process_mutex);
        /* No plugin to silence; without this the error below goes to /dev/null */
        restore_stderr();
        fprintf(stderr, "Minihost: Failed to load plugin: %s\n",
                err_buf[0] ? err_buf : path);
        return -1;
    }

    g_mh.plugins[slot] = plugin;

    /* Get plugin name via probe */
    MH_PluginDesc desc;
    if (mh_probe(path, &desc, NULL, 0)) {
        g_mh.plugin_names[slot] = strdup(desc.name);
    } else {
        /* Fallback: use filename */
        const char* filename = strrchr(path, '/');
        g_mh.plugin_names[slot] = strdup(filename ? filename + 1 : path);
    }

    mh_mutex_unlock(&g_mh.process_mutex);

    return 0;
}

void shared_minihost_unload(int slot) {
    if (!g_mh.initialized) {
        return;
    }

    if (slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return;
    }

    mh_mutex_lock(&g_mh.process_mutex);

    if (g_mh.plugins[slot]) {
        mh_close(g_mh.plugins[slot]);
        g_mh.plugins[slot] = NULL;
    }

    if (g_mh.plugin_names[slot]) {
        free(g_mh.plugin_names[slot]);
        g_mh.plugin_names[slot] = NULL;
    }

    mh_mutex_unlock(&g_mh.process_mutex);
}

int shared_minihost_has_plugin(int slot) {
    if (!g_mh.initialized) {
        return 0;
    }

    if (slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return 0;
    }

    return g_mh.plugins[slot] != NULL;
}

const char* shared_minihost_get_plugin_name(int slot) {
    if (!g_mh.initialized) {
        return NULL;
    }

    if (slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return NULL;
    }

    return g_mh.plugin_names[slot];
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

int shared_minihost_enable(void) {
    /* Auto-initialize if needed */
    if (!g_mh.initialized) {
        if (shared_minihost_init() != 0) {
            return -1;
        }
    }

    /* Increment reference count */
    g_mh.ref_count++;

    if (g_mh.enabled) {
        return 0;
    }

    /* Initialize audio device if not already done */
    if (!g_mh.audio_device_initialized) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = MH_CHANNELS;
        config.sampleRate = (ma_uint32)g_mh.sample_rate;
        config.dataCallback = minihost_audio_callback;
        config.pUserData = NULL;
        config.periodSizeInFrames = g_mh.block_size;

        if (ma_device_init(NULL, &config, &g_mh.audio_device) != MA_SUCCESS) {
            fprintf(stderr, "Minihost: Failed to initialize audio device\n");
            return -1;
        }

        g_mh.audio_device_initialized = 1;
    }

    /* Start audio device */
    if (ma_device_start(&g_mh.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "Minihost: Failed to start audio device\n");
        return -1;
    }

    g_mh.enabled = 1;
    return 0;
}

void shared_minihost_disable(void) {
    if (!g_mh.initialized || g_mh.ref_count <= 0) {
        return;
    }

    g_mh.ref_count--;

    if (g_mh.ref_count > 0) {
        return;
    }

    if (!g_mh.enabled) {
        return;
    }

    /* Stop all notes */
    shared_minihost_all_notes_off();

    /* Stop audio device */
    if (g_mh.audio_device_initialized) {
        ma_device_stop(&g_mh.audio_device);
    }

    g_mh.enabled = 0;
}

int shared_minihost_is_enabled(void) {
    return g_mh.initialized && g_mh.enabled;
}

/* ============================================================================
 * MIDI Event Dispatch
 * ============================================================================ */

static void queue_midi_event(unsigned char status, unsigned char data1, unsigned char data2) {
    mh_mutex_lock(&g_mh.midi_mutex);

    if (g_mh.midi_queue_count < MH_MIDI_QUEUE_SIZE) {
        MH_MidiEvent* ev = &g_mh.midi_queue[g_mh.midi_queue_count++];
        ev->sample_offset = 0;
        ev->status = status;
        ev->data1 = data1;
        ev->data2 = data2;
    }

    mh_mutex_unlock(&g_mh.midi_mutex);
}

void shared_minihost_send_note_on(int channel, int pitch, int velocity) {
    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    /* Channel is 1-16, MIDI uses 0-15 */
    int ch = (channel - 1) & 0x0F;
    unsigned char status = 0x90 | ch;

    queue_midi_event(status, pitch & 0x7F, velocity & 0x7F);
}

void shared_minihost_send_note_off(int channel, int pitch) {
    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    /* Channel is 1-16, MIDI uses 0-15 */
    int ch = (channel - 1) & 0x0F;
    unsigned char status = 0x80 | ch;

    queue_midi_event(status, pitch & 0x7F, 0);
}

void shared_minihost_send_cc(int channel, int cc, int value) {
    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    int ch = (channel - 1) & 0x0F;
    unsigned char status = 0xB0 | ch;

    queue_midi_event(status, cc & 0x7F, value & 0x7F);
}

void shared_minihost_send_program(int channel, int program) {
    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    int ch = (channel - 1) & 0x0F;
    unsigned char status = 0xC0 | ch;

    queue_midi_event(status, program & 0x7F, 0);
}

void shared_minihost_send_pitch_bend(int channel, int bend) {
    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    int ch = (channel - 1) & 0x0F;
    unsigned char status = 0xE0 | ch;

    /* Convert -8192..8191 to 14-bit value 0..16383 */
    int value = bend + 8192;
    if (value < 0) value = 0;
    if (value > 16383) value = 16383;

    queue_midi_event(status, value & 0x7F, (value >> 7) & 0x7F);
}

void shared_minihost_all_notes_off(void) {
    if (!g_mh.enabled || !g_mh.plugins[0]) {
        return;
    }

    /* Send CC 123 (All Notes Off) on all channels */
    for (int ch = 1; ch <= 16; ch++) {
        shared_minihost_send_cc(ch, 123, 0);
    }
}

/* ============================================================================
 * Parameter Control
 * ============================================================================ */

int shared_minihost_get_num_params(int slot) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return 0;
    }

    if (!g_mh.plugins[slot]) {
        return 0;
    }

    return mh_get_num_params(g_mh.plugins[slot]);
}

float shared_minihost_get_param(int slot, int index) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return 0.0f;
    }

    if (!g_mh.plugins[slot]) {
        return 0.0f;
    }

    return mh_get_param(g_mh.plugins[slot], index);
}

int shared_minihost_set_param(int slot, int index, float value) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot]) {
        return -1;
    }

    return mh_set_param(g_mh.plugins[slot], index, value) ? 0 : -1;
}

int shared_minihost_get_param_name(int slot, int index, char* buf, int size) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot] || !buf || size <= 0) {
        return -1;
    }

    MH_ParamInfo info;
    if (!mh_get_param_info(g_mh.plugins[slot], index, &info)) {
        buf[0] = '\0';
        return -1;
    }

    strncpy(buf, info.name, size - 1);
    buf[size - 1] = '\0';
    return 0;
}

/* ============================================================================
 * Preset/Program Control
 * ============================================================================ */

int shared_minihost_get_num_presets(int slot) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return 0;
    }

    if (!g_mh.plugins[slot]) {
        return 0;
    }

    return mh_get_num_programs(g_mh.plugins[slot]);
}

int shared_minihost_get_preset(int slot) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot]) {
        return -1;
    }

    return mh_get_program(g_mh.plugins[slot]);
}

int shared_minihost_set_preset(int slot, int index) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot]) {
        return -1;
    }

    return mh_set_program(g_mh.plugins[slot], index) ? 0 : -1;
}

int shared_minihost_get_preset_name(int slot, int index, char* buf, int size) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot] || !buf || size <= 0) {
        return -1;
    }

    return mh_get_program_name(g_mh.plugins[slot], index, buf, size) ? 0 : -1;
}

/* ============================================================================
 * State Persistence
 * ============================================================================ */

int shared_minihost_save_state(int slot, const char* path) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot] || !path) {
        return -1;
    }

    /* Get state size */
    int state_size = mh_get_state_size(g_mh.plugins[slot]);
    if (state_size <= 0) {
        return -1;
    }

    /* Allocate buffer and get state */
    void* state_data = malloc(state_size);
    if (!state_data) {
        return -1;
    }

    if (!mh_get_state(g_mh.plugins[slot], state_data, state_size)) {
        free(state_data);
        return -1;
    }

    /* Write to file */
    FILE* f = fopen(path, "wb");
    if (!f) {
        free(state_data);
        return -1;
    }

    size_t written = fwrite(state_data, 1, state_size, f);
    fclose(f);
    free(state_data);

    return (written == (size_t)state_size) ? 0 : -1;
}

int shared_minihost_load_state(int slot, const char* path) {
    if (!g_mh.initialized || slot < 0 || slot >= MINIHOST_MAX_PLUGINS) {
        return -1;
    }

    if (!g_mh.plugins[slot] || !path) {
        return -1;
    }

    /* Read file */
    FILE* f = fopen(path, "rb");
    if (!f) {
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return -1;
    }

    void* state_data = malloc(file_size);
    if (!state_data) {
        fclose(f);
        return -1;
    }

    size_t read_size = fread(state_data, 1, file_size, f);
    fclose(f);

    if (read_size != (size_t)file_size) {
        free(state_data);
        return -1;
    }

    /* Restore state */
    int result = mh_set_state(g_mh.plugins[slot], state_data, (int)file_size) ? 0 : -1;
    free(state_data);

    return result;
}

/* ============================================================================
 * Plugin Scanning
 * ============================================================================ */

/* Adapter for scan callback */
typedef struct {
    minihost_scan_callback user_callback;
    void* user_data;
} scan_adapter_data;

static void scan_callback_adapter(const MH_PluginDesc* desc, void* user_data) {
    scan_adapter_data* adapter = (scan_adapter_data*)user_data;
    if (adapter->user_callback) {
        adapter->user_callback(desc->name, desc->path, adapter->user_data);
    }
}

int shared_minihost_scan_directory(const char* path,
    minihost_scan_callback callback, void* userdata) {

    if (!g_mh.initialized || !path || !callback) {
        return -1;
    }

    scan_adapter_data adapter = { callback, userdata };
    return mh_scan_directory(path, scan_callback_adapter, &adapter);
}

/* ============================================================================
 * Audio Processing
 * ============================================================================ */

void shared_minihost_process_block(float* output, int frames) {
    if (!g_mh.enabled || !output) {
        return;
    }

    mh_mutex_lock(&g_mh.process_mutex);

    /* Drain MIDI queue */
    mh_mutex_lock(&g_mh.midi_mutex);
    MH_MidiEvent events[MH_MIDI_QUEUE_SIZE];
    int event_count = g_mh.midi_queue_count;
    if (event_count > 0) {
        memcpy(events, g_mh.midi_queue, event_count * sizeof(MH_MidiEvent));
        g_mh.midi_queue_count = 0;
    }
    mh_mutex_unlock(&g_mh.midi_mutex);

    /* Clear output buffers */
    float* left = g_mh.output_buf[0];
    float* right = g_mh.output_buf[1];
    memset(left, 0, frames * sizeof(float));
    memset(right, 0, frames * sizeof(float));

    /* Process instrument (slot 0) with MIDI */
    if (g_mh.plugins[0]) {
        mh_process_midi(g_mh.plugins[0],
            (const float* const*)g_mh.input_buf,
            g_mh.output_buf,
            frames,
            events, event_count);
    }

    /* Process effects chain (slots 1-7) */
    for (int i = 1; i < MINIHOST_MAX_PLUGINS; i++) {
        if (g_mh.plugins[i]) {
            /* Copy output to input for next plugin */
            memcpy(g_mh.input_buf[0], left, frames * sizeof(float));
            memcpy(g_mh.input_buf[1], right, frames * sizeof(float));

            /* Process */
            mh_process(g_mh.plugins[i],
                (const float* const*)g_mh.input_buf,
                g_mh.output_buf,
                frames);
        }
    }

    /* Mix into output buffer (interleaved stereo) */
    for (int i = 0; i < frames; i++) {
        output[i * 2] += left[i];
        output[i * 2 + 1] += right[i];
    }

    mh_mutex_unlock(&g_mh.process_mutex);
}

int shared_minihost_get_sample_rate(void) {
    return (int)g_mh.sample_rate;
}

void shared_minihost_set_sample_rate(int rate) {
    if (rate > 0) {
        g_mh.sample_rate = rate;
        /* Update sample rate for all loaded plugins */
        for (int i = 0; i < MINIHOST_MAX_PLUGINS; i++) {
            if (g_mh.plugins[i]) {
                mh_set_sample_rate(g_mh.plugins[i], rate);
            }
        }
    }
}

#else /* BUILD_MINIHOST_BACKEND not defined */

/* ============================================================================
 * Stub implementations when minihost is not available
 * ============================================================================ */

int shared_minihost_init(void) {
    return -1;
}

void shared_minihost_cleanup(void) {
    /* No-op */
}

int shared_minihost_is_available(void) {
    return 0;
}

void shared_minihost_set_log_file(const char* path) {
    (void)path;
}

int shared_minihost_load(int slot, const char* path) {
    (void)slot;
    (void)path;
    fprintf(stderr, "Minihost: Not compiled in (BUILD_MINIHOST_BACKEND=OFF)\n");
    return -1;
}

void shared_minihost_unload(int slot) {
    (void)slot;
}

int shared_minihost_has_plugin(int slot) {
    (void)slot;
    return 0;
}

const char* shared_minihost_get_plugin_name(int slot) {
    (void)slot;
    return NULL;
}

int shared_minihost_enable(void) {
    return -1;
}

void shared_minihost_disable(void) {
    /* No-op */
}

int shared_minihost_is_enabled(void) {
    return 0;
}

void shared_minihost_send_note_on(int channel, int pitch, int velocity) {
    (void)channel;
    (void)pitch;
    (void)velocity;
}

void shared_minihost_send_note_off(int channel, int pitch) {
    (void)channel;
    (void)pitch;
}

void shared_minihost_send_cc(int channel, int cc, int value) {
    (void)channel;
    (void)cc;
    (void)value;
}

void shared_minihost_send_program(int channel, int program) {
    (void)channel;
    (void)program;
}

void shared_minihost_send_pitch_bend(int channel, int bend) {
    (void)channel;
    (void)bend;
}

void shared_minihost_all_notes_off(void) {
    /* No-op */
}

int shared_minihost_get_num_params(int slot) {
    (void)slot;
    return 0;
}

float shared_minihost_get_param(int slot, int index) {
    (void)slot;
    (void)index;
    return 0.0f;
}

int shared_minihost_set_param(int slot, int index, float value) {
    (void)slot;
    (void)index;
    (void)value;
    return -1;
}

int shared_minihost_get_param_name(int slot, int index, char* buf, int size) {
    (void)slot;
    (void)index;
    (void)buf;
    (void)size;
    return -1;
}

int shared_minihost_get_num_presets(int slot) {
    (void)slot;
    return 0;
}

int shared_minihost_get_preset(int slot) {
    (void)slot;
    return -1;
}

int shared_minihost_set_preset(int slot, int index) {
    (void)slot;
    (void)index;
    return -1;
}

int shared_minihost_get_preset_name(int slot, int index, char* buf, int size) {
    (void)slot;
    (void)index;
    (void)buf;
    (void)size;
    return -1;
}

int shared_minihost_save_state(int slot, const char* path) {
    (void)slot;
    (void)path;
    return -1;
}

int shared_minihost_load_state(int slot, const char* path) {
    (void)slot;
    (void)path;
    return -1;
}

int shared_minihost_scan_directory(const char* path,
    minihost_scan_callback callback, void* userdata) {
    (void)path;
    (void)callback;
    (void)userdata;
    return -1;
}

void shared_minihost_process_block(float* output, int frames) {
    (void)output;
    (void)frames;
}

int shared_minihost_get_sample_rate(void) {
    return 44100;
}

void shared_minihost_set_sample_rate(int rate) {
    (void)rate;
}

#endif /* BUILD_MINIHOST_BACKEND */
