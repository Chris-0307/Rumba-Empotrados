#include <stdio.h>

#include "reactive_coverage.h"
#include "route_map.h"

typedef struct
{
    float front;
} SensorSample;

static void execute_action(
    RouteMap *map,
    RobotAction action
)
{
    switch (action)
    {
        case ACTION_FORWARD:
            route_map_move_forward(map);
            break;

        case ACTION_BACKWARD:
            route_map_move_backward(map);
            break;

        case ACTION_TURN_LEFT:
            route_map_turn_left(map);
            break;

        case ACTION_TURN_RIGHT:
            route_map_turn_right(map);
            break;

        case ACTION_STOP:
            break;
    }
}

int main(void)
{
    const float min_distance = 25.0f;

    SensorSample samples[] =
    {
        {100.0f},
        {90.0f},
        {15.0f},
        {14.0f},
        {100.0f},
        {100.0f},
        {15.0f},
        {100.0f}
    };

    int sample_count =
        sizeof(samples) / sizeof(samples[0]);

    CoverageState coverage;
    RouteMap map;

    coverage_init(&coverage);
    route_map_init(&map);

    printf("Mapa inicial:\n");
    route_map_print(&map);

    for (int i = 0; i < sample_count; i++)
    {
        printf("\nCiclo %d\n", i + 1);
        printf("Sensor frontal: %.1f cm\n", samples[i].front);

        if (samples[i].front >= 0.0f &&
            samples[i].front < min_distance)
        {
            route_map_mark_obstacle_front(&map);
        }

        AvoidancePlan plan = coverage_step(
            &coverage,
            samples[i].front,
            min_distance
        );

        for (int j = 0; j < plan.count; j++)
        {
            printf(
                "  -> %s\n",
                robot_action_to_string(plan.actions[j])
            );

            execute_action(
                &map,
                plan.actions[j]
            );
        }

        printf("\nMapa:\n");
        route_map_print(&map);
    }

    printf("\nResumen\n");
    printf("Ciclos ejecutados: %u\n", coverage.cycles);
    printf(
        "Obstaculos detectados: %u\n",
        coverage.obstacles_detected
    );

    return 0;
}
