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

/* Aspiracion independiente de los motores de desplazamiento. */
int robot_suction_set(int enabled);
/* 0 apagada, 1 encendida; negativo si no disponible. Estado de GPIO, no RPM. */
int robot_suction_get(void);


/* =========================================================
 * Sensores
 * =========================================================
 */

#define ROBOT_SENSOR_ERROR (-1.0f)

float robot_get_front_distance(void);

/* Centimetros o ROBOT_SENSOR_ERROR; nunca convertir un error en piso presente. */
float robot_get_floor_distance(void);


/* =========================================================
 * LEDs
 * =========================================================
 */

int robot_led_set(RobotLed led, int state);


/* =========================================================
 * Audio
 * =========================================================
 */

typedef enum { ROBOT_ALERT_START=0, ROBOT_ALERT_MANUAL, ROBOT_ALERT_AUTOMATIC, ROBOT_ALERT_OBSTACLE, ROBOT_ALERT_CYCLE_END } RobotAudioAlert;
/* Encola un aviso; no espera a la reproduccion. Fallo de aviso no afecta movimiento. */
int robot_audio_alert(RobotAudioAlert alert);

int robot_audio_play(const char *filename);

int robot_audio_pause(void);

int robot_audio_stop(void);

int robot_audio_set_volume(int volume);
/* Consultar desde el mismo hilo que controla el audio. */
const char *robot_audio_get_state(void);


#endif