#include "robot_hw.h"
#include "motor_backend.h"


static int valid_speed(int speed)
{
    return speed >= -100 && speed <= 100;
}


static int valid_movement_speed(int speed)
{
    return speed >= 0 && speed <= 100;
}


int robot_set_motor_speeds(int left_speed, int right_speed)
{
    int result;


    if (!valid_speed(left_speed) ||
        !valid_speed(right_speed))
    {
        return ROBOT_INVALID_ARGUMENT;
    }


    result = motor_backend_set_speed(
        MOTOR_LEFT,
        left_speed
    );

    if (result != ROBOT_OK)
    {
        return result;
    }


    result = motor_backend_set_speed(
        MOTOR_RIGHT,
        right_speed
    );

    if (result != ROBOT_OK)
    {
        return result;
    }


    return ROBOT_OK;
}


int robot_move_forward(int speed)
{
    if (!valid_movement_speed(speed))
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return robot_set_motor_speeds(
        speed,
        speed
    );
}


int robot_move_backward(int speed)
{
    if (!valid_movement_speed(speed))
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return robot_set_motor_speeds(
        -speed,
        -speed
    );
}


int robot_turn_left(int speed)
{
    if (!valid_movement_speed(speed))
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return robot_set_motor_speeds(
        -speed,
        speed
    );
}


int robot_turn_right(int speed)
{
    if (!valid_movement_speed(speed))
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return robot_set_motor_speeds(
        speed,
        -speed
    );
}


int robot_stop(void)
{
    return robot_set_motor_speeds(
        0,
        0
    );
}