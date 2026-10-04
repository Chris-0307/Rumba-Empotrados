#define _POSIX_C_SOURCE 200809L
#include "robot_adapter.h"
#include "control_logic.h"
#include "floor_guard.h"
#include "map_estimate.h"
#include "audio_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#ifdef ROBOT_WITH_HARDWARE
#include <robot_hw.h>
#endif

static int simulation(void) {
    const char *v=getenv("ROBOT_SIMULATE"); return v && !strcmp(v,"1");
}
static pthread_mutex_t controller_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t sensor_thread;
static int ready, worker_started, quitting, moving;
static int simulated;
static double last_map_time, front_sample_time;
static double cached_front=NAN;
static FloorGuard floor_guard;
static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC,&ts)) return 0;
    return (double)ts.tv_sec+ts.tv_nsec/1000000000.0;
}
static double floor_limit(void) {
    const char *s=getenv("ROBOT_FLOOR_MAX_CM"); char *end;
    if (!s || !*s) return 0;
    double cm=strtod(s,&end);
    return end!=s && !*end && isfinite(cm) && cm>2 && cm<=100 ? cm:0;
}
/* Solo bajo controller_lock; hardware motor, LEDs, mapa y modo se serializan. */
static void advance_map(void) {
    double now=now_seconds();
    if (last_map_time>0 && now>last_map_time) map_estimate_advance(now-last_map_time);
    if (now>0) last_map_time=now;
}
static int stop_locked(void) {
    advance_map();
    int result=control_stop();
    advance_map(); map_estimate_motion("stop",0);
    /* Si el stop falla, el trabajador debe reintentarlo. */
    if (!result) moving=0;
    return result;
}
static void *sample_sensors(void *unused) {
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&controller_lock);
        int done=quitting;
        pthread_mutex_unlock(&controller_lock);
        if (done) break;
        double floor=NAN, front=NAN;
        /* Suelo primero; publicar y detener ANTES de esperar al sensor frontal. */
        if (simulated) floor=7;
#ifdef ROBOT_WITH_HARDWARE
        else floor=robot_get_floor_distance();
#endif
        pthread_mutex_lock(&controller_lock);
        double now=now_seconds();
        if (floor_guard.sampled_at>0 && now-floor_guard.sampled_at>0.30 && moving) {
            floor_guard.latched=1; (void)stop_locked();
        }
        floor_guard_observe(&floor_guard,floor,now,moving);
        if (moving && floor_guard_blocked(&floor_guard,now)) {
            fprintf(stderr,"Proteccion suelo: %s (%.2f cm); parada\n",floor_guard_state(&floor_guard,now),floor);
            (void)stop_locked();
        }
        pthread_mutex_unlock(&controller_lock);
        if (simulated) front=48;
#ifdef ROBOT_WITH_HARDWARE
        else front=robot_get_front_distance();
#endif
        pthread_mutex_lock(&controller_lock);
        cached_front=isfinite(front) && front>=2 && front<=400 ? front:NAN;
        front_sample_time=now_seconds();
#ifdef ROBOT_WITH_HARDWARE
        if (!simulated) (void)robot_led_set(ROBOT_LED_OBSTACLE,isfinite(cached_front) && cached_front<20);
