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
static atomic_int floor_value=7, front_value=48, stop_count, move_count, audio_count, turn_count;
static void pause_ms(long ms) { struct timespec ts={ms/1000,(ms%1000)*1000000}; nanosleep(&ts,NULL); }
static atomic_int alert_counts[5];
int robot_audio_alert(RobotAudioAlert event) {assert(event>=0 && event<=4);atomic_fetch_add(&alert_counts[event],1);return 0;}
static atomic_int suction_state, suction_failure;
int robot_init(void) { atomic_store(&suction_state,0);return 0; }
int robot_suction_set(int value) { if (atomic_load(&suction_failure)) return -1;atomic_store(&suction_state,value);return 0; }
int robot_suction_get(void) { return atomic_load(&suction_failure)?-3:atomic_load(&suction_state); }
void robot_cleanup(void) {}
int robot_stop(void) { atomic_fetch_add(&stop_count,1); return 0; }
int robot_led_set(RobotLed led,int state) { (void)led;(void)state; return 0; }
static atomic_int pair_left,pair_right,pair_calls;
int robot_set_motor_speeds(int left,int right) {
    atomic_store(&pair_left,left);atomic_store(&pair_right,right);atomic_fetch_add(&pair_calls,1);
    if (left*right<0) atomic_fetch_add(&turn_count,1);
    atomic_fetch_add(&move_count,1);return 0;
}
int robot_move_forward(int speed) { (void)speed; atomic_fetch_add(&move_count,1); return 0; }
int robot_move_backward(int s) { return robot_move_forward(s); }
int robot_turn_left(int s) { assert(s==25); atomic_fetch_add(&turn_count,1);return robot_move_forward(s); }
int robot_turn_right(int s) { assert(s==25); atomic_fetch_add(&turn_count,1);return robot_move_forward(s); }
float robot_get_floor_distance(void) { pause_ms(65); return atomic_load(&floor_value); }
float robot_get_front_distance(void) { pause_ms(65); return atomic_load(&front_value); }
int robot_audio_set_volume(int v) { (void)v; atomic_fetch_add(&audio_count,1); return 0; }
int robot_audio_play(const char *p) { (void)p;return 0; }
int robot_audio_pause(void) { return 0; }
int robot_audio_stop(void) { return 0; }
const char *robot_audio_get_state(void) { return "playing"; }
int main(void) {
    char json[1024];unsetenv("ROBOT_SIMULATE");setenv("ROBOT_FLOOR_MAX_CM","4",1);
    atomic_store(&floor_value,3);assert(robot_adapter_init()==0);assert(atomic_load(&alert_counts[ROBOT_ALERT_START])==1);
    assert(robot_adapter_move("forward",30)==-4);pause_ms(450);
    assert(robot_adapter_suction_json(json,sizeof json)==0 && strstr(json,"\"enabled\":false"));
    assert(robot_adapter_suction_set(2)==-2);
    assert(robot_adapter_suction_set(1)==0);assert(atomic_load(&suction_state)==1);
    assert(robot_adapter_move("forward",30)==0);
    int before=atomic_load(&stop_count);atomic_store(&floor_value,5);pause_ms(300);
    assert(atomic_load(&stop_count)>before);assert(robot_adapter_move("backward",30)==-4);
    assert(atomic_load(&suction_state)==1); /* Aspiracion independiente del movimiento. */
    assert(robot_adapter_suction_set(0)==0);assert(atomic_load(&suction_state)==0);
    atomic_store(&floor_value,3);pause_ms(450);assert(robot_adapter_move("forward",30)==-4);
    assert(robot_adapter_move("stop",0)==0);assert(robot_adapter_move("forward",30)==0);
    assert(robot_adapter_mode("automatic")==0);pause_ms(550);assert(atomic_load(&alert_counts[ROBOT_ALERT_AUTOMATIC])==1);assert(robot_adapter_mode("automatic")==0);assert(atomic_load(&alert_counts[ROBOT_ALERT_AUTOMATIC])==1);pause_ms(550);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"forward"));
    atomic_store(&front_value,10);before=atomic_load(&turn_count);pause_ms(700);assert(atomic_load(&turn_count)>before);
    char turning[1024];assert(robot_adapter_mode_json(turning,sizeof turning)==0);
    const char *side=strstr(turning,"turn_left")?"turn_left":"turn_right";
    assert(atomic_load(&alert_counts[ROBOT_ALERT_OBSTACLE])==1);
    before=atomic_load(&turn_count);pause_ms(1400);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,side));
    assert(atomic_load(&turn_count)==before); /* Misma orden, sin alternar/repetir. */
    assert(atomic_load(&alert_counts[ROBOT_ALERT_OBSTACLE])==1);
    atomic_store(&front_value,48);pause_ms(1000);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"forward"));

    assert(robot_adapter_move("stop",0)==0);assert(robot_adapter_mode("manual")==0);assert(atomic_load(&alert_counts[ROBOT_ALERT_MANUAL])==1);
    atomic_store(&suction_failure,1);assert(robot_adapter_suction_set(1)==-1);
    assert(robot_adapter_suction_json(json,sizeof json)==0 && strstr(json,"\"enabled\":null"));
    assert(robot_adapter_audio("volume",0,70)==0); /* Fallo de aspiracion no bloquea audio. */
    atomic_store(&suction_failure,0);
    assert(robot_adapter_suction_set(1)==0);pause_ms(3000);assert(atomic_load(&suction_state)==1);
    assert(robot_adapter_suction_set(1)==0);pause_ms(2300);assert(atomic_load(&suction_state)==1); /* Renovacion. */
    pause_ms(2900);assert(atomic_load(&suction_state)==0); /* Vence sin HTTP. */
    assert(robot_adapter_mode_timed("automatic",-1)==-2);
    assert(robot_adapter_mode_timed("automatic",86401)==-2);
    assert(robot_adapter_mode_timed("automatic",1)==0);
    assert(robot_adapter_suction_set(1)==0);
    int moves_before=atomic_load(&move_count);
    pause_ms(1500); /* Sin consultas HTTP: el trabajador finaliza el ciclo. */
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"cycle_completed") && strstr(json,"\"status\":\"completed\""));
    assert(atomic_load(&suction_state)==0);
    assert(atomic_load(&alert_counts[ROBOT_ALERT_CYCLE_END])==1);
    assert(atomic_load(&move_count)>=moves_before);
    assert(robot_adapter_suction_set(1)==-5);
    moves_before=atomic_load(&move_count);pause_ms(400);
    assert(atomic_load(&move_count)==moves_before);
    assert(atomic_load(&alert_counts[ROBOT_ALERT_CYCLE_END])==1);
    assert(robot_adapter_move("stop",0)==0);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"cycle_completed"));
    assert(robot_adapter_mode_timed("automatic",1)==0);
    assert(robot_adapter_move("stop",0)==0);pause_ms(1200);
    assert(atomic_load(&alert_counts[ROBOT_ALERT_CYCLE_END])==1); /* Cancelar no notifica fin. */
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"cancelled"));
    assert(robot_adapter_mode_timed("automatic",0)==0);pause_ms(1200);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"\"status\":\"running\"") && strstr(json,"\"remaining_seconds\":null"));
    assert(robot_adapter_mode_timed("automatic",1)==0);
    atomic_store(&suction_failure,0);assert(robot_adapter_suction_set(1)==0);
    atomic_store(&suction_failure,1);pause_ms(1400);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"finishing"));
    assert(atomic_load(&alert_counts[ROBOT_ALERT_CYCLE_END])==1);
    atomic_store(&suction_failure,0);pause_ms(300);
    assert(atomic_load(&alert_counts[ROBOT_ALERT_CYCLE_END])==2);
    assert(atomic_load(&suction_state)==0);
    assert(robot_adapter_mode("manual")==0);
    assert(robot_adapter_motors_json(json,sizeof json)==0 && strstr(json,"\"left_speed\":30"));
    assert(robot_adapter_motors_set(-1,30)==-2);
    assert(robot_adapter_motors_set(30,101)==-2);
    int pairs_before=atomic_load(&pair_calls);
    assert(robot_adapter_motors_set(40,60)==0);
    assert(atomic_load(&pair_calls)==pairs_before); /* Guardar parado no inicia movimiento. */
    assert(robot_adapter_motors_json(json,sizeof json)==0 && strstr(json,"\"turn_left_speed\":33") && strstr(json,"\"turn_right_speed\":50"));
    assert(robot_adapter_move_configured("forward")==0);
    assert(atomic_load(&pair_left)==40 && atomic_load(&pair_right)==60);
    assert(robot_adapter_motors_set(35,45)==0);
    assert(atomic_load(&pair_left)==35 && atomic_load(&pair_right)==45); /* Cambio en marcha. */
    assert(robot_adapter_move_configured("backward")==0);
    assert(atomic_load(&pair_left)==-35 && atomic_load(&pair_right)==-45);
    assert(robot_adapter_move_configured("left")==0);
    assert(atomic_load(&pair_left)==-35 && atomic_load(&pair_right)==45);
    assert(robot_adapter_move_configured("right")==0);
    assert(atomic_load(&pair_left)==35 && atomic_load(&pair_right)==-45);
    assert(robot_adapter_move("stop",0)==0);
    assert(robot_adapter_motors_set(0,40)==0);
    assert(robot_adapter_move_configured("forward")==0);
    assert(atomic_load(&pair_left)==0 && atomic_load(&pair_right)==40);
    assert(robot_adapter_motors_set(0,0)==0);
    pairs_before=atomic_load(&pair_calls);
    assert(robot_adapter_motors_set(40,60)==0);
    assert(atomic_load(&pair_calls)==pairs_before); /* No reactivar tras parada. */
    assert(robot_adapter_mode("automatic")==0);pause_ms(550);
    assert(atomic_load(&pair_left)==40 && atomic_load(&pair_right)==60);
    assert(robot_adapter_motors_set(30,36)==0);pause_ms(250);
    assert(atomic_load(&pair_left)==30 && atomic_load(&pair_right)==36);
    atomic_store(&front_value,10);pause_ms(700);
    assert(abs(atomic_load(&pair_left))==25 && abs(atomic_load(&pair_right))==30);
    assert(atomic_load(&pair_left)*atomic_load(&pair_right)<0);
    assert(robot_adapter_motors_set(0,0)==0);pause_ms(250);
    assert(robot_adapter_mode_json(json,sizeof json)==0 && strstr(json,"zero_speed"));
    pairs_before=atomic_load(&pair_calls);
    assert(robot_adapter_motors_set(30,30)==0);pause_ms(300);
    assert(atomic_load(&pair_calls)==pairs_before);
    assert(robot_adapter_mode("manual")==0);
    assert(robot_adapter_suction_set(1)==0);robot_adapter_cleanup();assert(atomic_load(&suction_state)==0);
    puts("OK: aspiracion, renovacion, apagado por timeout/cierre y proteccion de suelo original.");
}
