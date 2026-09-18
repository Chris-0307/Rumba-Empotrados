#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/*
 * Configuracion de hardware para Raspberry Pi 4 Model B.
 * Los GPIO usan numeracion BCM.
 */
#define ROBOT_GPIO_CHIP "/dev/gpiochip0"

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
 * ADC ADS1115 + Sharp
 * ========================= */
#define I2C_DEVICE "/dev/i2c-1"
#define ADS1115_ADDRESS 0x48

#define SENSOR_FRONT_CHANNEL 0
#define SENSOR_LEFT_CHANNEL  1
#define SENSOR_RIGHT_CHANNEL 2

#define SHARP_MIN_DISTANCE_CM 10.0f
#define SHARP_MAX_DISTANCE_CM 80.0f

#endif
