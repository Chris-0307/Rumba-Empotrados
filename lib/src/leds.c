#include <gpiod.h>
#include <stddef.h>

#include "robot_hw.h"
#include "hardware_config.h"
#include "leds.h"

#define LED_COUNT 4U

static struct gpiod_chip *led_chip = NULL;
static struct gpiod_line_request *led_request = NULL;

static const unsigned int led_offsets[LED_COUNT] =
{
    GPIO_LED_POWER,
    GPIO_LED_AUTONOMOUS,
    GPIO_LED_MANUAL,
    GPIO_LED_OBSTACLE
};


static unsigned int get_led_offset(RobotLed led)
{
    switch (led)
    {
        case ROBOT_LED_POWER:
            return GPIO_LED_POWER;

        case ROBOT_LED_AUTONOMOUS:
            return GPIO_LED_AUTONOMOUS;

        case ROBOT_LED_MANUAL:
            return GPIO_LED_MANUAL;

        case ROBOT_LED_OBSTACLE:
            return GPIO_LED_OBSTACLE;

        default:
            return 0U;
    }
}


int robot_led_init(void)
{
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_config = NULL;
    struct gpiod_request_config *request_config = NULL;
    int result = ROBOT_ERROR;


    if (led_request != NULL)
    {
        return ROBOT_OK;
    }


    led_chip = gpiod_chip_open(ROBOT_GPIO_CHIP);

    if (led_chip == NULL)
    {
        return ROBOT_ERROR;
    }


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
            led_offsets,
            LED_COUNT,
            settings) < 0)
    {
        goto cleanup;
    }


    gpiod_request_config_set_consumer(
        request_config,
        "librobot-leds"
    );


    led_request = gpiod_chip_request_lines(
        led_chip,
        request_config,
        line_config
    );

    if (led_request == NULL)
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


    if (result != ROBOT_OK)
    {
        if (led_request != NULL)
        {
            gpiod_line_request_release(led_request);
            led_request = NULL;
        }

        if (led_chip != NULL)
        {
            gpiod_chip_close(led_chip);
            led_chip = NULL;
        }
    }


    return result;
}


int robot_led_set(RobotLed led, int state)
{
    unsigned int offset;
    enum gpiod_line_value value;


    if (led < ROBOT_LED_POWER || led > ROBOT_LED_OBSTACLE)
    {
        return ROBOT_INVALID_ARGUMENT;
    }


    if (state != 0 && state != 1)
    {
        return ROBOT_INVALID_ARGUMENT;
    }


    if (led_request == NULL)
    {
        return ROBOT_NOT_INITIALIZED;
    }


    offset = get_led_offset(led);

    value = state != 0
        ? GPIOD_LINE_VALUE_ACTIVE
        : GPIOD_LINE_VALUE_INACTIVE;


    if (gpiod_line_request_set_value(
            led_request,
            offset,
            value) < 0)
    {
        return ROBOT_ERROR;
    }


    return ROBOT_OK;
}


void robot_led_cleanup(void)
{
    enum gpiod_line_value values[LED_COUNT] =
    {
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_INACTIVE
    };


    if (led_request != NULL)
    {
        gpiod_line_request_set_values(
            led_request,
            values
        );

        gpiod_line_request_release(led_request);
        led_request = NULL;
    }


    if (led_chip != NULL)
    {
        gpiod_chip_close(led_chip);
        led_chip = NULL;
    }
}
