#include <stdio.h>
#include "obstacle_avoidance.h"

static void run_test(
    const char *name,
    float front,
    float left,
    float right
)
{
    const float min_distance = 25.0f;

    printf(
        "Sensores: frente=%.1f cm, izquierda=%.1f cm, derecha=%.1f cm\n",
        front,
        left,
        right
    );

    AvoidancePlan plan = obstacle_avoidance_plan(
        front,
        left,
        right,
        min_distance
    );

    printf("Acciones:\n");

    for (int i = 0; i < plan.count; i++)
    {
        printf("  -> %s\n", robot_action_to_string(plan.actions[i]));
    }
}

int main(void)
{
    run_test(
        "Camino libre",
        100.0f,
        100.0f,
        100.0f
    );

    run_test(
        "Obstaculo frontal, izquierda libre",
        15.0f,
        80.0f,
        30.0f
    );

    run_test(
        "Obstaculo frontal, derecha libre",
        15.0f,
        20.0f,
        80.0f
    );

    run_test(
        "Obstaculo izquierdo",
        100.0f,
        10.0f,
        80.0f
    );

    run_test(
        "Obstaculo derecho",
        100.0f,
        80.0f,
        10.0f
    );

    return 0;
}