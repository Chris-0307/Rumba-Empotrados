#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/*
 * Configuracion GPIO para Raspberry Pi 4 Model B.
 * Los numeros corresponden a la numeracion BCM (offsets de gpiochip0).
 */
#define ROBOT_GPIO_CHIP "/dev/gpiochip0"

#define GPIO_LED_POWER       17U
#define GPIO_LED_AUTONOMOUS  27U
#define GPIO_LED_MANUAL      22U
#define GPIO_LED_OBSTACLE    23U

#endif
