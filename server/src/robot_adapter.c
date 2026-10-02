#include "robot_adapter.h"
#include "control_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ROBOT_SIMULATE=1 permite probar la interfaz SIN activar el hardware. */
static int simulation(void) {
    const char *v = getenv("ROBOT_SIMULATE");
    return v && strcmp(v, "1") == 0;
}

#include <math.h>
#include <time.h>
#include "map_estimate.h"
#ifdef ROBOT_WITH_HARDWARE
#include <robot_hw.h>
#endif

static double last_map_time;
static double last_sensor_time;
static double cached_front = NAN;
static int sensor_sampled;
static int ready;

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC,&ts)) return 0;
    return (double)ts.tv_sec + ts.tv_nsec/1000000000.0;
}
static void advance_map(void) {
    double now=now_seconds();
    if (last_map_time>0 && now>last_map_time) map_estimate_advance(now-last_map_time);
    if (now>0) last_map_time=now;
}
int robot_adapter_sensors(double *front, double *left, double *right) {
    if (!front || !left || !right || !ready) return -1;
    *left=NAN; *right=NAN;
    double now=now_seconds();
    if (!sensor_sampled || now<=0 || now-last_sensor_time>=0.1) {
        sensor_sampled=1; last_sensor_time=now;
        if (simulation()) cached_front=48.0;
        else {
#ifdef ROBOT_WITH_HARDWARE
            float value=robot_get_front_distance();
            cached_front=isfinite(value) && value>=2 && value<=400 ? value : NAN;
            (void)robot_led_set(ROBOT_LED_OBSTACLE,isfinite(cached_front) && cached_front<20);
#else
            cached_front=NAN;
#endif
        }
    }
    *front=cached_front;
    return isfinite(*front)?0:-1;
}
int robot_adapter_init(void) {
    int result=control_init();
    if (!result) {
        map_estimate_init();last_map_time=now_seconds();last_sensor_time=0;
        sensor_sampled=0;cached_front=NAN;ready=1;
    }
    return result;
}
void robot_adapter_cleanup(void) {
    advance_map();map_estimate_motion("stop",0);ready=0;control_cleanup();
}
const char *robot_adapter_get_mode(void) { return control_get_mode(); }
int robot_adapter_move(const char *direction, int speed) {
    advance_map();
    int result=control_move(direction,speed);
    /* The accepted motion begins after the controller call completes. */
    advance_map();
    if (!result) map_estimate_motion(direction,speed);
    else {
        (void)control_stop();map_estimate_motion("stop",0);
    }
    return result;
}
int robot_adapter_mode(const char *mode) {
    advance_map();
    int result=control_set_mode(mode);
    advance_map();
    if (!result || !strcmp(control_get_mode(),"unknown")) map_estimate_motion("stop",0);
    return result;
}
int robot_adapter_audio(const char *action, int song_id, int volume) {
    if (!simulation()) return -1;
    fprintf(stderr, "SIMULACION: audio=%s song=%d volume=%d (sin reproductor)\n", action, song_id, volume);
    return 0;
}
int robot_adapter_map(char *json, unsigned long capacity) {
    if (!ready) return -1;
    advance_map();
    double f,l,r;
    if (!robot_adapter_sensors(&f,&l,&r)) {
        advance_map();map_estimate_obstacle(f);
    }
    return map_estimate_json(json,capacity);
}
