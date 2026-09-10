#include "robot_hw.h"
#include "motor_backend.h"
#include "sensor_backend.h"


int robot_init(void)
{
    int result;


    result = motor_backend_init();

    if (result != ROBOT_OK)
    {
        return result;
    }


    result = sensor_backend_init();

    if (result != ROBOT_OK)
    {
        motor_backend_cleanup();

        return result;
    }


    return ROBOT_OK;
}


void robot_cleanup(void)
{
    robot_stop();

    sensor_backend_cleanup();

    motor_backend_cleanup();
}