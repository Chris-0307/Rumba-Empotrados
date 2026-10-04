#define _POSIX_C_SOURCE 200809L
#include "robot_adapter.h"
#include "robot_hw.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
/* Doble para verificar el trabajador; no reemplaza la biblioteca del robot. */
static atomic_int floor_value=7, front_value=48, stop_count, move_count, audio_count;
static void pause_ms(long ms) { struct timespec ts={ms/1000,(ms%1000)*1000000}; nanosleep(&ts,NULL); }
int robot_init(void) { return 0; }
void robot_cleanup(void) {}
int robot_stop(void) { atomic_fetch_add(&stop_count,1); return 0; }
int robot_led_set(RobotLed led,int state) { (void)led;(void)state; return 0; }
int robot_move_forward(int speed) { (void)speed; atomic_fetch_add(&move_count,1); return 0; }
int robot_move_backward(int s) { return robot_move_forward(s); }
int robot_turn_left(int s) { return robot_move_forward(s); }
int robot_turn_right(int s) { return robot_move_forward(s); }
float robot_get_floor_distance(void) { pause_ms(65); return atomic_load(&floor_value); }
float robot_get_front_distance(void) { pause_ms(65); return atomic_load(&front_value); }
int robot_audio_set_volume(int v) { (void)v; atomic_fetch_add(&audio_count,1); return 0; }
int robot_audio_play(const char *p) { (void)p;return 0; }
int robot_audio_pause(void) { return 0; }
int robot_audio_stop(void) { return 0; }
const char *robot_audio_get_state(void) { return "playing"; }
int main(void) {
    char json[1024]; unsetenv("ROBOT_SIMULATE"); unsetenv("ROBOT_FLOOR_MAX_CM");
    assert(robot_adapter_init()==0); pause_ms(450);
    assert(robot_adapter_move("forward",30)==-4);
    assert(robot_adapter_sensor_json(json,sizeof json)==0 && strstr(json,"unconfigured"));
    robot_adapter_cleanup(); robot_adapter_cleanup();
    setenv("ROBOT_FLOOR_MAX_CM","10",1);
    assert(robot_adapter_init()==0);
    assert(robot_adapter_move("forward",30)==-4);
    pause_ms(450); assert(robot_adapter_move("forward",30)==0);
    int before=atomic_load(&stop_count); atomic_store(&floor_value,14);
    /* Ninguna consulta HTTP o sensor: debe detenerse por si solo. */
    pause_ms(300); assert(atomic_load(&stop_count)>before);
    assert(robot_adapter_sensor_json(json,sizeof json)==0 && strstr(json,"cliff"));
    assert(robot_adapter_move("backward",30)==-4);
    atomic_store(&floor_value,7); pause_ms(450);
    assert(robot_adapter_move("forward",30)==-4); /* Requiere stop explicito. */
    assert(robot_adapter_move("stop",0)==0); assert(robot_adapter_move("forward",30)==0);
    atomic_store(&front_value,5); pause_ms(200);
    assert(robot_adapter_move("forward",30)==0); /* Frontal no frena en manual. */
    atomic_store(&floor_value,-1); before=atomic_load(&stop_count); pause_ms(300);
    assert(atomic_load(&stop_count)>before); assert(robot_adapter_move("left",30)==-4);
    assert(robot_adapter_sensor_json(json,sizeof json)==0 && strstr(json,"sensor_error") && strstr(json,"\"floor_cm\":null"));
    assert(robot_adapter_audio("volume",0,70)==0 && atomic_load(&audio_count)==1);
    assert(!strcmp(robot_adapter_audio_state(),"playing"));
    assert(robot_adapter_map(json,sizeof json)==-1); /* Buffer insuficiente no corrompe. */
    robot_adapter_cleanup();
    puts("Adaptador: parada independiente, bloqueos, recuperacion y audio OK");
}
