#include "map_estimate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static double heading(void) {char json[12000];assert(!map_estimate_json(json,sizeof json));char *p=strstr(json,"\"heading_deg\":");assert(p);return atof(p+14);}
int main(void) {
 map_estimate_init();map_estimate_motor_speeds(30,30);map_estimate_advance(1);assert(heading()==-90);
 map_estimate_init();map_estimate_motor_speeds(20,60);map_estimate_advance(1);assert(heading()<-90);
 map_estimate_init();map_estimate_motor_speeds(60,20);map_estimate_advance(1);assert(heading()>-90);
 map_estimate_init();map_estimate_motor_speeds(-30,30);map_estimate_advance(1);assert(heading()==-117);
 map_estimate_motor_speeds(0,0);map_estimate_advance(1);assert(heading()==-117);
 puts("OK: odometria estimada recta, curvas, giro y parada.");
}
