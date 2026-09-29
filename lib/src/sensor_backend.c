#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <gpiod.h>

#include "robot_hw.h"
#include "hardware_config.h"
#include "sensor_backend.h"

static struct gpiod_chip *sensor_chip = NULL;
static struct gpiod_line_request *trigger_request = NULL;
static struct gpiod_line_request *echo_request = NULL;


static uint64_t monotonic_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0U;
    }

    return ((uint64_t)ts.tv_sec * 1000000ULL) +
           ((uint64_t)ts.tv_nsec / 1000ULL);
}


static int wait_for_echo(
    enum gpiod_line_value expected,
    uint64_t timeout_us,
    uint64_t *timestamp_us)
{
    uint64_t start;

    start = monotonic_us();

    if (start == 0U)
    {
        return ROBOT_ERROR;
    }

    for (;;)
    {
        enum gpiod_line_value value;
        uint64_t now;

        value = gpiod_line_request_get_value(
            echo_request,
            GPIO_HCSR04_ECHO
        );

        if (value == GPIOD_LINE_VALUE_ERROR)
        {
            return ROBOT_ERROR;
        }

        now = monotonic_us();

        if (now == 0U)
        {
            return ROBOT_ERROR;
        }

        if (value == expected)
        {
            if (timestamp_us != NULL)
            {
                *timestamp_us = now;
            }

            return ROBOT_OK;
        }

        if ((now - start) >= timeout_us)
        {
            return ROBOT_ERROR;
        }
    }
}