#endif
        if (moving && floor_guard_blocked(&floor_guard,now_seconds())) {
            floor_guard.latched=1; (void)stop_locked();
        }
        pthread_mutex_unlock(&controller_lock);
        /* En hardware librobot separa los disparos; simulacion evita un bucle ocupado. */
        struct timespec pause={0,simulated?130000000:1000000}; nanosleep(&pause,NULL);
    }
    return NULL;
}
int robot_adapter_init(void) {
    int result=control_init();
    if (result) return result;
    simulated=simulation();
    map_estimate_init();last_map_time=now_seconds();front_sample_time=0;
    cached_front=NAN;quitting=0;moving=0;
    floor_guard_init(&floor_guard,floor_limit()); ready=1;
    if (floor_guard.max_cm<=0) fprintf(stderr,"Configurar ROBOT_FLOOR_MAX_CM: movimiento bloqueado hasta calibrar\n");
    if (pthread_create(&sensor_thread,NULL,sample_sensors,NULL)) {
        ready=0; control_cleanup(); return -1;
    }
    worker_started=1; return 0;
}
void robot_adapter_cleanup(void) {
    pthread_mutex_lock(&controller_lock);
    if (!ready) { pthread_mutex_unlock(&controller_lock); return; }
    quitting=1; (void)stop_locked();
    pthread_mutex_unlock(&controller_lock);
    if (worker_started) pthread_join(sensor_thread,NULL);
    pthread_mutex_lock(&controller_lock);
    worker_started=0; ready=0; control_cleanup();
    pthread_mutex_unlock(&controller_lock);
}
const char *robot_adapter_get_mode(void) {
    pthread_mutex_lock(&controller_lock);
    const char *mode=control_get_mode();
    pthread_mutex_unlock(&controller_lock); return mode; /* Literales estaticos. */
}
int robot_adapter_sensors(double *front, double *left, double *right) {
    if (!front || !left || !right) return -1;
    pthread_mutex_lock(&controller_lock);
    double now=now_seconds();
    *left=NAN; *right=NAN;
    *front=ready && now>0 && now>=front_sample_time && now-front_sample_time<=0.30 ? cached_front:NAN;
    pthread_mutex_unlock(&controller_lock); return isfinite(*front)?0:-1;
}
int robot_adapter_sensor_json(char *json, unsigned long capacity) {
    if (!json || !capacity) return -1;
    pthread_mutex_lock(&controller_lock);
    if (!ready) { pthread_mutex_unlock(&controller_lock); return -1; }
    double now=now_seconds(); char f[40]="null", down[40]="null", limit[40]="null";
    if (isfinite(cached_front) && now>0 && now>=front_sample_time && now-front_sample_time<=0.30)
        snprintf(f,sizeof f,"%.2f",cached_front);
    const char *state=floor_guard_state(&floor_guard,now);
    if (isfinite(floor_guard.cm) && strcmp(state,"stale") && floor_guard.sampled_at>0)
        snprintf(down,sizeof down,"%.2f",floor_guard.cm);
    if (floor_guard.max_cm>0) snprintf(limit,sizeof limit,"%.2f",floor_guard.max_cm);
    int n=snprintf(json,capacity,"{\"front_cm\":%s,\"left_cm\":null,\"right_cm\":null,\"floor_cm\":%s,\"floor_state\":\"%s\",\"floor_max_cm\":%s,\"movement_blocked\":%s,\"floor_latched\":%s}",
        f,down,state,limit,floor_guard_blocked(&floor_guard,now)?"true":"false",floor_guard.latched?"true":"false");
    pthread_mutex_unlock(&controller_lock); return n>=0 && (unsigned long)n<capacity?0:-1;
}
int robot_adapter_move(const char *direction, int speed) {
    if (!direction || speed<0 || speed>100 || (strcmp(direction,"forward") && strcmp(direction,"backward") &&
        strcmp(direction,"left") && strcmp(direction,"right") && strcmp(direction,"stop"))) return -2;
    pthread_mutex_lock(&controller_lock);
    int result;
    if (!ready) result=-1;
    else if (!strcmp(direction,"stop") || speed==0) {
        result=stop_locked();
        if (!result) floor_guard_acknowledge(&floor_guard,now_seconds());
    } else if (floor_guard_blocked(&floor_guard,now_seconds())) {
        (void)stop_locked(); result=-4;
    } else {
        advance_map(); result=control_move(direction,speed); advance_map();
        if (!result) { moving=1; map_estimate_motion(direction,speed); }
        else (void)stop_locked();
    }
    pthread_mutex_unlock(&controller_lock); return result;
}
int robot_adapter_mode(const char *mode) {
    if (!mode || (strcmp(mode,"manual") && strcmp(mode,"automatic"))) return -2;
    pthread_mutex_lock(&controller_lock);
    advance_map(); int result=ready?control_set_mode(mode):-1; advance_map();
    if (!result) { moving=0; map_estimate_motion("stop",0); }
    else (void)stop_locked();
    pthread_mutex_unlock(&controller_lock); return result;
}
int robot_adapter_audio(const char *action, int song_id, int volume) {
    if (simulation()) return 0;
#ifdef ROBOT_WITH_HARDWARE
    if (!strcmp(action,"volume")) return robot_audio_set_volume(volume)==ROBOT_OK?0:-1;
    if (!strcmp(action,"pause")) return robot_audio_pause()==ROBOT_OK?0:-1;
    if (!strcmp(action,"stop")) return robot_audio_stop()==ROBOT_OK?0:-1;
    if (!strcmp(action,"play")) {
        char path[1024];
        if (audio_catalog_path(song_id,path,sizeof path)) return -1;
        if (robot_audio_set_volume(volume)!=ROBOT_OK) return -1;
        return robot_audio_play(path)==ROBOT_OK?0:-1;
    }
#else
    (void)action;(void)song_id;(void)volume;
#endif
    return -1;
}
const char *robot_adapter_audio_state(void) {
    if (simulation()) return NULL;
#ifdef ROBOT_WITH_HARDWARE
    return robot_audio_get_state();
#else
    return "stopped";
#endif
}
int robot_adapter_map(char *json, unsigned long capacity) {
    pthread_mutex_lock(&controller_lock);
    if (!ready) { pthread_mutex_unlock(&controller_lock); return -1; }
    advance_map();
    double now=now_seconds();
    if (isfinite(cached_front) && now>0 && now>=front_sample_time && now-front_sample_time<=0.30)
        map_estimate_obstacle(cached_front);
    int result=map_estimate_json(json,capacity);
    pthread_mutex_unlock(&controller_lock); return result;
}
