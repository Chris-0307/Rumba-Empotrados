#include "robot_adapter.h"
#include "control_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ROBOT_SIMULATE=1 permite probar la interfaz SIN activar el hardware. */
static int simulation(void) {
    const char *v = getenv("ROBOT_SIMULATE");
    return v && strcmp(v, "1") == 0;
}

int robot_adapter_sensors(double *front, double *left, double *right) {
    if (!simulation()) return -1;
    *front = 48.0; *left = 72.0; *right = 35.0;
    return 0;
}
int robot_adapter_init(void) { return control_init(); }
void robot_adapter_cleanup(void) { control_cleanup(); }
const char *robot_adapter_get_mode(void) { return control_get_mode(); }
int robot_adapter_move(const char *direction, int speed) {
    return control_move(direction, speed);
}
int robot_adapter_mode(const char *mode) {
    return control_set_mode(mode);
}
int robot_adapter_audio(const char *action, int song_id, int volume) {
    if (!simulation()) return -1;
    fprintf(stderr, "SIMULACION: audio=%s song=%d volume=%d (sin reproductor)\n", action, song_id, volume);
    return 0;
}
int robot_adapter_map(char *json, unsigned long capacity) {
    if (!simulation()) return -1;
    int n = snprintf(json, capacity, "{\"width\":4,\"height\":3,\"robot\":{\"x\":1,\"y\":1,\"direction\":\"north\"},\"cells\":[[\"unknown\",\"unknown\",\"unknown\",\"unknown\"],[\"visited\",\"visited\",\"obstacle\",\"unknown\"],[\"unknown\",\"unknown\",\"unknown\",\"unknown\"]]}");
    return n >= 0 && (unsigned long)n < capacity ? 0 : -1;
}
