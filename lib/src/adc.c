#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

#include "robot_hw.h"
#include "hardware_config.h"
#include "adc.h"

#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG     0x01

#define ADS1115_OS_SINGLE      0x8000
#define ADS1115_PGA_4_096V     0x0200
#define ADS1115_MODE_SINGLE    0x0100
#define ADS1115_DR_250SPS      0x00A0
#define ADS1115_COMP_DISABLE   0x0003

#define ADS1115_FULL_SCALE_VOLTAGE 4.096f

static int adc_fd = -1;


static int adc_write_register(uint8_t reg, uint16_t value)
{
    uint8_t data[3];

    data[0] = reg;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value & 0xFF);

    if (write(adc_fd, data, sizeof(data)) != (ssize_t)sizeof(data))
    {
        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}


static int adc_read_register(uint8_t reg, uint16_t *value)
{
    uint8_t data[2];

    if (value == NULL)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    if (write(adc_fd, &reg, 1) != 1)
    {
        return ROBOT_ERROR;
    }

    if (read(adc_fd, data, sizeof(data)) != (ssize_t)sizeof(data))
    {
        return ROBOT_ERROR;
    }

    *value =
        ((uint16_t)data[0] << 8) |
        (uint16_t)data[1];

    return ROBOT_OK;
}


int adc_init(void)
{
    if (adc_fd >= 0)
    {
        return ROBOT_OK;
    }

    adc_fd = open(I2C_DEVICE, O_RDWR);

    if (adc_fd < 0)
    {
        return ROBOT_ERROR;
    }

    if (ioctl(adc_fd, I2C_SLAVE, ADS1115_ADDRESS) < 0)
    {
        close(adc_fd);
        adc_fd = -1;

        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}


void adc_cleanup(void)
{
    if (adc_fd >= 0)
    {
        close(adc_fd);
        adc_fd = -1;
    }
}


int adc_read_voltage(int channel, float *voltage)
{
    uint16_t mux;
    uint16_t config;
    uint16_t status;
    uint16_t raw_value;
    int16_t raw_signed;
    int i;

    if (adc_fd < 0)
    {
        return ROBOT_NOT_INITIALIZED;
    }

    if (channel < 0 || channel > 3 || voltage == NULL)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    /* Single-ended AIN0, AIN1, AIN2 o AIN3. */
    mux = (uint16_t)(0x4000 + ((uint16_t)channel << 12));

    config =
        ADS1115_OS_SINGLE |
        mux |
        ADS1115_PGA_4_096V |
        ADS1115_MODE_SINGLE |
        ADS1115_DR_250SPS |
        ADS1115_COMP_DISABLE;

    if (adc_write_register(ADS1115_REG_CONFIG, config) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    /* Espera a que termine la conversion. */
    for (i = 0; i < 20; i++)
    {
        usleep(1000);

        if (adc_read_register(ADS1115_REG_CONFIG, &status) != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }

        if ((status & ADS1115_OS_SINGLE) != 0)
        {
            break;
        }
    }

    if (i == 20)
    {
        return ROBOT_ERROR;
    }

    if (adc_read_register(
            ADS1115_REG_CONVERSION,
            &raw_value) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    raw_signed = (int16_t)raw_value;

    if (raw_signed < 0)
    {
        return ROBOT_ERROR;
    }

    *voltage =
        ((float)raw_signed * ADS1115_FULL_SCALE_VOLTAGE) /
        32768.0f;

    return ROBOT_OK;
}
