#include "reactive_coverage.h"

void coverage_init(CoverageState *state)
{
    state->cycles = 0;
    state->obstacles_detected = 0;
}

AvoidancePlan coverage_step(
    CoverageState *state,
    float front_distance,
    float left_distance,
    float right_distance,
    float min_distance
)
{
    state->cycles++;

    if (front_distance < min_distance)
    {
        state->obstacles_detected++;
    }

    return obstacle_avoidance_plan(
        front_distance,
        left_distance,
        right_distance,
        min_distance
    );
}
