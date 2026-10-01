/**
 * @file midi.c
 * @brief Shared MIDI I/O using libremidi.
 *
 * Pure MIDI port management and message sending.
 * Event routing is handled by context.c.
 */

#include "midi.h"
#include "../context.h"
#include <libremidi/libremidi-c.h>
#include <stdio.h>
#include <string.h>

/* Cross-platform sleep */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
#else
#include <unistd.h>
#endif

/* ============================================================================
 * Port Enumeration Callback
 * ============================================================================ */

typedef struct {
    SharedContext* ctx;
} EnumContext;

static void on_output_port_found(void* user_ctx, const libremidi_midi_out_port* port) {
    EnumContext* ec = (EnumContext*)user_ctx;
    SharedContext* ctx = ec->ctx;

    if (ctx->out_port_count >= SHARED_MAX_PORTS) return;

    libremidi_midi_out_port_clone(port, &ctx->out_ports[ctx->out_port_count]);
    ctx->out_port_count++;
}

/* ============================================================================
 * Observer and Port Enumeration
 * ============================================================================ */

void shared_midi_init_observer(SharedContext* ctx) {
    if (!ctx) return;

    int ret = 0;

    /* Free existing observer if any */
    if (ctx->observer != NULL) {
        for (int i = 0; i < ctx->out_port_count; i++) {
            if (ctx->out_ports[i]) {
                libremidi_midi_out_port_free(ctx->out_ports[i]);
                ctx->out_ports[i] = NULL;
            }
        }
        ctx->out_port_count = 0;
        libremidi_midi_observer_free(ctx->observer);
        ctx->observer = NULL;
    }

    /* Create observer configuration */
    libremidi_observer_configuration observer_conf;
    ret = libremidi_midi_observer_configuration_init(&observer_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to init observer config: %d\n", ret);
        return;
    }

    observer_conf.track_hardware = true;
    observer_conf.track_virtual = true;
    observer_conf.track_any = true;

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to init API config: %d\n", ret);
        return;
    }

    api_conf.configuration_type = Observer;
    api_conf.api = UNSPECIFIED;

    /* Create observer */
    ret = libremidi_midi_observer_new(&observer_conf, &api_conf, &ctx->observer);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to create observer: %d\n", ret);
        return;
    }

    /* Enumerate output ports */
    ctx->out_port_count = 0;
    EnumContext ec = { .ctx = ctx };
    ret = libremidi_midi_observer_enumerate_output_ports(ctx->observer, &ec, on_output_port_found);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to enumerate ports: %d\n", ret);
    }
}

/* Output ports every Linux desktop exposes with no synth behind them. The
 * libremidi C API gives only the port name, so PipeWire's two sequencer
 * clients are matched by their bare port name. */
static int is_system_output(const char* name, size_t len) {
    static const char through[] = "Midi Through";
    if (len >= sizeof(through) - 1 && strncmp(name, through, sizeof(through) - 1) == 0) {
        return 1;
    }
    return len == 5 && strncmp(name, "input", 5) == 0;
}

static void on_output_port_counted(void* user_ctx, const libremidi_midi_out_port* port) {
    int* count = (int*)user_ctx;
    const char* name = NULL;
    size_t len = 0;
    if (libremidi_midi_out_port_name(port, &name, &len) == 0 && name &&
        is_system_output(name, len)) {
        return;
    }
    (*count)++;
}

int shared_midi_count_synth_outputs(void) {
    libremidi_observer_configuration observer_conf;
    libremidi_api_configuration api_conf;
    libremidi_midi_observer_handle* observer = NULL;
    int count = 0;

    if (libremidi_midi_observer_configuration_init(&observer_conf) != 0 ||
        libremidi_midi_api_configuration_init(&api_conf) != 0) {
        return 0;
    }
    observer_conf.track_hardware = true;
    observer_conf.track_virtual = true;
    observer_conf.track_any = true;
    api_conf.configuration_type = Observer;
    api_conf.api = UNSPECIFIED;

    if (libremidi_midi_observer_new(&observer_conf, &api_conf, &observer) != 0) {
        return 0;
    }
    libremidi_midi_observer_enumerate_output_ports(observer, &count, on_output_port_counted);
    libremidi_midi_observer_free(observer);
    return count;
}

void shared_midi_cleanup(SharedContext* ctx) {
    if (!ctx) return;

    /* Close output */
    if (ctx->midi_out != NULL) {
        shared_midi_all_notes_off(ctx);
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }

    /* Free ports */
    for (int i = 0; i < ctx->out_port_count; i++) {
        if (ctx->out_ports[i]) {
            libremidi_midi_out_port_free(ctx->out_ports[i]);
            ctx->out_ports[i] = NULL;
        }
    }
    ctx->out_port_count = 0;

    /* Free observer */
    if (ctx->observer != NULL) {
        libremidi_midi_observer_free(ctx->observer);
        ctx->observer = NULL;
    }
}

void shared_midi_list_ports(SharedContext* ctx) {
    if (!ctx) return;

    shared_midi_init_observer(ctx);

    printf("MIDI outputs:\n");
    if (ctx->out_port_count == 0) {
        printf("  (none - use virtual port)\n");
    } else {
        for (int i = 0; i < ctx->out_port_count; i++) {
            const char* name = NULL;
            size_t len = 0;
            if (libremidi_midi_out_port_name(ctx->out_ports[i], &name, &len) == 0) {
                printf("  %d: %s\n", i, name);
            }
        }
    }
}

int shared_midi_get_port_count(SharedContext* ctx) {
    if (!ctx) return 0;
    return ctx->out_port_count;
}

