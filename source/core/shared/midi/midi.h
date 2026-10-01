/**
 * @file midi.h
 * @brief Shared MIDI I/O API using libremidi.
 *
 * Provides port enumeration, connection management, and message sending.
 * MIDI handles are stored per-context in SharedContext.
 */

#ifndef SHARED_MIDI_H
#define SHARED_MIDI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct SharedContext;

/* ============================================================================
 * Observer and Port Enumeration
 * ============================================================================ */

/**
 * @brief Initialize MIDI observer for port enumeration.
 * @param ctx Shared context.
 */
void shared_midi_init_observer(struct SharedContext* ctx);

/**
 * @brief Cleanup MIDI resources (observer and output).
 * @param ctx Shared context.
 */
void shared_midi_cleanup(struct SharedContext* ctx);

/**
 * @brief List available MIDI output ports to stdout.
 * @param ctx Shared context.
 */
void shared_midi_list_ports(struct SharedContext* ctx);

/**
 * @brief Get the number of available output ports.
 * @param ctx Shared context.
 * @return Number of available ports.
 */
int shared_midi_get_port_count(struct SharedContext* ctx);

/**
 * @brief Count output ports that could reach a synth.
 *
 * Skips system ports that make no sound: ALSA's "Midi Through" loopback and
 * PipeWire's "input" sequencer clients. Needs no SharedContext.
 * @return Number of such ports, or 0 if enumeration fails.
 */
int shared_midi_count_synth_outputs(void);

/**
 * @brief Get the name of a MIDI output port.
 * @param ctx Shared context.
 * @param port_idx Port index.
 * @return Port name, or NULL if invalid index.
 */
const char* shared_midi_get_port_name(struct SharedContext* ctx, int port_idx);

/* ============================================================================
 * Port Connection
 * ============================================================================ */

/**
 * @brief Open a MIDI output port by index.
 * @param ctx Shared context.
 * @param port_idx Port index (0-based).
 * @return 0 on success, -1 on error.
 */
int shared_midi_open_port(struct SharedContext* ctx, int port_idx);

/**
 * @brief Open a virtual MIDI output port.
 * @param ctx Shared context.
 * @param name Name for the virtual port.
 * @return 0 on success, -1 on error.
 */
int shared_midi_open_virtual(struct SharedContext* ctx, const char* name);

/**
 * @brief Open a MIDI port by name (substring match).
 * @param ctx Shared context.
 * @param name Port name to search for.
 * @return 0 on success, -1 if not found.
 */
int shared_midi_open_by_name(struct SharedContext* ctx, const char* name);

/**
 * @brief Auto-connect: try external port first, fall back to virtual.
 * @param ctx Shared context.
 * @param virtual_name Name for virtual port if needed.
 * @return 0 on success, -1 on error.
 */
int shared_midi_open_auto(struct SharedContext* ctx, const char* virtual_name);

/**
 * @brief Close the current MIDI output port.
 * @param ctx Shared context.
 */
void shared_midi_close(struct SharedContext* ctx);

/**
 * @brief Check if a MIDI port is open.
 * @param ctx Shared context.
 * @return Non-zero if open, 0 if closed.
 */
int shared_midi_is_open(struct SharedContext* ctx);

/* ============================================================================
 * Message Sending
 * ============================================================================ */

/**
 * @brief Send a note-on message.
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void shared_midi_send_note_on(struct SharedContext* ctx, int channel, int pitch, int velocity);

/**
 * @brief Send a note-off message.
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 */
void shared_midi_send_note_off(struct SharedContext* ctx, int channel, int pitch);

/**
 * @brief Send a program change message.
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param program Program number (0-127).
 */
void shared_midi_send_program(struct SharedContext* ctx, int channel, int program);

/**
 * @brief Send a control change message.
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 */
void shared_midi_send_cc(struct SharedContext* ctx, int channel, int cc, int value);

/**
 * @brief Send all notes off on all channels.
 * @param ctx Shared context.
 */
void shared_midi_all_notes_off(struct SharedContext* ctx);

/* ============================================================================
 * MIDI Input (for parameter CC binding)
 * ============================================================================ */

/**
 * @brief Initialize MIDI input observer for port enumeration.
 * @param ctx Shared context.
 */
void shared_midi_in_init_observer(struct SharedContext* ctx);

/**
 * @brief List available MIDI input ports to stdout.
 * @param ctx Shared context.
 */
void shared_midi_in_list_ports(struct SharedContext* ctx);

/**
 * @brief Get the number of available input ports.
 * @param ctx Shared context.
 * @return Number of available ports.
 */
int shared_midi_in_get_port_count(struct SharedContext* ctx);

/**
 * @brief Get the name of a MIDI input port.
 * @param ctx Shared context.
 * @param port_idx Port index.
 * @return Port name, or NULL if invalid index.
 */
const char* shared_midi_in_get_port_name(struct SharedContext* ctx, int port_idx);

/**
 * @brief Open a MIDI input port by index.
 *
 * CC messages received on this port will update bound parameters.
 *
 * @param ctx Shared context.
 * @param port_idx Port index (0-based).
 * @return 0 on success, -1 on error.
 */
int shared_midi_in_open_port(struct SharedContext* ctx, int port_idx);

/**
 * @brief Open a virtual MIDI input port.
 * @param ctx Shared context.
 * @param name Name for the virtual port.
 * @return 0 on success, -1 on error.
 */
int shared_midi_in_open_virtual(struct SharedContext* ctx, const char* name);

/**
 * @brief Close the current MIDI input port.
 * @param ctx Shared context.
 */
void shared_midi_in_close(struct SharedContext* ctx);

/**
 * @brief Check if a MIDI input port is open.
 * @param ctx Shared context.
 * @return Non-zero if open, 0 if closed.
 */
int shared_midi_in_is_open(struct SharedContext* ctx);

/**
 * @brief Cleanup MIDI input resources.
 * @param ctx Shared context.
 */
void shared_midi_in_cleanup(struct SharedContext* ctx);

/**
 * @brief MIDI note callback type.
 * Called when note-on or note-off is received.
 * @param user_data User data passed to callback.
 * @param channel MIDI channel (1-16).
 * @param note MIDI note number (0-127).
 * @param velocity Note velocity (0-127).
 * @param is_note_on Non-zero for note-on, 0 for note-off.
 */
typedef void (*SharedMidiNoteCallback)(void* user_data, int channel, int note, int velocity, int is_note_on);

/**
 * @brief Register a callback for MIDI note messages.
 * @param ctx Shared context.
 * @param callback Callback function, or NULL to unregister.
 * @param user_data User data passed to callback.
 */
void shared_midi_set_note_callback(struct SharedContext* ctx, SharedMidiNoteCallback callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_MIDI_H */
