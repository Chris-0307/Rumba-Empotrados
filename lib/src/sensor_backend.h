#ifndef SENSOR_BACKEND_H
#define SENSOR_BACKEND_H

int sensor_backend_init(void);

void sensor_backend_cleanup(void);

float sensor_backend_get_distance(void);

#endif
