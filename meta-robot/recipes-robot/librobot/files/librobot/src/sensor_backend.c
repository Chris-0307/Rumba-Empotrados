#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <gpiod.h>
#include "robot_hw.h"
#include "hardware_config.h"
#include "sensor_backend.h"

typedef struct {
    unsigned int trigger, echo;
    struct gpiod_line_request *trig_req, *echo_req;
    struct gpiod_edge_event_buffer *events;
} Ultrasonic;
static Ultrasonic sensors[2] = {
    {.trigger=GPIO_HCSR04_TRIGGER, .echo=GPIO_HCSR04_ECHO},
    {.trigger=GPIO_HCSR04_FLOOR_TRIGGER, .echo=GPIO_HCSR04_FLOOR_ECHO}
};
static struct gpiod_chip *chip;
static pthread_mutex_t sensor_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t last_trigger_ns;

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint64_t)ts.tv_sec*1000000000ULL+(uint64_t)ts.tv_nsec;
}
static void pause_ns(uint64_t ns) {
    struct timespec delay={(time_t)(ns/1000000000ULL),(long)(ns%1000000000ULL)};
    while (nanosleep(&delay,&delay) && errno==EINTR) {}
}
static struct gpiod_line_request *request_line(unsigned int offset, int output) {
    struct gpiod_line_settings *settings=gpiod_line_settings_new();
    struct gpiod_line_config *config=gpiod_line_config_new();
    struct gpiod_request_config *request=gpiod_request_config_new();
    struct gpiod_line_request *result=NULL;
    if (!settings || !config || !request) goto done;
    if (gpiod_line_settings_set_direction(settings,output?GPIOD_LINE_DIRECTION_OUTPUT:GPIOD_LINE_DIRECTION_INPUT)) goto done;
    if (output) {
        if (gpiod_line_settings_set_output_value(settings,GPIOD_LINE_VALUE_INACTIVE)) goto done;
    } else {
        if (gpiod_line_settings_set_edge_detection(settings,GPIOD_LINE_EDGE_BOTH) ||
            gpiod_line_settings_set_event_clock(settings,GPIOD_LINE_CLOCK_MONOTONIC)) goto done;
    }
    if (gpiod_line_config_add_line_settings(config,&offset,1,settings)) goto done;
    gpiod_request_config_set_consumer(request,"librobot-ultrasonic");
    result=gpiod_chip_request_lines(chip,request,config);
done:
    if (settings) gpiod_line_settings_free(settings);
    if (config) gpiod_line_config_free(config);
    if (request) gpiod_request_config_free(request);
    return result;
}
static void cleanup_locked(void) {
    for (unsigned int i=0;i<2;i++) {
        Ultrasonic *s=&sensors[i];
        if (s->trig_req) {
            (void)gpiod_line_request_set_value(s->trig_req,s->trigger,GPIOD_LINE_VALUE_INACTIVE);
            gpiod_line_request_release(s->trig_req); s->trig_req=NULL;
        }
        if (s->echo_req) { gpiod_line_request_release(s->echo_req); s->echo_req=NULL; }
        if (s->events) { gpiod_edge_event_buffer_free(s->events); s->events=NULL; }
    }
    if (chip) { gpiod_chip_close(chip); chip=NULL; }
    last_trigger_ns=0;
}
int sensor_backend_init(void) {
    int result=ROBOT_ERROR;
    pthread_mutex_lock(&sensor_lock);
    if (chip) { result=ROBOT_OK; goto done; }
    chip=gpiod_chip_open(ROBOT_GPIO_CHIP);
    if (!chip) goto failed;
    for (unsigned int i=0;i<2;i++) {
        Ultrasonic *s=&sensors[i];
        s->trig_req=request_line(s->trigger,1);
        s->echo_req=request_line(s->echo,0);
        s->events=gpiod_edge_event_buffer_new(16);
        if (!s->trig_req || !s->echo_req || !s->events) goto failed;
    }
    pause_ns(2000000ULL); result=ROBOT_OK; goto done;
failed:
    cleanup_locked();
done:
    pthread_mutex_unlock(&sensor_lock); return result;
}
void sensor_backend_cleanup(void) {
    pthread_mutex_lock(&sensor_lock); cleanup_locked(); pthread_mutex_unlock(&sensor_lock);
}
static float measure_locked(Ultrasonic *s) {
    if (!s->trig_req || !s->echo_req) return ROBOT_SENSOR_ERROR;
    uint64_t now=now_ns();
    if (!now) return ROBOT_SENSOR_ERROR;
    if (last_trigger_ns && now-last_trigger_ns<HCSR04_TRIGGER_GAP_NS)
        pause_ns(HCSR04_TRIGGER_GAP_NS-(now-last_trigger_ns));
    /* Vaciar eventos anteriores, sin espera indefinida. */
    for (int i=0;i<32;i++) {
        int pending=gpiod_line_request_wait_edge_events(s->echo_req,0);
        if (pending<0) return ROBOT_SENSOR_ERROR;
        if (!pending) break;
        if (gpiod_line_request_read_edge_events(s->echo_req,s->events,16)<0) return ROBOT_SENSOR_ERROR;
        if (i==31) return ROBOT_SENSOR_ERROR;
    }
    if (gpiod_line_request_get_value(s->echo_req,s->echo)!=GPIOD_LINE_VALUE_INACTIVE)
        return ROBOT_SENSOR_ERROR;
    uint64_t started=now_ns();
    if (!started) return ROBOT_SENSOR_ERROR;
    /* Registrar incluso intentos fallidos para conservar la separacion. */
    last_trigger_ns=started;
    if (gpiod_line_request_set_value(s->trig_req,s->trigger,GPIOD_LINE_VALUE_ACTIVE)) return ROBOT_SENSOR_ERROR;
    pause_ns(10000ULL);
    int lowered=gpiod_line_request_set_value(s->trig_req,s->trigger,GPIOD_LINE_VALUE_INACTIVE);
    /* Separar desde el final del pulso: nanosleep puede demorarse en Linux. */
    last_trigger_ns=now_ns();
    if (lowered || !last_trigger_ns) return ROBOT_SENSOR_ERROR;
    uint64_t deadline=last_trigger_ns+(uint64_t)HCSR04_TIMEOUT_US*1000ULL;
    uint64_t rise=0;
    for (;;) {
        now=now_ns();
        if (!now || now>=deadline) return ROBOT_SENSOR_ERROR;
        if (gpiod_line_request_wait_edge_events(s->echo_req,(int64_t)(deadline-now))<=0)
            return ROBOT_SENSOR_ERROR;
        int count=gpiod_line_request_read_edge_events(s->echo_req,s->events,16);
        if (count<0) return ROBOT_SENSOR_ERROR;
        for (int i=0;i<count;i++) {
            struct gpiod_edge_event *event=gpiod_edge_event_buffer_get_event(s->events,(unsigned long)i);
            uint64_t timestamp=gpiod_edge_event_get_timestamp_ns(event);
            if (timestamp<started || timestamp>deadline) continue;
            enum gpiod_edge_event_type type=gpiod_edge_event_get_event_type(event);
            if (type==GPIOD_EDGE_EVENT_RISING_EDGE) rise=timestamp;
            else if (type==GPIOD_EDGE_EVENT_FALLING_EDGE && rise && timestamp>rise) {
                float cm=(float)(timestamp-rise)*0.00001715f;
                return cm>=HCSR04_MIN_DISTANCE_CM && cm<=HCSR04_MAX_DISTANCE_CM ? cm : ROBOT_SENSOR_ERROR;
            }
        }
    }
}
static float measure(unsigned int index) {
    pthread_mutex_lock(&sensor_lock);
    float cm=measure_locked(&sensors[index]);
    pthread_mutex_unlock(&sensor_lock); return cm;
}
float sensor_backend_get_distance(void) { return measure(0); }
float sensor_backend_get_floor_distance(void) { return measure(1); }
