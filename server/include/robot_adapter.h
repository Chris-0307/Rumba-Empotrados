#ifndef ROBOT_ADAPTER_H
#define ROBOT_ADAPTER_H

/* Ciclo de vida y consulta de la unica fuente del modo. */
int robot_adapter_init(void);
void robot_adapter_cleanup(void);
const char *robot_adapter_get_mode(void);
/* Movimiento: -3 indica que se requiere modo manual. */
int robot_adapter_sensors(double *front, double *left, double *right);
int robot_adapter_move(const char *direction, int speed);
int robot_adapter_mode(const char *mode);
int robot_adapter_audio(const char *action, int song_id, int volume);
int robot_adapter_map(char *json, unsigned long capacity);

#endif