static int request_trigger_line(void)
{
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_config = NULL;
    struct gpiod_request_config *request_config = NULL;
    const unsigned int offset = GPIO_HCSR04_TRIGGER;
    int result = ROBOT_ERROR;

    settings = gpiod_line_settings_new();
    line_config = gpiod_line_config_new();
    request_config = gpiod_request_config_new();

    if (settings == NULL ||
        line_config == NULL ||
        request_config == NULL)
    {
        goto cleanup;
    }

    if (gpiod_line_settings_set_direction(
            settings,
            GPIOD_LINE_DIRECTION_OUTPUT) < 0)
    {
        goto cleanup;
    }

    if (gpiod_line_settings_set_output_value(
            settings,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        goto cleanup;
    }

    if (gpiod_line_config_add_line_settings(
            line_config,
            &offset,
            1U,
            settings) < 0)
    {
        goto cleanup;
    }

    gpiod_request_config_set_consumer(
        request_config,
        "librobot-hcsr04-trigger"
    );

    trigger_request = gpiod_chip_request_lines(
        sensor_chip,
        request_config,
        line_config
    );

    if (trigger_request == NULL)
    {
        goto cleanup;
    }

    result = ROBOT_OK;

cleanup:
    if (request_config != NULL)
    {
        gpiod_request_config_free(request_config);
    }

    if (line_config != NULL)
    {
        gpiod_line_config_free(line_config);
    }

    if (settings != NULL)
    {
        gpiod_line_settings_free(settings);
    }

    return result;
}


static int request_echo_line(void)
{
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_config = NULL;
    struct gpiod_request_config *request_config = NULL;
    const unsigned int offset = GPIO_HCSR04_ECHO;
    int result = ROBOT_ERROR;

    settings = gpiod_line_settings_new();
    line_config = gpiod_line_config_new();
    request_config = gpiod_request_config_new();

    if (settings == NULL ||
        line_config == NULL ||
        request_config == NULL)
    {
        goto cleanup;
    }

    if (gpiod_line_settings_set_direction(
            settings,
            GPIOD_LINE_DIRECTION_INPUT) < 0)
    {
        goto cleanup;
    }

    if (gpiod_line_config_add_line_settings(
            line_config,
            &offset,
            1U,
            settings) < 0)
    {
        goto cleanup;
    }

    gpiod_request_config_set_consumer(
        request_config,
        "librobot-hcsr04-echo"
    );

    echo_request = gpiod_chip_request_lines(
        sensor_chip,
        request_config,
        line_config
    );

    if (echo_request == NULL)
    {
        goto cleanup;
    }

    result = ROBOT_OK;

cleanup:
    if (request_config != NULL)
    {
        gpiod_request_config_free(request_config);
    }

    if (line_config != NULL)
    {
        gpiod_line_config_free(line_config);
    }

    if (settings != NULL)
    {
        gpiod_line_settings_free(settings);
    }

    return result;
}


int sensor_backend_init(void)
{
    if (trigger_request != NULL && echo_request != NULL)
    {
        return ROBOT_OK;
    }

    sensor_chip = gpiod_chip_open(ROBOT_GPIO_CHIP);

    if (sensor_chip == NULL)
    {
        return ROBOT_ERROR;
    }

    if (request_trigger_line() != ROBOT_OK)
    {
        sensor_backend_cleanup();
        return ROBOT_ERROR;
    }

    if (request_echo_line() != ROBOT_OK)
    {
        sensor_backend_cleanup();
        return ROBOT_ERROR;
    }

    /* El HC-SR04 debe permanecer con TRIG en bajo antes de medir. */
    if (gpiod_line_request_set_value(
            trigger_request,
            GPIO_HCSR04_TRIGGER,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        sensor_backend_cleanup();
        return ROBOT_ERROR;
    }

    usleep(2000);

    return ROBOT_OK;
}


void sensor_backend_cleanup(void)
{
    if (echo_request != NULL)
    {
        gpiod_line_request_release(echo_request);
        echo_request = NULL;
    }

    if (trigger_request != NULL)
    {
        gpiod_line_request_set_value(
            trigger_request,
            GPIO_HCSR04_TRIGGER,
            GPIOD_LINE_VALUE_INACTIVE
        );

        gpiod_line_request_release(trigger_request);
        trigger_request = NULL;
    }

    if (sensor_chip != NULL)
    {
        gpiod_chip_close(sensor_chip);
        sensor_chip = NULL;
    }
}


float sensor_backend_get_distance(void)
{
    uint64_t echo_start;
    uint64_t echo_end;
    uint64_t pulse_us;
    float distance_cm;

    if (trigger_request == NULL || echo_request == NULL)
    {
        return ROBOT_SENSOR_ERROR;
    }

    /* Pulso de disparo de al menos 10 us. */
    if (gpiod_line_request_set_value(
            trigger_request,
            GPIO_HCSR04_TRIGGER,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        return ROBOT_SENSOR_ERROR;
    }

    usleep(2);

    if (gpiod_line_request_set_value(
            trigger_request,
            GPIO_HCSR04_TRIGGER,
            GPIOD_LINE_VALUE_ACTIVE) < 0)
    {
        return ROBOT_SENSOR_ERROR;
    }

    usleep(10);

    if (gpiod_line_request_set_value(
            trigger_request,
            GPIO_HCSR04_TRIGGER,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        return ROBOT_SENSOR_ERROR;
    }

    if (wait_for_echo(
            GPIOD_LINE_VALUE_ACTIVE,
            HCSR04_TIMEOUT_US,
            &echo_start) != ROBOT_OK)
    {
        return ROBOT_SENSOR_ERROR;
    }

    if (wait_for_echo(
            GPIOD_LINE_VALUE_INACTIVE,
            HCSR04_TIMEOUT_US,
            &echo_end) != ROBOT_OK)
    {
        return ROBOT_SENSOR_ERROR;
    }

    if (echo_end <= echo_start)
    {
        return ROBOT_SENSOR_ERROR;
    }

    pulse_us = echo_end - echo_start;

    /* Velocidad del sonido aproximada: 0.0343 cm/us. Ida y vuelta / 2. */
    distance_cm = ((float)pulse_us * 0.0343f) / 2.0f;

    if (distance_cm < HCSR04_MIN_DISTANCE_CM ||
        distance_cm > HCSR04_MAX_DISTANCE_CM)
    {
        return ROBOT_SENSOR_ERROR;
    }

    return distance_cm;
}
