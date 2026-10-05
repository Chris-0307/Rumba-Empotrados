#ifndef CONTROL_LOGIC_H
#define CONTROL_LOGIC_H

/* El adaptador serializa estas funciones con controller_lock.
 * No llamarlas directamente desde otros hilos. */
enum { CONTROL_OK = 0, CONTROL_ERROR = -1,
       CONTROL_INVALID_ARGUMENT = -2, CONTROL_MANUAL_REQUIRED = -3 };
int control_init(void);
void control_cleanup(void);
int control_set_mode(const char *mode);
const char *control_get_mode(void);
int control_move(const char *direction, int speed);
int control_move_pair(const char *direction,int left,int right,int automatic);
int control_stop(void);
/* Solo modo automatico; el adaptador verifica sensores y serializa acceso. */
int control_auto_move(const char *direction, int speed);
#endif
