#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "robot_hw.h"
/* Solo sensores: no solicitar motores, LEDs o audio durante la calibracion. */
#include "../src/sensor_backend.h"
static volatile sig_atomic_t stopped;
static void stop_test(int sig) { (void)sig; stopped=1; }
int main(void) {
    signal(SIGINT,stop_test); signal(SIGTERM,stop_test);
    if (sensor_backend_init()!=ROBOT_OK) { fputs("No se pudieron solicitar GPIO de sensores\n",stderr); return 1; }
    puts("Distancias en cm. -1 significa error/sin eco. Ctrl+C termina.");
    while (!stopped) {
        float floor=robot_get_floor_distance();
        float front=robot_get_front_distance();
        printf("suelo=%.2f frontal=%.2f\n",floor,front); fflush(stdout);
        struct timespec pause={0,20000000}; nanosleep(&pause,NULL);
    }
    sensor_backend_cleanup(); return 0;
}