const char* shared_midi_get_port_name(SharedContext* ctx, int port_idx) {
    if (!ctx || port_idx < 0 || port_idx >= ctx->out_port_count) return NULL;

    const char* name = NULL;
    size_t len = 0;
    if (libremidi_midi_out_port_name(ctx->out_ports[port_idx], &name, &len) == 0) {
        return name;
    }
    return NULL;
}

/* ============================================================================
 * Port Connection
 * ============================================================================ */

int shared_midi_open_port(SharedContext* ctx, int port_idx) {
    if (!ctx) return -1;

    shared_midi_init_observer(ctx);

    if (port_idx < 0 || port_idx >= ctx->out_port_count) {
        fprintf(stderr, "MIDI: Invalid port index: %d (have %d ports)\n",
                port_idx, ctx->out_port_count);
        return -1;
    }

    /* Close existing output */
    if (ctx->midi_out != NULL) {
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }

    int ret = 0;

    /* Create MIDI configuration */
    libremidi_midi_configuration midi_conf;
    ret = libremidi_midi_configuration_init(&midi_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to init config\n");
        return -1;
    }

    midi_conf.version = MIDI1;
    midi_conf.out_port = ctx->out_ports[port_idx];

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to init API config\n");
        return -1;
    }

    api_conf.configuration_type = Output;
    api_conf.api = UNSPECIFIED;

    /* Open output */
    ret = libremidi_midi_out_new(&midi_conf, &api_conf, &ctx->midi_out);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to open output: %d\n", ret);
        return -1;
    }

    return 0;
}

int shared_midi_open_virtual(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    shared_midi_init_observer(ctx);

    /* Close existing output */
    if (ctx->midi_out != NULL) {
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }

    int ret = 0;

    /* Create MIDI configuration */
    libremidi_midi_configuration midi_conf;
    ret = libremidi_midi_configuration_init(&midi_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to init config\n");
        return -1;
    }

    midi_conf.version = MIDI1;
    midi_conf.virtual_port = true;
    midi_conf.port_name = name;

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to init API config\n");
        return -1;
    }

    api_conf.configuration_type = Output;
    api_conf.api = UNSPECIFIED;

    /* Create virtual output */
    ret = libremidi_midi_out_new(&midi_conf, &api_conf, &ctx->midi_out);
    if (ret != 0) {
        fprintf(stderr, "MIDI: Failed to create virtual output: %d\n", ret);
        return -1;
    }

    return 0;
}

int shared_midi_open_by_name(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    shared_midi_init_observer(ctx);

    /* Search for substring match in hardware port names */
    for (int i = 0; i < ctx->out_port_count; i++) {
        const char* port_name = NULL;
        size_t len = 0;
        if (libremidi_midi_out_port_name(ctx->out_ports[i], &port_name, &len) == 0) {
            if (strstr(port_name, name) != NULL) {
                return shared_midi_open_port(ctx, i);
            }
        }
    }

    /* No hardware port matched - create virtual port */
    return shared_midi_open_virtual(ctx, name);
}

int shared_midi_open_auto(SharedContext* ctx, const char* virtual_name) {
    if (!ctx) return -1;

    shared_midi_init_observer(ctx);

    /* If hardware ports available, open the first one */
    if (ctx->out_port_count > 0) {
        return shared_midi_open_port(ctx, 0);
    }

    /* No hardware ports - create virtual port */
    return shared_midi_open_virtual(ctx, virtual_name);
}

void shared_midi_close(SharedContext* ctx) {
    if (!ctx) return;

    if (ctx->midi_out != NULL) {
        shared_midi_all_notes_off(ctx);
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }
}

int shared_midi_is_open(SharedContext* ctx) {
    return ctx && ctx->midi_out != NULL;
}

/* ============================================================================
 * Message Sending (raw MIDI, no routing)
 * ============================================================================ */

void shared_midi_send_note_on(SharedContext* ctx, int channel, int pitch, int velocity) {
    if (!ctx || !ctx->midi_out) return;

    unsigned char msg[3];
    msg[0] = 0x90 | ((channel - 1) & 0x0F);
    msg[1] = pitch & 0x7F;
    msg[2] = velocity & 0x7F;
    libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
}

void shared_midi_send_note_off(SharedContext* ctx, int channel, int pitch) {
    if (!ctx || !ctx->midi_out) return;

    unsigned char msg[3];
    msg[0] = 0x80 | ((channel - 1) & 0x0F);
    msg[1] = pitch & 0x7F;
    msg[2] = 0;
    libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
}

void shared_midi_send_program(SharedContext* ctx, int channel, int program) {
    if (!ctx || !ctx->midi_out) return;

    unsigned char msg[2];
    msg[0] = 0xC0 | ((channel - 1) & 0x0F);
    msg[1] = program & 0x7F;
    libremidi_midi_out_send_message(ctx->midi_out, msg, 2);
}

void shared_midi_send_cc(SharedContext* ctx, int channel, int cc, int value) {
    if (!ctx || !ctx->midi_out) return;

    unsigned char msg[3];
    msg[0] = 0xB0 | ((channel - 1) & 0x0F);
    msg[1] = cc & 0x7F;
    msg[2] = value & 0x7F;
    libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
}

void shared_midi_all_notes_off(SharedContext* ctx) {
    if (!ctx || !ctx->midi_out) return;

    /* Send All Notes Off (CC 123) on all channels */
    for (int ch = 0; ch < 16; ch++) {
        unsigned char msg[3];
        msg[0] = 0xB0 | ch;
        msg[1] = 123;  /* All Notes Off */
        msg[2] = 0;
        libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
    }
}
