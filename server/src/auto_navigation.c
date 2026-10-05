#include "auto_navigation.h"
#include <math.h>
#define OBSTACLE_CM 20.0
#define CLEAR_CM 25.0
#define CLEAR_SAMPLES 3U
static uint32_t random_next(AutoNavigation *n) {
    uint32_t x=n->random_state;x^=x<<13;x^=x>>17;x^=x<<5;
    return n->random_state=x;
}
void auto_navigation_init(AutoNavigation *n,uint32_t seed,int speed,int turn_speed,double timeout) {
    *n=(AutoNavigation){.phase=AUTO_IDLE,.reason="manual",.random_state=seed?seed:0x12345U,.speed=speed,.turn_speed=turn_speed,.turn_timeout=timeout};
}
void auto_navigation_start(AutoNavigation *n) {n->phase=AUTO_WAITING;n->reason="waiting_sensors";n->turns=0;n->clear_samples=0;n->deadline=0;}
void auto_navigation_cancel(AutoNavigation *n) {n->phase=AUTO_IDLE;n->reason="manual";n->turns=0;n->clear_samples=0;}
void auto_navigation_pause(AutoNavigation *n,const char *reason) {n->phase=AUTO_PAUSED;n->reason=reason;n->clear_samples=0;}
static AutoAction brake(AutoNavigation *n,double now) {
    n->turn_action=(random_next(n)&1U)?AUTO_LEFT:AUTO_RIGHT;
    n->phase=AUTO_BRAKING;n->reason="obstacle";n->deadline=now+0.15;n->clear_samples=0;
    return AUTO_STOP;
}
static int clear_front(AutoNavigation *n,double front) {
    if (front>CLEAR_CM) {if(n->clear_samples<CLEAR_SAMPLES)n->clear_samples++;}
    else n->clear_samples=0;
    return n->clear_samples>=CLEAR_SAMPLES;
}
AutoAction auto_navigation_step(AutoNavigation *n,double now,double front,int floor_safe) {
    if (n->phase==AUTO_IDLE || n->phase==AUTO_PAUSED) return AUTO_STOP;
    if (!isfinite(now) || now<=0) {auto_navigation_pause(n,"clock_error");return AUTO_STOP;}
    if (!floor_safe) {
        if (n->phase!=AUTO_WAITING) auto_navigation_pause(n,"floor_safety_blocked");
        else {n->reason="waiting_floor";n->clear_samples=0;}
        return AUTO_STOP;
    }
    /* El limite incluye las pausas por eco invalido: no se reinicia el giro. */
    if (n->phase==AUTO_TURNING && now>=n->deadline) {
        auto_navigation_pause(n,"turn_timeout");return AUTO_STOP;
    }
    if (!isfinite(front) || front<2 || front>400) {
        if (n->phase==AUTO_FORWARD) n->phase=AUTO_WAITING;
        n->reason="waiting_front";n->clear_samples=0;
        return AUTO_STOP;
    }
    if (n->phase==AUTO_BRAKING) {
        if (now<n->deadline) return AUTO_STOP;
        n->phase=AUTO_TURNING;n->reason="searching_clearance";n->turns++;
        n->deadline=now+n->turn_timeout;n->clear_samples=0;
        return n->turn_action;
    }
    if (n->phase==AUTO_TURNING) {
        if (clear_front(n,front)) {
            n->phase=AUTO_WAITING;n->reason="checking_front";n->clear_samples=0;
            return AUTO_STOP; /* Reconfirmar despues de detener el giro. */
        }
        n->reason="searching_clearance";
        return n->turn_action;
    }
    if (front<OBSTACLE_CM) return brake(n,now);
    if (n->phase==AUTO_WAITING && !clear_front(n,front)) {
        n->reason="confirming_clearance";return AUTO_STOP;
    }
    n->phase=AUTO_FORWARD;n->reason="clear_front";n->turns=0;n->clear_samples=0;
    return AUTO_ADVANCE;
}
const char *auto_navigation_state(const AutoNavigation *n) {
    switch(n->phase) {
        case AUTO_IDLE:return "idle";case AUTO_WAITING:return "waiting";case AUTO_FORWARD:return "forward";
        case AUTO_BRAKING:return "braking";case AUTO_TURNING:return n->turn_action==AUTO_LEFT?"turn_left":"turn_right";
        case AUTO_PAUSED:return "paused";
    }
    return "unknown";
}
