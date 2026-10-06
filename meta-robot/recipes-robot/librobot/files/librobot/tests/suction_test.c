#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <gpiod.h>
#include "robot_hw.h"
#include "suction.h"
struct gpiod_chip {int unused;};
struct gpiod_line_request {int unused;};
struct gpiod_line_settings {int unused;};
struct gpiod_line_config {int unused;};
struct gpiod_request_config {int unused;};
static struct gpiod_chip chip;
static struct gpiod_line_request request;
static struct gpiod_line_settings settings;
static struct gpiod_line_config lines;
static struct gpiod_request_config config;
static enum gpiod_line_value value;
static int failed, released, closed;
struct gpiod_chip *gpiod_chip_open(const char *path) {assert(!strcmp(path,"/dev/gpiochip0"));return &chip;}
void gpiod_chip_close(struct gpiod_chip *c) {(void)c;closed++;}
struct gpiod_line_settings *gpiod_line_settings_new(void) {return &settings;}
void gpiod_line_settings_free(struct gpiod_line_settings *s) {(void)s;}
int gpiod_line_settings_set_direction(struct gpiod_line_settings *s,enum gpiod_line_direction d) {(void)s;assert(d==GPIOD_LINE_DIRECTION_OUTPUT);return 0;}
int gpiod_line_settings_set_output_value(struct gpiod_line_settings *s,enum gpiod_line_value v) {(void)s;value=v;return 0;}
struct gpiod_line_config *gpiod_line_config_new(void) {return &lines;}
void gpiod_line_config_free(struct gpiod_line_config *l) {(void)l;}
int gpiod_line_config_add_line_settings(struct gpiod_line_config *l,const unsigned int *o,size_t n,struct gpiod_line_settings *s) {(void)l;(void)s;assert(n==1 && *o==13);return 0;}
struct gpiod_request_config *gpiod_request_config_new(void) {return &config;}
void gpiod_request_config_free(struct gpiod_request_config *c) {(void)c;}
void gpiod_request_config_set_consumer(struct gpiod_request_config *c,const char *name) {(void)c;assert(!strcmp(name,"librobot-suction"));}
struct gpiod_line_request *gpiod_chip_request_lines(struct gpiod_chip *c,struct gpiod_request_config *r,struct gpiod_line_config *l) {(void)c;(void)r;(void)l;assert(value==GPIOD_LINE_VALUE_INACTIVE);return failed?NULL:&request;}
int gpiod_line_request_set_value(struct gpiod_line_request *r,unsigned int o,enum gpiod_line_value v) {(void)r;assert(o==13);if(failed)return -1;value=v;return 0;}
enum gpiod_line_value gpiod_line_request_get_value(struct gpiod_line_request *r,unsigned int o) {(void)r;assert(o==13);return failed?GPIOD_LINE_VALUE_ERROR:value;}
void gpiod_line_request_release(struct gpiod_line_request *r) {(void)r;released++;}
int main(void) {
 assert(robot_suction_get()==ROBOT_NOT_INITIALIZED);assert(robot_suction_set(1)==ROBOT_NOT_INITIALIZED);
 failed=1;assert(robot_suction_init()==ROBOT_ERROR);assert(closed==1);failed=0;
 assert(robot_suction_init()==0);assert(robot_suction_get()==0);
 assert(robot_suction_set(1)==0);assert(robot_suction_get()==1);
 assert(robot_suction_set(2)==ROBOT_INVALID_ARGUMENT);assert(robot_suction_get()==1);
 failed=1;assert(robot_suction_set(0)==ROBOT_ERROR);assert(robot_suction_get()==ROBOT_ERROR);failed=0;
 robot_suction_cleanup();assert(value==GPIOD_LINE_VALUE_INACTIVE);assert(released==1);
 robot_suction_cleanup();assert(released==1);assert(robot_suction_get()==ROBOT_NOT_INITIALIZED);
 puts("OK: GPIO13 inicia apagado, conmutacion, errores y liberacion apagada.");
}
