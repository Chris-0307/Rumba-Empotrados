#define _POSIX_C_SOURCE 200809L
#include "robot_adapter.h"
#include "control_logic.h"
#include "floor_guard.h"
#include "auto_navigation.h"
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
static int simulated_suction, suction_active;
static double suction_deadline;
static double last_map_time, front_sample_time;
static double cached_front=NAN;
static int obstacle_armed=1;
static unsigned obstacle_clear;
static double last_obstacle_alert;
static FloorGuard floor_guard;
static AutoNavigation navigation;
static AutoAction applied_auto_action=AUTO_STOP;
/* 0 sin ciclo, 1 activo, 2 apagando, 3 finalizado, 4 cancelado. */
static int cycle_state, cycle_duration;
static int motor_left, motor_right, motor_dirty;
static double turn_factor;
static char manual_direction[16]="stop";
static int turn_value(int value) {return (int)lround(value*turn_factor);}
static void map_pair(const char *direction,int left,int right) {
    if (!strcmp(direction,"backward") || !strcmp(direction,"left")) left=-left;
    if (!strcmp(direction,"backward") || !strcmp(direction,"right")) right=-right;
    map_estimate_motor_speeds(left,right);
}
static double cycle_started, cycle_deadline;
static const char *cycle_status(void) {
    static const char *states[]={"idle","running","finishing","completed","cancelled"};
    return states[cycle_state];
}
static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC,&ts)) return 0;
    return (double)ts.tv_sec+ts.tv_nsec/1000000000.0;
}
static void alert_locked(int kind) {
#ifdef ROBOT_WITH_HARDWARE
    if (!simulated) (void)robot_audio_alert((RobotAudioAlert)kind);
#else
    (void)kind;
#endif
}
/* Aviso al entrar en obstaculo; rearmar tras tres muestras >25 cm. */
static void obstacle_alert_locked(double front) {
    if (isfinite(front) && front>25) {
        if (obstacle_clear<3) obstacle_clear++;
        if (obstacle_clear>=3) obstacle_armed=1;
    } else {
        obstacle_clear=0;
        if (isfinite(front) && front<20 && obstacle_armed) {
            double now=now_seconds();
            if (!last_obstacle_alert || now-last_obstacle_alert>=3.0) {
                alert_locked(3);last_obstacle_alert=now;
            }
            obstacle_armed=0;
        }
    }
}
static double floor_limit(void) {
    const char *s=getenv("ROBOT_FLOOR_MAX_CM"); char *end;
    if (!s || !*s) return 0;
    double cm=strtod(s,&end);
    return end!=s && !*end && isfinite(cm) && cm>2 && cm<=100 ? cm:0;
}
static double auto_setting(const char *name,double fallback,double min,double max) {
    const char *s=getenv(name);char *end;
    if (!s || !*s) return fallback;
    double value=strtod(s,&end);
    return end!=s && !*end && isfinite(value) && value>=min && value<=max?value:fallback;
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
    if (!result) {moving=0;strcpy(manual_direction,"stop");}
    return result;
}
/* controller_lock protege el GPIO y el plazo; la web renueva cada 600 ms. */
static int suction_set_locked(int enabled) {
    int result=-1;
    if (simulated) {simulated_suction=enabled;result=0;}
#ifdef ROBOT_WITH_HARDWARE
    else result=robot_suction_set(enabled)==ROBOT_OK?0:-1;
#endif
    if (!result) {suction_active=enabled;suction_deadline=enabled?now_seconds()+5.0:0;}
    return result;
}
static int suction_get_locked(void) {
    if (simulated) return simulated_suction;
#ifdef ROBOT_WITH_HARDWARE
    return robot_suction_get();
#else
    return -1;
#endif
}
static void suction_watchdog_locked(void) {
    if (suction_active && (now_seconds()<=0 || now_seconds()>=suction_deadline))
        (void)suction_set_locked(0); /* Reintentar si el hardware rechaza apagar. */
}
/* Se ejecuta aunque no haya clientes HTTP o el automatico este pausado. */
static void cycle_check_locked(void) {
    double now=now_seconds();
    if (cycle_state==1 && cycle_duration>0 && now>=cycle_deadline) {
        cycle_state=2;
        auto_navigation_pause(&navigation,"cycle_ending");
    }
    if (cycle_state!=2) return;
    int stopped=stop_locked();
    int suction_off=suction_active?suction_set_locked(0):0;
    applied_auto_action=AUTO_STOP;
    if (!stopped && !suction_off) {
        cycle_state=3;
        auto_navigation_pause(&navigation,"cycle_completed");
        alert_locked(4);
    }
}
/* El mismo hilo de sensores ejecuta el patron; ningun sleep de maniobra. */
static void automatic_step_locked(void) {
    if (strcmp(control_get_mode(),"automatic")) return;
    double now=now_seconds();
    double front=now>0 && now>=front_sample_time && now-front_sample_time<=0.30?cached_front:NAN;
    AutoAction action=auto_navigation_step(&navigation,now,front,!floor_guard_blocked(&floor_guard,now));
    if (action==AUTO_STOP) {
        if (moving || applied_auto_action!=AUTO_STOP) {
            if (stop_locked()) auto_navigation_pause(&navigation,"motor_error");
        }
        applied_auto_action=AUTO_STOP;
        return;
    }
    if (action==applied_auto_action && moving && !motor_dirty) return;
    /* Toda nueva orden vuelve a verificar el piso en el punto de ejecucion. */
    if (floor_guard_blocked(&floor_guard,now_seconds())) {
        auto_navigation_pause(&navigation,"floor_safety_blocked");(void)stop_locked();applied_auto_action=AUTO_STOP;return;
    }
    const char *direction=action==AUTO_ADVANCE?"forward":action==AUTO_LEFT?"left":"right";
    int left=action==AUTO_ADVANCE?motor_left:turn_value(motor_left);
    int right=action==AUTO_ADVANCE?motor_right:turn_value(motor_right);
    if (!left && !right) {auto_navigation_pause(&navigation,"zero_speed");(void)stop_locked();applied_auto_action=AUTO_STOP;return;}
    advance_map();int result=control_move_pair(direction,left,right,1);advance_map();
    if (result) { moving=1;auto_navigation_pause(&navigation,"motor_error");(void)stop_locked();applied_auto_action=AUTO_STOP; }
    else { moving=1;motor_dirty=0;applied_auto_action=action;map_pair(direction,left,right); }
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
        if (simulated) floor=3;
#ifdef ROBOT_WITH_HARDWARE
        else floor=robot_get_floor_distance();
#endif
        pthread_mutex_lock(&controller_lock);
        double now=now_seconds();
        suction_watchdog_locked();
        cycle_check_locked();
        if (floor_guard.sampled_at>0 && now-floor_guard.sampled_at>0.30 && moving) {
            floor_guard.latched=1;
            if (!strcmp(control_get_mode(),"automatic")) auto_navigation_pause(&navigation,"floor_safety_blocked");
            (void)stop_locked();
        }
        floor_guard_observe(&floor_guard,floor,now,moving);
        if (moving && floor_guard_blocked(&floor_guard,now)) {
            if (!strcmp(control_get_mode(),"automatic")) auto_navigation_pause(&navigation,"floor_safety_blocked");
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
            floor_guard.latched=1;
            if (!strcmp(control_get_mode(),"automatic")) auto_navigation_pause(&navigation,"floor_safety_blocked");
            (void)stop_locked();
        }
        if (moving && !strcmp(control_get_mode(),"unknown")) (void)stop_locked();
        suction_watchdog_locked();
        cycle_check_locked();
        automatic_step_locked();
        obstacle_alert_locked(cached_front);
        pthread_mutex_unlock(&controller_lock);
        /* En hardware librobot separa los disparos; simulacion evita un bucle ocupado. */
        struct timespec pause={0,simulated?130000000:1000000}; nanosleep(&pause,NULL);
    }
    return NULL;
}
int robot_adapter_init(void) {
    int result=control_init();
    if (result) return result;
    simulated=simulation();simulated_suction=0;suction_active=0;suction_deadline=0;
    map_estimate_init();last_map_time=now_seconds();front_sample_time=0;
    cached_front=NAN;quitting=0;moving=0;
    obstacle_armed=1;obstacle_clear=0;last_obstacle_alert=0;
    floor_guard_init(&floor_guard,floor_limit());
    double timeout=auto_setting("ROBOT_AUTO_TURN_TIMEOUT_MS",6000,500,15000)/1000.0;
    uint32_t seed=(uint32_t)time(NULL)^(uint32_t)(now_seconds()*1000000.0);
    auto_navigation_init(&navigation,seed,(int)auto_setting("ROBOT_AUTO_SPEED",30,20,60),
        (int)auto_setting("ROBOT_AUTO_TURN_SPEED",25,20,60),timeout);
    motor_left=motor_right=navigation.speed;motor_dirty=0;
    turn_factor=fmin(1.0,(double)navigation.turn_speed/navigation.speed);
    strcpy(manual_direction,"stop");
    cycle_state=0;cycle_duration=0;cycle_started=cycle_deadline=0;
    applied_auto_action=AUTO_STOP;ready=1;
    if (floor_guard.max_cm<=0) fprintf(stderr,"Configurar ROBOT_FLOOR_MAX_CM: movimiento bloqueado hasta calibrar\n");
    if (pthread_create(&sensor_thread,NULL,sample_sensors,NULL)) {
        ready=0; control_cleanup(); return -1;
    }
    worker_started=1;
    alert_locked(0);
    return 0;
}
void robot_adapter_cleanup(void) {
    pthread_mutex_lock(&controller_lock);
    if (!ready) { pthread_mutex_unlock(&controller_lock); return; }
    quitting=1; (void)suction_set_locked(0); (void)stop_locked();
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
static int move_requested(const char *direction,int speed,int configured) {
    if (!direction || speed<0 || speed>100 || (strcmp(direction,"forward") && strcmp(direction,"backward") &&
        strcmp(direction,"left") && strcmp(direction,"right") && strcmp(direction,"stop"))) return -2;
    pthread_mutex_lock(&controller_lock);
    int result;
    if (!ready) result=-1;
    else if (!strcmp(direction,"stop") || (configured?(!motor_left && !motor_right):speed==0)) {
        if (!strcmp(control_get_mode(),"automatic") && cycle_state!=2 && cycle_state!=3) {
            auto_navigation_pause(&navigation,"user_stop");
            if (cycle_state==1) cycle_state=4;
        }
        applied_auto_action=AUTO_STOP;
        result=stop_locked();
        if (!result) floor_guard_acknowledge(&floor_guard,now_seconds());
    } else if (floor_guard_blocked(&floor_guard,now_seconds())) {
        (void)stop_locked(); result=-4;
    } else {
        advance_map();result=configured?control_move_pair(direction,motor_left,motor_right,0):control_move(direction,speed);advance_map();
        if (!result) {
            moving=1;strcpy(manual_direction,direction);
            if (configured) map_pair(direction,motor_left,motor_right);
            else map_estimate_motion(direction,speed);
        }
        else (void)stop_locked();
    }
    pthread_mutex_unlock(&controller_lock); return result;
}
int robot_adapter_move(const char *direction,int speed) {return move_requested(direction,speed,0);}
int robot_adapter_move_configured(const char *direction) {return move_requested(direction,1,1);}
int robot_adapter_motors_set(int left,int right) {
    if (left<0 || left>100 || right<0 || right>100) return -2;
    pthread_mutex_lock(&controller_lock);
    if (!ready) {pthread_mutex_unlock(&controller_lock);return -1;}
    cycle_check_locked();motor_left=left;motor_right=right;motor_dirty=1;
    int result=0;
    if (moving && !strcmp(control_get_mode(),"manual")) {
        if (floor_guard_blocked(&floor_guard,now_seconds()) || (!left && !right)) result=stop_locked();
        else {
            advance_map();result=control_move_pair(manual_direction,left,right,0);advance_map();
            if (!result) map_pair(manual_direction,left,right);
            else (void)stop_locked();
        }
    }
    pthread_mutex_unlock(&controller_lock);return result;
}
int robot_adapter_motors_json(char *json,unsigned long capacity) {
    if (!json || !capacity) return -1;
    pthread_mutex_lock(&controller_lock);
    if (!ready) {pthread_mutex_unlock(&controller_lock);return -1;}
    int n=snprintf(json,capacity,"{\"left_speed\":%d,\"right_speed\":%d,\"turn_left_speed\":%d,\"turn_right_speed\":%d,\"turn_factor\":%.4f}",motor_left,motor_right,turn_value(motor_left),turn_value(motor_right),turn_factor);
    pthread_mutex_unlock(&controller_lock);return n>=0 && (unsigned long)n<capacity?0:-1;
}
int robot_adapter_mode(const char *mode) { return robot_adapter_mode_timed(mode,0); }
int robot_adapter_mode_timed(const char *mode,int duration_seconds) {
    if (duration_seconds<0 || duration_seconds>86400) return -2;
    if (!mode || (strcmp(mode,"manual") && strcmp(mode,"automatic"))) return -2;
    pthread_mutex_lock(&controller_lock);
    int changed=strcmp(control_get_mode(),mode)!=0;
    advance_map(); int result=ready?control_set_mode(mode):-1; advance_map();
    if (!result) {
        moving=0;applied_auto_action=AUTO_STOP;map_estimate_motion("stop",0);
        if (!strcmp(mode,"automatic")) {
            auto_navigation_start(&navigation);
            cycle_state=1;cycle_duration=duration_seconds;cycle_started=now_seconds();
            cycle_deadline=duration_seconds?cycle_started+duration_seconds:0;
        } else {
            auto_navigation_cancel(&navigation);
            if (cycle_state==1 || cycle_state==2) cycle_state=4;
        }
        if (changed) alert_locked(!strcmp(mode,"automatic")?2:1);
    }
    else (void)stop_locked();
    pthread_mutex_unlock(&controller_lock); return result;
}
int robot_adapter_mode_json(char *json,unsigned long capacity) {
    if (!json || !capacity) return -1;
    pthread_mutex_lock(&controller_lock);
    double remaining=cycle_state==1 && cycle_duration>0?fmax(0,cycle_deadline-now_seconds()):0;
    char remaining_json[32]="null";
    if (cycle_duration>0) snprintf(remaining_json,sizeof remaining_json,"%.0f",ceil(remaining));
    int n=snprintf(json,capacity,"{\"mode\":\"%s\",\"navigation\":{\"pattern\":\"random_bounce\",\"state\":\"%s\",\"reason\":\"%s\",\"speed\":%d,\"turns\":%u,\"strategy\":\"turn_until_clear\",\"turn_speed\":%d,\"turn_timeout_ms\":%.0f,\"clear_samples\":%u},\"cycle\":{\"status\":\"%s\",\"duration_seconds\":%d,\"remaining_seconds\":%s}}",
        control_get_mode(),auto_navigation_state(&navigation),navigation.reason,navigation.speed,navigation.turns,navigation.turn_speed,navigation.turn_timeout*1000,navigation.clear_samples,cycle_status(),cycle_duration,remaining_json);
    pthread_mutex_unlock(&controller_lock);return n>=0 && (unsigned long)n<capacity?0:-1;
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

int robot_adapter_suction_set(int enabled) {
    if (enabled!=0 && enabled!=1) return -2;
    pthread_mutex_lock(&controller_lock);
    cycle_check_locked();
    int result=!ready?-1:enabled && !strcmp(control_get_mode(),"automatic") && (cycle_state==2 || cycle_state==3)?-5:suction_set_locked(enabled);
    pthread_mutex_unlock(&controller_lock);return result;
}
int robot_adapter_suction_json(char *json,unsigned long capacity) {
    if (!json || !capacity) return -1;
    pthread_mutex_lock(&controller_lock);
    int state=ready?suction_get_locked():-1;
    int n=snprintf(json,capacity,"{\"available\":%s,\"enabled\":%s,\"state\":\"%s\",\"timeout_seconds\":5}",
        state>=0?"true":"false",state<0?"null":state?"true":"false",state<0?"unavailable":state?"on":"off");
    pthread_mutex_unlock(&controller_lock);return n>=0 && (unsigned long)n<capacity?0:-1;
}
