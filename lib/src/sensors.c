#include "robot_hw.h"
#include "sensor_backend.h"


float robot_get_front_distance(void)
{
    return sensor_backend_get_distance(
        SENSOR_FRONT
    );
}


float robot_get_left_distance(void)
{
    return sensor_backend_get_distance(
        SENSOR_LEFT
    );
}


float robot_get_right_distance(void)
{
    return sensor_backend_get_distance(
        SENSOR_RIGHT
    );
}