#ifndef MOTOR_BACKEND_H
#define MOTOR_BACKEND_H


typedef enum
{
    MOTOR_LEFT = 0,
    MOTOR_RIGHT

} MotorId;


int motor_backend_init(void);

void motor_backend_cleanup(void);

int motor_backend_set_speed(MotorId motor, int speed);


#endif