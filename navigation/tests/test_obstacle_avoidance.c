#include <stdio.h>
#include "obstacle_avoidance.h"

static void run_test(
    const char *name,
    float front,
    int turn_left
)
{
    const float min_distance = 25.0f;

    printf("\n%s\n", name);
    printf("Sensor frontal: %.1f cm\n", front);

    AvoidancePlan plan = obstacle_avoidance_plan(
        front,
        min_distance,
        turn_left
    );

    printf("Acciones:\n");

    for (int i = 0; i < plan.count; i++)
    {
        printf("  -> %s\n", robot_action_to_string(plan.actions[i]));
    }
}

int main(void)
{
    run_test("Camino libre", 100.0f, 1);
    run_test("Obstaculo frontal, giro izquierdo", 15.0f, 1);
    run_test("Obstaculo frontal, giro derecho", 15.0f, 0);

    return 0;
}
