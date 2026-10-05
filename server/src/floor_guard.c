#include "floor_guard.h"
#include <math.h>
enum { FLOOR_UNKNOWN, FLOOR_PRESENT, FLOOR_CLIFF, FLOOR_ERROR, FLOOR_WARMUP };
#define MAX_SAMPLE_AGE 0.30
void floor_guard_init(FloorGuard *g, double max_cm) {
    *g=(FloorGuard){.max_cm=max_cm,.cm=NAN};
}
void floor_guard_observe(FloorGuard *g, double cm, double now, int moving) {
    g->sampled_at=now; g->cm=isfinite(cm) && cm>=2 && cm<=400 ? cm:NAN;
    if (!isfinite(g->cm) || now<=0) { g->state=FLOOR_ERROR; g->good_samples=0; }
    else if (g->max_cm<=0) { g->state=FLOOR_UNKNOWN; g->good_samples=0; }
    else if (cm>g->max_cm) { g->state=FLOOR_CLIFF; g->good_samples=0; }
    else {
        if (g->good_samples<3) g->good_samples++;
        g->state=g->good_samples>=3 ? FLOOR_PRESENT:FLOOR_WARMUP;
    }
    if (moving && floor_guard_blocked(g,now)) g->latched=1;
}
static int stale(const FloorGuard *g, double now) {
    return now<=0 || g->sampled_at<=0 || now<g->sampled_at || now-g->sampled_at>MAX_SAMPLE_AGE;
}
int floor_guard_blocked(const FloorGuard *g, double now) {
    return g->max_cm<=0 || g->state!=FLOOR_PRESENT || g->latched || stale(g,now);
}
const char *floor_guard_state(const FloorGuard *g, double now) {
    if (g->max_cm<=0) return "unconfigured";
    if (g->state==FLOOR_UNKNOWN) return "unknown";
    if (stale(g,now)) return "stale";
    if (g->state==FLOOR_ERROR) return "sensor_error";
    if (g->state==FLOOR_CLIFF) return "cliff";
    if (g->state==FLOOR_WARMUP) return "warming_up";
    return "present";
}
void floor_guard_acknowledge(FloorGuard *g, double now) {
    if (g->state==FLOOR_PRESENT && !stale(g,now) && g->max_cm>0) g->latched=0;
}
