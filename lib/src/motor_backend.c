#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gpiod.h>

#include "robot_hw.h"
#include "hardware_config.h"
#include "motor_backend.h"

#define MOTOR_GPIO_COUNT 4U

static struct gpiod_chip *motor_chip = NULL;
static struct gpiod_line_request *motor_request = NULL;

static const unsigned int motor_offsets[MOTOR_GPIO_COUNT] =
{
    GPIO_MOTOR_LEFT_IN1,
    GPIO_MOTOR_LEFT_IN2,
    GPIO_MOTOR_RIGHT_IN1,
    GPIO_MOTOR_RIGHT_IN2
};


static int write_text_file(const char *path, const char *text)
{
    int fd;
    ssize_t expected;
    ssize_t written;

    fd = open(path, O_WRONLY);

    if (fd < 0)
    {
        return ROBOT_ERROR;
    }

    expected = (ssize_t)strlen(text);
    written = write(fd, text, (size_t)expected);
    close(fd);

    if (written != expected)
    {
        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}


static int wait_for_path(const char *path)
{
    int i;

    for (i = 0; i < 100; i++)
    {
        if (access(path, F_OK) == 0)
        {
            return ROBOT_OK;
        }

        usleep(10000);
    }

    return ROBOT_ERROR;
}


static int pwm_export(unsigned int channel)
{
    char pwm_path[128];
    char value[16];
    char export_path[128];

    snprintf(
        pwm_path,
        sizeof(pwm_path),
        "%s/pwm%u",
        PWM_CHIP_PATH,
        channel
    );

    if (access(pwm_path, F_OK) == 0)
    {
        return ROBOT_OK;
    }

    snprintf(
        export_path,
        sizeof(export_path),
        "%s/export",
        PWM_CHIP_PATH
    );

    snprintf(value, sizeof(value), "%u", channel);

    if (write_text_file(export_path, value) != ROBOT_OK)
    {
        /* Si otro proceso ya lo exporto, comprobamos si aparecio. */
        if (wait_for_path(pwm_path) != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }
    }

    return wait_for_path(pwm_path);
}


static int pwm_write_value(
    unsigned int channel,
    const char *property,
    unsigned long value)
{
    char path[160];
    char text[32];

    snprintf(
        path,
        sizeof(path),
        "%s/pwm%u/%s",
        PWM_CHIP_PATH,
        channel,
        property
    );

    snprintf(text, sizeof(text), "%lu", value);

    return write_text_file(path, text);
}


static int pwm_prepare(unsigned int channel)
{
    if (pwm_export(channel) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    /* El periodo solo se modifica con el canal deshabilitado. */
    pwm_write_value(channel, "enable", 0UL);

    if (pwm_write_value(
            channel,
            "period",
            MOTOR_PWM_PERIOD_NS) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    if (pwm_write_value(channel, "duty_cycle", 0UL) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    if (pwm_write_value(channel, "enable", 1UL) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}


static int pwm_set_percent(unsigned int channel, int percent)
{
    unsigned long duty_cycle;

    if (percent < 0 || percent > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    duty_cycle =
        (MOTOR_PWM_PERIOD_NS * (unsigned long)percent) / 100UL;

    return pwm_write_value(
        channel,
        "duty_cycle",
        duty_cycle
    );
}


static void pwm_cleanup(unsigned int channel)
{
    char path[128];
    char value[16];

    pwm_set_percent(channel, 0);
    pwm_write_value(channel, "enable", 0UL);

    snprintf(
        path,
        sizeof(path),
        "%s/unexport",
        PWM_CHIP_PATH
    );

    snprintf(value, sizeof(value), "%u", channel);
    write_text_file(path, value);
}


static int motor_set_gpio(unsigned int offset, int state)
{
    enum gpiod_line_value value;

    if (motor_request == NULL)
    {
        return ROBOT_NOT_INITIALIZED;
    }

    value = state != 0
        ? GPIOD_LINE_VALUE_ACTIVE
        : GPIOD_LINE_VALUE_INACTIVE;

    if (gpiod_line_request_set_value(
            motor_request,
            offset,
            value) < 0)
    {
        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}


static int motor_set_direction(MotorId motor, int direction)
{
    unsigned int in1;
    unsigned int in2;
    int reversed;

    if (motor == MOTOR_LEFT)
    {
        in1 = GPIO_MOTOR_LEFT_IN1;
        in2 = GPIO_MOTOR_LEFT_IN2;
        reversed = MOTOR_LEFT_REVERSED;
    }
    else if (motor == MOTOR_RIGHT)
    {
        in1 = GPIO_MOTOR_RIGHT_IN1;
        in2 = GPIO_MOTOR_RIGHT_IN2;
        reversed = MOTOR_RIGHT_REVERSED;
    }
    else
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    if (reversed != 0)
    {
        direction = -direction;
    }

    if (direction > 0)
    {
        if (motor_set_gpio(in1, 1) != ROBOT_OK ||
            motor_set_gpio(in2, 0) != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }
    }
    else if (direction < 0)
    {
        if (motor_set_gpio(in1, 0) != ROBOT_OK ||
            motor_set_gpio(in2, 1) != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }
    }
    else
    {
        if (motor_set_gpio(in1, 0) != ROBOT_OK ||
            motor_set_gpio(in2, 0) != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }
    }

    return ROBOT_OK;
}


int motor_backend_init(void)
{
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_config = NULL;
    struct gpiod_request_config *request_config = NULL;
    int result = ROBOT_ERROR;

    if (motor_request != NULL)
    {
        return ROBOT_OK;
    }

    motor_chip = gpiod_chip_open(ROBOT_GPIO_CHIP);

    if (motor_chip == NULL)
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
            motor_offsets,
            MOTOR_GPIO_COUNT,
            settings) < 0)
    {
        goto cleanup;
    }

    gpiod_request_config_set_consumer(
        request_config,
        "librobot-motors"
    );

    motor_request = gpiod_chip_request_lines(
        motor_chip,
        request_config,
        line_config
    );

    if (motor_request == NULL)
    {
        goto cleanup;
    }

    if (pwm_prepare(PWM_MOTOR_LEFT_CHANNEL) != ROBOT_OK)
    {
        goto cleanup;
    }

    if (pwm_prepare(PWM_MOTOR_RIGHT_CHANNEL) != ROBOT_OK)
    {
        pwm_cleanup(PWM_MOTOR_LEFT_CHANNEL);
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
        if (motor_request != NULL)
        {
            gpiod_line_request_release(motor_request);
            motor_request = NULL;
        }

        if (motor_chip != NULL)
        {
            gpiod_chip_close(motor_chip);
            motor_chip = NULL;
        }
    }

    return result;
}


void motor_backend_cleanup(void)
{
    pwm_cleanup(PWM_MOTOR_LEFT_CHANNEL);
    pwm_cleanup(PWM_MOTOR_RIGHT_CHANNEL);

    if (motor_request != NULL)
    {
        motor_set_direction(MOTOR_LEFT, 0);
        motor_set_direction(MOTOR_RIGHT, 0);

        gpiod_line_request_release(motor_request);
        motor_request = NULL;
    }

    if (motor_chip != NULL)
    {
        gpiod_chip_close(motor_chip);
        motor_chip = NULL;
    }
}


int motor_backend_set_speed(MotorId motor, int speed)
{
    unsigned int pwm_channel;
    int direction;
    int power;

    if (motor_request == NULL)
    {
        return ROBOT_NOT_INITIALIZED;
    }

    if (speed < -100 || speed > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    if (motor == MOTOR_LEFT)
    {
        pwm_channel = PWM_MOTOR_LEFT_CHANNEL;
    }
    else if (motor == MOTOR_RIGHT)
    {
        pwm_channel = PWM_MOTOR_RIGHT_CHANNEL;
    }
    else
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    power = abs(speed);

    if (speed > 0)
    {
        direction = 1;
    }
    else if (speed < 0)
    {
        direction = -1;
    }
    else
    {
        direction = 0;
    }

    /* Evita cambiar de sentido mientras el motor sigue energizado. */
    if (pwm_set_percent(pwm_channel, 0) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    if (motor_set_direction(motor, direction) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    if (pwm_set_percent(pwm_channel, power) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}
