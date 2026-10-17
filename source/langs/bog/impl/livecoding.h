#ifndef CPROLOG_LIVECODING_H
#define CPROLOG_LIVECODING_H

#include "bog.h"
#include "scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BogLiveEvaluator BogLiveEvaluator;

/* Validation helper */
bool bog_validate_code(const char* text, BogProgram** program_out,
                           BogArena** arena_out, char** error_message);

/* Live evaluator */
BogLiveEvaluator*
bog_live_evaluator_create(BogScheduler* scheduler,
                              double debounce_seconds);
void bog_live_evaluator_destroy(BogLiveEvaluator* evaluator);
bool bog_live_evaluator_evaluate(BogLiveEvaluator* evaluator,
                                     const char* text, char** error_message);
const char*
bog_live_evaluator_last_code(const BogLiveEvaluator* evaluator);
const BogProgram*
bog_live_evaluator_program(const BogLiveEvaluator* evaluator);
void bog_live_evaluator_set_scheduler(BogLiveEvaluator* evaluator,
                                          BogScheduler* scheduler);
/* With a transition manager, evaluate swaps on its next bar line instead of
 * at once. The transition must drive the evaluator's scheduler. */
void bog_live_evaluator_set_transition(BogLiveEvaluator* evaluator,
                                       BogTransitionManager* transition);

typedef void (*BogLiveValidatedCallback)(bool success,
                                             const BogProgram* program,
                                             const char* text,
                                             const char* error_message,
                                             void* userdata);

int bog_live_evaluator_on_validated(BogLiveEvaluator* evaluator,
                                        BogLiveValidatedCallback callback,
                                        void* userdata);
void bog_live_evaluator_remove_callback(BogLiveEvaluator* evaluator,
                                            int handle);

#ifdef __cplusplus
}
#endif

#endif /* CPROLOG_LIVECODING_H */
