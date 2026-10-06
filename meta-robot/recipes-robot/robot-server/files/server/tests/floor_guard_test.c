#include "floor_guard.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    FloorGuard g;
    floor_guard_init(&g,0); floor_guard_observe(&g,7,1,0);
    assert(floor_guard_blocked(&g,1)); assert(!strcmp(floor_guard_state(&g,1),"unconfigured"));
    floor_guard_init(&g,10); assert(floor_guard_blocked(&g,1));
    floor_guard_observe(&g,7,1,0); floor_guard_observe(&g,7,1.1,0);
    assert(floor_guard_blocked(&g,1.1));
    floor_guard_observe(&g,7,1.2,0); assert(!floor_guard_blocked(&g,1.2));
    assert(floor_guard_blocked(&g,1.51));
    floor_guard_observe(&g,14,1.6,1); assert(g.latched);
    assert(!strcmp(floor_guard_state(&g,1.6),"cliff"));
    floor_guard_acknowledge(&g,1.6); assert(g.latched);
    for (int i=0;i<3;i++) floor_guard_observe(&g,7,1.7+i*.1,0);
    assert(floor_guard_blocked(&g,1.9)); floor_guard_acknowledge(&g,1.9);
    assert(!floor_guard_blocked(&g,1.9));
    floor_guard_observe(&g,NAN,2,1); assert(g.latched && floor_guard_blocked(&g,2));
    assert(!strcmp(floor_guard_state(&g,2),"sensor_error"));
    floor_guard_observe(&g,-1,2.1,0); assert(isnan(g.cm));
    floor_guard_observe(&g,0,2.2,0); assert(isnan(g.cm));
    floor_guard_observe(&g,INFINITY,2.3,0); assert(isnan(g.cm));
    floor_guard_observe(&g,7,0,0); assert(floor_guard_blocked(&g,0));
    puts("Suelo: umbral, errores, antiguedad, confirmacion y enclavamiento OK");
}
