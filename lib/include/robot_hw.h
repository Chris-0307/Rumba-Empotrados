#ifndef ROBOT_HW_H
#define ROBOT_HW_H


// API de la biblioteca de la rumba

/* =========================================================
 * Codigos de retorno
 * =========================================================
 */

typedef enum
{
    ROBOT_OK = 0,
    ROBOT_ERROR = -1,
    ROBOT_INVALID_ARGUMENT = -2,
    ROBOT_NOT_INITIALIZED = -3

} RobotStatus;


/* =========================================================
 * LEDs
 * =========================================================
 */

typedef enum
{
    ROBOT_LED_POWER = 0,
    ROBOT_LED_AUTONOMOUS,
    ROBOT_LED_MANUAL,
    ROBOT_LED_OBSTACLE

} RobotLed;


/* =========================================================
 * Inicializacion
 * =========================================================
 */

int robot_init(void);

void robot_cleanup(void);


/* =========================================================
 * Motores
 * =========================================================
 */

int robot_set_motor_speeds(int left_speed, int right_speed);

int robot_move_forward(int speed);

int robot_move_backward(int speed);

int robot_turn_left(int speed);

int robot_turn_right(int speed);

int robot_stop(void);


/* =========================================================
 * Sensores
 * =========================================================
 */

float robot_get_front_distance(void);

float robot_get_left_distance(void);

float robot_get_right_distance(void);


/* =========================================================
 * LEDs
 * =========================================================
 */

int robot_led_set(RobotLed led, int state);


/* =========================================================
 * Audio
 * =========================================================
 */

int robot_audio_play(const char *filename);

int robot_audio_pause(void);

int robot_audio_stop(void);

int robot_audio_set_volume(int volume);


#endif