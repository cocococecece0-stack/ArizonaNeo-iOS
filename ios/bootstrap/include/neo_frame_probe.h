#ifndef NEO_FRAME_PROBE_H
#define NEO_FRAME_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the one-shot, bounded main-queue discovery of
   Arizona.GLView and installs the tick swizzle. Safe to call more than once.
   This never starts Lua, NeoLoaderHost, or the game renderer. */
void neo_ios_frame_probe_start(void);

/* Schedules neo_ios_frame_probe_start() on the main queue. Intended for a
   dyld constructor so probe static initialization cannot race the constructor. */
void neo_ios_frame_probe_schedule_start(void);

/* Diagnostic state for hosts/tests. */
int neo_ios_frame_probe_is_installed(void);
uint64_t neo_ios_frame_probe_tick_count(void);

#ifdef __cplusplus
}
#endif

#endif /* NEO_FRAME_PROBE_H */
