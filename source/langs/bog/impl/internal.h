#ifndef CPROLOG_INTERNAL_H
#define CPROLOG_INTERNAL_H

#include <stdint.h>

#include "bog.h"

/* Internal term constructor helpers shared across translation units */
BogTerm* bog_make_num(BogArena* arena, double value);
BogTerm* bog_make_atom(BogArena* arena, const char* name);
BogTerm* bog_make_var(BogArena* arena, const char* name);
BogTerm* bog_make_compound(BogArena* arena, const char* functor,
                                   BogTerm** args, size_t arity);
BogTerm* bog_make_list(BogArena* arena, BogTerm** items,
                               size_t length, BogTerm* tail);
BogTerm* bog_make_expr(BogArena* arena, char op, BogTerm* left,
                               BogTerm* right);

/* Per-instance random stream (splitmix64) */
uint64_t bog_rng_seed_from_clock(const void* salt);
double bog_rng_unit(uint64_t* state); /* uniform in [0, 1) */

#endif /* CPROLOG_INTERNAL_H */
