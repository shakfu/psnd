#ifndef CPROLOG_SCHEDULER_H
#define CPROLOG_SCHEDULER_H

#include <stdint.h>

#include "bog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* userdata;
    void (*init)(void* userdata);
    double (*time)(void* userdata);
    void (*kick)(void* userdata, double time, double velocity);
    void (*snare)(void* userdata, double time, double velocity);
    void (*hat)(void* userdata, double time, double velocity);
    void (*clap)(void* userdata, double time, double velocity);
    void (*sine)(void* userdata, double time, double midi, double velocity);
    void (*square)(void* userdata, double time, double midi, double velocity);
    void (*triangle)(void* userdata, double time, double midi,
                     double velocity);
    void (*noise)(void* userdata, double time, double velocity);
    /* Receives every event, any voice atom, when set; the named callbacks
     * above are then unused */
    void (*note)(void* userdata, const char* voice, double time,
                 double midi, double velocity);
} BogAudioCallbacks;

typedef void (*BogBeatCallback)(int beat, void* userdata);

typedef struct BogScheduler BogScheduler;
typedef struct BogTransitionManager BogTransitionManager;

/* State manager API */
BogStateManager* bog_state_manager_create(void);
void bog_state_manager_destroy(BogStateManager* manager);
void bog_state_manager_reset(BogStateManager* manager);
size_t bog_state_manager_get_cycle(const BogStateManager* manager,
                                       const char* key);
size_t bog_state_manager_increment_cycle(BogStateManager* manager,
                                             const char* key,
                                             size_t list_length);
double
bog_state_manager_get_last_trigger(const BogStateManager* manager,
                                       const char* key, bool* found);
void bog_state_manager_set_last_trigger(BogStateManager* manager,
                                            const char* key,
                                            double time_value);
bool bog_state_manager_can_trigger(const BogStateManager* manager,
                                       const char* key, double now,
                                       double gap);
/* Random stream for prob/pick/rand/randint. Seeded from the clock at create. */
void bog_state_manager_seed(BogStateManager* manager, uint64_t seed);
double bog_state_manager_random(BogStateManager* manager); /* [0, 1) */

/* Scheduler API */
BogScheduler* bog_scheduler_create(const BogAudioCallbacks* audio,
                                           const BogBuiltins* builtins,
                                           BogStateManager* state_manager);
void bog_scheduler_destroy(BogScheduler* scheduler);
void bog_scheduler_set_program(BogScheduler* scheduler,
                                   const BogProgram* program);
const BogProgram* bog_scheduler_get_program(const BogScheduler* scheduler);
/* Hold queried events and call back when now reaches their time, for hosts
 * whose output cannot be timestamped. Off by default. */
void bog_scheduler_set_dispatch_on_time(BogScheduler* scheduler,
                                        bool enabled);
void bog_scheduler_configure(BogScheduler* scheduler, double bpm,
                                 double swing, double lookahead_ms,
                                 double grid_beats);
void bog_scheduler_start(BogScheduler* scheduler);
void bog_scheduler_stop(BogScheduler* scheduler);
void bog_scheduler_tick(BogScheduler* scheduler);
void bog_scheduler_tick_at(BogScheduler* scheduler,
                               double now_seconds);
double bog_scheduler_now(const BogScheduler* scheduler);
int bog_scheduler_get_current_source_line(const BogScheduler* scheduler);
int bog_scheduler_add_beat_callback(BogScheduler* scheduler,
                                        BogBeatCallback cb,
                                        void* userdata);
void bog_scheduler_remove_beat_callback(BogScheduler* scheduler,
                                            int handle);

/* Transition manager API */
/* Transition manager API */
BogTransitionManager*
bog_transition_manager_create(BogScheduler* scheduler,
                                  double quantization_beats);
void bog_transition_manager_destroy(BogTransitionManager* manager);
void bog_transition_manager_schedule(BogTransitionManager* manager,
                                         const BogProgram* program);
void bog_transition_manager_cancel(BogTransitionManager* manager);
bool bog_transition_manager_has_pending(
    const BogTransitionManager* manager);
void bog_transition_manager_process(BogTransitionManager* manager,
                                        double now_seconds);

#ifdef __cplusplus
}
#endif

#endif /* CPROLOG_SCHEDULER_H */
