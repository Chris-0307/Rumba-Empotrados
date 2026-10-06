#include <robot_hw.h>
#include <stdio.h>
#include <math.h>
int main(void) {
    if (robot_init()!=ROBOT_OK) {
        fprintf(stderr,"Hardware: fallo al inicializar librobot\n");
        robot_cleanup();return 1;
    }
    int result=0;
    int traction=robot_stop(),suction=robot_suction_set(0);
    if (traction!=ROBOT_OK || suction!=ROBOT_OK) {
        fprintf(stderr,"Hardware: no se pudo confirmar orden de apagado\n");result=1;
    }
    float floor=robot_get_floor_distance(),front=robot_get_front_distance();
    if (isfinite(floor) && floor>=2 && floor<=400) printf("Hardware: suelo %.2f cm\n",floor);
    else fprintf(stderr,"Hardware: suelo sin eco valido; el servidor bloqueara movimiento\n");
    if (isfinite(front) && front>=2 && front<=400) printf("Hardware: frontal %.2f cm\n",front);
    else fprintf(stderr,"Hardware: frontal sin eco valido; automatico esperara sensores\n");
    robot_cleanup();return result;
}
