#include "robot_hw.h"
#include "motor_backend.h"


int robot_init(void)
{
    int result;


    result = motor_backend_init();

    if (result != ROBOT_OK)
    {
        return result;
    }


    return ROBOT_OK;
}


void robot_cleanup(void)
{
    robot_stop();

    motor_backend_cleanup();
}