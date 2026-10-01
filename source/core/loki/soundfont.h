/* soundfont.h - Default soundfont discovery
 *
 * Resolution order when no -sf is given:
 * 1. PSND_SOUNDFONT environment variable ("none" disables all of these)
 * 2. [audio] soundfont in .psnd/config.toml or ~/.psnd/config.toml
 * 3. A soundfont found by psnd_soundfont_search() in the default
 *    directories, used only when no MIDI output can reach a synth
 */

#ifndef LOKI_SOUNDFONT_H
#define LOKI_SOUNDFONT_H

#include <stddef.h>

/* Search dirs in order: every preferred General MIDI filename in every
 * dir first, then the alphabetically first *.sf2 of the first dir that
 * has one. Missing dirs are skipped. Returns 0 and writes the path to
 * out, or -1 if nothing matched or out is too small. */
int psnd_soundfont_search(const char *const *dirs, size_t ndirs,
                          char *out, size_t out_size);

/* Returns explicit_sf if set. Returns NULL if other_backend is set, i.e.
 * the user chose MIDI (-p, --virtual), Csound, a plugin, or is listing
 * ports. Otherwise returns the configured or discovered soundfont, or NULL
 * to keep the MIDI default. The result stays valid until the next call. */
const char *psnd_soundfont_resolve(const char *explicit_sf, int other_backend);

#endif /* LOKI_SOUNDFONT_H */
