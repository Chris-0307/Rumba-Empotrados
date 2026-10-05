#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/*
 * Configuracion de hardware para Raspberry Pi 4 Model B.
 * Los GPIO usan numeracion BCM.
 */
#define ROBOT_GPIO_CHIP "/dev/gpiochip0"

/* Aspiracion: BCM13, pin fisico 33; salida activa alta hacia BJT. */
#define GPIO_SUCTION 13U

/* =========================
 * LEDs
 * ========================= */
#define GPIO_LED_POWER       17U
#define GPIO_LED_AUTONOMOUS  27U
#define GPIO_LED_MANUAL      22U
#define GPIO_LED_OBSTACLE    23U

/* =========================
 * Motores - L298N
 * =========================
 * ENA -> GPIO18 / PWM0
 * ENB -> GPIO19 / PWM1
 *
 * Los pines ENA y ENB deben tener retirados los jumpers del modulo L298N
 * para poder controlar la velocidad mediante PWM.
 */
#define GPIO_MOTOR_LEFT_IN1   5U
#define GPIO_MOTOR_LEFT_IN2   6U
#define GPIO_MOTOR_RIGHT_IN1 16U
#define GPIO_MOTOR_RIGHT_IN2 20U

#define GPIO_MOTOR_LEFT_PWM  18U
#define GPIO_MOTOR_RIGHT_PWM 19U

/* Cambiar a 1 si un motor queda girando al sentido contrario. */
#define MOTOR_LEFT_REVERSED   0
#define MOTOR_RIGHT_REVERSED  0

/* PWM del kernel de Linux. Requiere dos canales habilitados. */
#define PWM_CHIP_PATH "/sys/class/pwm/pwmchip0"
#define PWM_MOTOR_LEFT_CHANNEL   0U
#define PWM_MOTOR_RIGHT_CHANNEL  1U

/* 1 kHz = periodo de 1 000 000 ns. */
#define MOTOR_PWM_PERIOD_NS 1000000UL

/* =========================
 * Sensor ultrasonico HC-SR04
 * =========================
 * TRIG puede conectarse directamente a un GPIO de 3.3 V.
 * ECHO entrega 5 V y DEBE reducirse a 3.3 V antes de entrar
 * a la Raspberry Pi (por ejemplo, con un divisor resistivo).
 */
#define GPIO_HCSR04_TRIGGER 24U
#define GPIO_HCSR04_ECHO    25U

/* Segundo HC-SR04, orientado al suelo; ECHO requiere divisor. */
#define GPIO_HCSR04_FLOOR_TRIGGER 26U
#define GPIO_HCSR04_FLOOR_ECHO    21U
#define HCSR04_TRIGGER_GAP_NS 65000000ULL

#define HCSR04_TIMEOUT_US 30000U
#define HCSR04_MIN_DISTANCE_CM 2.0f
#define HCSR04_MAX_DISTANCE_CM 400.0f

/* =========================
 * Audio
 * =========================
 * mpg123 se ejecuta en modo remoto para que la reproduccion ocurra en
 * un proceso independiente del control del robot.
 */
#define AUDIO_PLAYER_PATH "/usr/bin/mpg123"
#define AUDIO_DEFAULT_VOLUME 75

#endif
