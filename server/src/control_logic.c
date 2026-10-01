#include "control_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ROBOT_WITH_HARDWARE
#include <robot_hw.h>
#endif

typedef enum { MODE_UNKNOWN, MODE_MANUAL, MODE_AUTOMATIC } ControlMode;
static ControlMode current_mode = MODE_UNKNOWN;
static int initialized;
static int simulated;

const char *control_get_mode(void) {
    if (current_mode == MODE_MANUAL) return "manual";
    if (current_mode == MODE_AUTOMATIC) return "automatic";
    return "unknown";
}

int control_stop(void) {
    if (!initialized) return CONTROL_ERROR;
    if (simulated) {
        fprintf(stderr, "SIMULACION: stop (sin GPIO)\n");
        return CONTROL_OK;
    }
#ifdef ROBOT_WITH_HARDWARE
    return robot_stop() == ROBOT_OK ? CONTROL_OK : CONTROL_ERROR;
#else
    return CONTROL_ERROR;
#endif
}

int control_set_mode(const char *mode) {
    if (!mode || (strcmp(mode, "manual") && strcmp(mode, "automatic")))
        return CONTROL_INVALID_ARGUMENT;
    if (!initialized) return CONTROL_ERROR;
    ControlMode next = !strcmp(mode, "manual") ? MODE_MANUAL : MODE_AUTOMATIC;
    if (control_stop() != CONTROL_OK) {
        current_mode = MODE_UNKNOWN;
        return CONTROL_ERROR;
    }
    if (simulated) {
        fprintf(stderr, "SIMULACION: manual_led=%d automatic_led=%d\n",
                next == MODE_MANUAL, next == MODE_AUTOMATIC);
    } else {
#ifdef ROBOT_WITH_HARDWARE
        /* Apagar ambos antes de encender el seleccionado. */
        int a = robot_led_set(ROBOT_LED_MANUAL, 0);
        int b = robot_led_set(ROBOT_LED_AUTONOMOUS, 0);
        if (a != ROBOT_OK || b != ROBOT_OK ||
            robot_led_set(next == MODE_MANUAL ? ROBOT_LED_MANUAL : ROBOT_LED_AUTONOMOUS, 1) != ROBOT_OK) {
            current_mode = MODE_UNKNOWN;
            (void)robot_led_set(ROBOT_LED_MANUAL, 0);
            (void)robot_led_set(ROBOT_LED_AUTONOMOUS, 0);
            return CONTROL_ERROR;
        }
#else
        return CONTROL_ERROR;
#endif
    }
    current_mode = next;
    return CONTROL_OK;
}

int control_init(void) {
    if (initialized) return CONTROL_OK;
    const char *value = getenv("ROBOT_SIMULATE");
    simulated = value && !strcmp(value, "1");
    if (!simulated) {
#ifdef ROBOT_WITH_HARDWARE
        if (robot_init() != ROBOT_OK) {
            robot_cleanup();
            return CONTROL_ERROR;
        }
#else
        fprintf(stderr, "Compilado sin hardware: requiere ROBOT_SIMULATE=1\n");
        return CONTROL_ERROR;
#endif
    }
    initialized = 1;
    if (control_set_mode("manual") != CONTROL_OK) {
        control_cleanup();
        return CONTROL_ERROR;
    }
    return CONTROL_OK;
}

void control_cleanup(void) {
    if (!initialized) return;
    (void)control_stop();
#ifdef ROBOT_WITH_HARDWARE
    if (!simulated) {
        (void)robot_led_set(ROBOT_LED_MANUAL, 0);
        (void)robot_led_set(ROBOT_LED_AUTONOMOUS, 0);
        robot_cleanup();
    }
#endif
    initialized = 0;
    current_mode = MODE_UNKNOWN;
}

int control_move(const char *direction, int speed) {
    if (!direction || speed < 0 || speed > 100) return CONTROL_INVALID_ARGUMENT;
    if (strcmp(direction, "forward") && strcmp(direction, "backward") &&
        strcmp(direction, "left") && strcmp(direction, "right") && strcmp(direction, "stop"))
        return CONTROL_INVALID_ARGUMENT;
    if (!initialized) return CONTROL_ERROR;
    if (!strcmp(direction, "stop")) return control_stop();
    if (current_mode != MODE_MANUAL) return CONTROL_MANUAL_REQUIRED;
    if (simulated) {
        fprintf(stderr, "SIMULACION: move=%s speed=%d (sin GPIO)\n", direction, speed);
        return CONTROL_OK;
    }
#ifdef ROBOT_WITH_HARDWARE
    int result;
    if (!strcmp(direction, "forward")) result = robot_move_forward(speed);
    else if (!strcmp(direction, "backward")) result = robot_move_backward(speed);
    else if (!strcmp(direction, "left")) result = robot_turn_left(speed);
    else result = robot_turn_right(speed);
    if (result != ROBOT_OK) {
        (void)robot_stop();
        current_mode = MODE_UNKNOWN;
        return CONTROL_ERROR;
    }
    return CONTROL_OK;
#else
    return CONTROL_ERROR;
#endif
}
