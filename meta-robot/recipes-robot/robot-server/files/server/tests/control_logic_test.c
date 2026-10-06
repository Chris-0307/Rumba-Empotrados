#define _POSIX_C_SOURCE 200809L
#include "control_logic.h"
#include <robot_hw.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Doble de hardware: no usar como sustituto de librobot en produccion. */
static int stops, moves, cleaned, leds[4], fail_led, fail_stop, fail_move;
int robot_init(void) { return ROBOT_OK; }
void robot_cleanup(void) { cleaned++; }
int robot_stop(void) { stops++; return fail_stop ? ROBOT_ERROR : ROBOT_OK; }
int robot_led_set(RobotLed led, int value) {
    if (fail_led) return ROBOT_ERROR;
    leds[led]=value; return ROBOT_OK;
}
int robot_set_motor_speeds(int left,int right) { (void)left;(void)right;moves++;return fail_move?ROBOT_ERROR:ROBOT_OK; }
int robot_move_forward(int speed) { (void)speed; moves++; return fail_move ? ROBOT_ERROR : ROBOT_OK; }
int robot_move_backward(int speed) { return robot_move_forward(speed); }
int robot_turn_left(int speed) { return robot_move_forward(speed); }
int robot_turn_right(int speed) { return robot_move_forward(speed); }
int main(void) {
    unsetenv("ROBOT_SIMULATE");
    assert(control_init()==0);
    assert(!strcmp(control_get_mode(),"manual"));
    assert(leds[ROBOT_LED_MANUAL]==1 && leds[ROBOT_LED_AUTONOMOUS]==0);
    assert(control_move("forward",40)==0 && moves==1);
    int before=stops;
    assert(control_set_mode("automatic")==0 && stops==before+1);
    assert(leds[ROBOT_LED_MANUAL]==0 && leds[ROBOT_LED_AUTONOMOUS]==1);
    assert(control_move("forward",40)==CONTROL_MANUAL_REQUIRED && moves==1);
    assert(control_move("stop",0)==0);
    assert(control_set_mode("invalid")==CONTROL_INVALID_ARGUMENT);
    assert(control_move("forward",101)==CONTROL_INVALID_ARGUMENT);
    fail_led=1;
    assert(control_set_mode("manual")==CONTROL_ERROR);
    assert(!strcmp(control_get_mode(),"unknown"));
    assert(control_move("forward",40)==CONTROL_MANUAL_REQUIRED);
    fail_led=0;
    assert(control_set_mode("manual")==0);
    fail_stop=1;
    assert(control_set_mode("automatic")==CONTROL_ERROR);
    assert(!strcmp(control_get_mode(),"unknown"));
    fail_stop=0;
    assert(control_set_mode("manual")==0);
    fail_move=1;
    before=stops;
    assert(control_move("forward",40)==CONTROL_ERROR && stops==before+1);
    assert(!strcmp(control_get_mode(),"unknown"));
    fail_move=0;
    control_cleanup(); assert(cleaned==1);
    assert(control_move("stop",0)==CONTROL_ERROR);
    setenv("ROBOT_SIMULATE","1",1);
    before=stops;
    assert(control_init()==0);
    assert(control_set_mode("automatic")==0);
    assert(control_move("left",20)==CONTROL_MANUAL_REQUIRED);
    assert(control_move("stop",0)==0);
    control_cleanup(); assert(stops==before && cleaned==1);
    puts("Control: pruebas aprobadas");
}
