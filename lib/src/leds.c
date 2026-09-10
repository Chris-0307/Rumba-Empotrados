#include "robot_hw.h"


int robot_led_set(RobotLed led, int state)
{
    if (led < ROBOT_LED_POWER || led > ROBOT_LED_OBSTACLE)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    if (state != 0 && state != 1)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}