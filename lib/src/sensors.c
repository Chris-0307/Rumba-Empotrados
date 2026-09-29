#include "robot_hw.h"
#include "sensor_backend.h"

float robot_get_front_distance(void)
{
    return sensor_backend_get_distance();
}
