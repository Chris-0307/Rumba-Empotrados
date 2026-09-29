#include <math.h>

#include "robot_hw.h"
#include "hardware_config.h"
#include "sensor_backend.h"
#include "adc.h"


static int get_sensor_channel(SensorId sensor)
{
    switch (sensor)
    {
        case SENSOR_FRONT:
            return SENSOR_FRONT_CHANNEL;

        case SENSOR_LEFT:
            return SENSOR_LEFT_CHANNEL;

        case SENSOR_RIGHT:
            return SENSOR_RIGHT_CHANNEL;

        default:
            return -1;
    }
}


static float sharp_voltage_to_distance(float voltage)
{
    float distance;

    if (voltage <= 0.0f)
    {
        return ROBOT_SENSOR_ERROR;
    }

    /*
     * Aproximacion inicial para GP2Y0A21YK0F.
     * Debe ajustarse con la calibracion real de los tres sensores.
     */
    distance = 29.988f * powf(voltage, -1.173f);

    if (distance < SHARP_MIN_DISTANCE_CM)
    {
        distance = SHARP_MIN_DISTANCE_CM;
    }

    if (distance > SHARP_MAX_DISTANCE_CM)
    {
        distance = SHARP_MAX_DISTANCE_CM;
    }

    return distance;
}


int sensor_backend_init(void)
{
    return adc_init();
}


void sensor_backend_cleanup(void)
{
    adc_cleanup();
}


float sensor_backend_get_distance(SensorId sensor)
{
    int channel;
    float voltage;

    channel = get_sensor_channel(sensor);

    if (channel < 0)
    {
        return ROBOT_SENSOR_ERROR;
    }

    if (adc_read_voltage(channel, &voltage) != ROBOT_OK)
    {
        return ROBOT_SENSOR_ERROR;
    }

    return sharp_voltage_to_distance(voltage);
}
