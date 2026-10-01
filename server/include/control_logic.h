#ifndef CONTROL_LOGIC_H
#define CONTROL_LOGIC_H

/* El servidor actual atiende peticiones en serie. Agregar sincronizacion
 * antes de usar estas funciones desde varios hilos. */
enum { CONTROL_OK = 0, CONTROL_ERROR = -1,
       CONTROL_INVALID_ARGUMENT = -2, CONTROL_MANUAL_REQUIRED = -3 };
int control_init(void);
void control_cleanup(void);
int control_set_mode(const char *mode);
const char *control_get_mode(void);
int control_move(const char *direction, int speed);
int control_stop(void);
#endif
