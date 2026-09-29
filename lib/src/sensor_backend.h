#ifndef SENSOR_BACKEND_H
#define SENSOR_BACKEND_H


typedef enum
{
    SENSOR_FRONT = 0,
    SENSOR_LEFT,
    SENSOR_RIGHT

} SensorId;


int sensor_backend_init(void);

void sensor_backend_cleanup(void);

float sensor_backend_get_distance(SensorId sensor);


#endif