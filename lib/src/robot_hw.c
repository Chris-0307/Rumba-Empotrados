#include "robot_hw.h"
#include "motor_backend.h"
#include "sensor_backend.h"
#include "leds.h"
#include "audio.h"


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


    result = robot_led_init();

    if (result != ROBOT_OK)
    {
        sensor_backend_cleanup();
        motor_backend_cleanup();

        return result;
    }


    result = robot_led_set(
        ROBOT_LED_POWER,
        1
    );

    if (result != ROBOT_OK)
    {
        robot_led_cleanup();
        sensor_backend_cleanup();
        motor_backend_cleanup();

        return result;
    }


    result = robot_audio_init();

    if (result != ROBOT_OK)
    {
        robot_led_set(ROBOT_LED_POWER, 0);
        robot_led_cleanup();
        sensor_backend_cleanup();
        motor_backend_cleanup();

        return result;
    }


    return ROBOT_OK;
}


void robot_cleanup(void)
{
    robot_stop();

    robot_audio_stop();
    robot_audio_cleanup();

    robot_led_set(
        ROBOT_LED_POWER,
        0
    );

    robot_led_cleanup();

    sensor_backend_cleanup();

    motor_backend_cleanup();
}
