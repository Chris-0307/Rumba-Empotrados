#include <stdio.h>
#include "reactive_coverage.h"

typedef struct
{
    float front;
    float left;
    float right;
} SensorSample;

int main(void)
{
    const float min_distance = 25.0f;

    SensorSample samples[] =
    {
        {100.0f, 100.0f, 100.0f},
        {80.0f,  70.0f,  90.0f},
        {15.0f,  80.0f,  30.0f},
        {90.0f,  60.0f,  80.0f},
        {70.0f,  10.0f,  80.0f},
        {100.0f, 100.0f, 100.0f}
    };

    int sample_count = sizeof(samples) / sizeof(samples[0]);

    CoverageState state;
    coverage_init(&state);

    for (int i = 0; i < sample_count; i++)
    {
        printf("\nCiclo %d\n", i + 1);

        printf(
            "Sensores: frente=%.1f cm, izquierda=%.1f cm, derecha=%.1f cm\n",
            samples[i].front,
            samples[i].left,
            samples[i].right
        );

        AvoidancePlan plan = coverage_step(
            &state,
            samples[i].front,
            samples[i].left,
            samples[i].right,
            min_distance
        );

        for (int j = 0; j < plan.count; j++)
        {
            printf(
                "  -> %s\n",
                robot_action_to_string(plan.actions[j])
            );
        }
    }

    printf("\nResumen\n");
    printf("Ciclos ejecutados: %u\n", state.cycles);
    printf("Obstaculos frontales detectados: %u\n",
           state.obstacles_detected);

    return 0;
}
