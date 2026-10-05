#ifndef FLOOR_GUARD_H
#define FLOOR_GUARD_H
/* Politica independiente del hardware. Todas las llamadas bajo mutex del adaptador. */
typedef struct {
    double max_cm, cm, sampled_at;
    unsigned int good_samples;
    int state, latched;
} FloorGuard;
void floor_guard_init(FloorGuard *guard, double max_cm);
void floor_guard_observe(FloorGuard *guard, double cm, double now, int moving);
int floor_guard_blocked(const FloorGuard *guard, double now);
const char *floor_guard_state(const FloorGuard *guard, double now);
void floor_guard_acknowledge(FloorGuard *guard, double now);
#endif
