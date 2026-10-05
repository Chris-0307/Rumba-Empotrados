#ifndef MAP_ESTIMATE_H
#define MAP_ESTIMATE_H
void map_estimate_init(void);
void map_estimate_advance(double seconds);
void map_estimate_motor_speeds(int left,int right);
void map_estimate_motion(const char *direction, int speed);
void map_estimate_obstacle(double distance_cm);
int map_estimate_json(char *json, unsigned long capacity);
#endif
