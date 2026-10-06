#include "robot_hw.h"
#include "hardware_config.h"
#include "suction.h"
#include <gpiod.h>

static struct gpiod_chip *chip;
static struct gpiod_line_request *request;
/* Serializar desde el controlador, igual que las otras funciones hardware. */
int robot_suction_init(void) {
    if (request) return ROBOT_OK;
    struct gpiod_line_settings *settings=NULL;
    struct gpiod_line_config *lines=NULL;
    struct gpiod_request_config *config=NULL;
    unsigned int offset=GPIO_SUCTION;
    chip=gpiod_chip_open(ROBOT_GPIO_CHIP);
    if (!chip) return ROBOT_ERROR;
    settings=gpiod_line_settings_new();lines=gpiod_line_config_new();config=gpiod_request_config_new();
    if (!settings || !lines || !config) goto done;
    if (gpiod_line_settings_set_direction(settings,GPIOD_LINE_DIRECTION_OUTPUT) ||
        gpiod_line_settings_set_output_value(settings,GPIOD_LINE_VALUE_INACTIVE) ||
        gpiod_line_config_add_line_settings(lines,&offset,1,settings)) goto done;
    gpiod_request_config_set_consumer(config,"librobot-suction");
    request=gpiod_chip_request_lines(chip,config,lines);
done:
    if (settings) gpiod_line_settings_free(settings);
    if (lines) gpiod_line_config_free(lines);
    if (config) gpiod_request_config_free(config);
    if (!request) {gpiod_chip_close(chip);chip=NULL;return ROBOT_ERROR;}
    return ROBOT_OK;
}
int robot_suction_set(int enabled) {
    if (enabled!=0 && enabled!=1) return ROBOT_INVALID_ARGUMENT;
    if (!request) return ROBOT_NOT_INITIALIZED;
    return gpiod_line_request_set_value(request,GPIO_SUCTION,enabled?GPIOD_LINE_VALUE_ACTIVE:GPIOD_LINE_VALUE_INACTIVE)?ROBOT_ERROR:ROBOT_OK;
}
int robot_suction_get(void) {
    if (!request) return ROBOT_NOT_INITIALIZED;
    enum gpiod_line_value value=gpiod_line_request_get_value(request,GPIO_SUCTION);
    if (value==GPIOD_LINE_VALUE_ERROR) return ROBOT_ERROR;
    return value==GPIOD_LINE_VALUE_ACTIVE?1:0;
}
void robot_suction_cleanup(void) {
    if (request) {(void)robot_suction_set(0);gpiod_line_request_release(request);request=NULL;}
    if (chip) {gpiod_chip_close(chip);chip=NULL;}
}
