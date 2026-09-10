#include "robot_hw.h"


int robot_set_motor_speeds(int left_speed, int right_speed)
{
    if (left_speed < -100 || left_speed > 100 ||
        right_speed < -100 || right_speed > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}


int robot_move_forward(int speed)
{
    if (speed < 0 || speed > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}


int robot_move_backward(int speed)
{
    if (speed < 0 || speed > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}


int robot_turn_left(int speed)
{
    if (speed < 0 || speed > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}


int robot_turn_right(int speed)
{
    if (speed < 0 || speed > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}


int robot_stop(void)
{
    return ROBOT_OK;
}