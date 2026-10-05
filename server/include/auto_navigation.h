#ifndef AUTO_NAVIGATION_H
#define AUTO_NAVIGATION_H
#include <stdint.h>
typedef enum { AUTO_IDLE, AUTO_WAITING, AUTO_FORWARD, AUTO_BRAKING, AUTO_TURNING, AUTO_PAUSED } AutoPhase;
typedef enum { AUTO_STOP, AUTO_ADVANCE, AUTO_LEFT, AUTO_RIGHT } AutoAction;
typedef struct {
    AutoPhase phase;
    const char *reason;
    double deadline;
    unsigned int turns, clear_samples;
    uint32_t random_state;
    AutoAction turn_action;
    int speed, turn_speed;
    double turn_timeout;
} AutoNavigation;
void auto_navigation_init(AutoNavigation *nav, uint32_t seed, int speed, int turn_speed, double turn_timeout);
void auto_navigation_start(AutoNavigation *nav);
void auto_navigation_cancel(AutoNavigation *nav);
void auto_navigation_pause(AutoNavigation *nav, const char *reason);
/* Una llamada por nueva muestra frontal, no por refresco HTTP. */
AutoAction auto_navigation_step(AutoNavigation *nav, double now, double front_cm, int floor_safe);
const char *auto_navigation_state(const AutoNavigation *nav);
#endif
