#ifndef OBSTACLE_AVOIDANCE_H
#define OBSTACLE_AVOIDANCE_H

#define MAX_AVOIDANCE_ACTIONS 2

typedef enum
{
    ACTION_FORWARD,
    ACTION_STOP,
    ACTION_BACKWARD,
    ACTION_TURN_LEFT,
    ACTION_TURN_RIGHT
} RobotAction;

typedef struct
{
    RobotAction actions[MAX_AVOIDANCE_ACTIONS];
    int count;
} AvoidancePlan;

AvoidancePlan obstacle_avoidance_plan(
    float front_distance,
    float min_distance,
    int turn_left
);

const char *robot_action_to_string(RobotAction action);

#endif
