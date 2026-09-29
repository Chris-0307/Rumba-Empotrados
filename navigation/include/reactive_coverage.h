#ifndef REACTIVE_COVERAGE_H
#define REACTIVE_COVERAGE_H

#include "obstacle_avoidance.h"

typedef struct
{
    unsigned int cycles;
    unsigned int obstacles_detected;
} CoverageState;

void coverage_init(CoverageState *state);

AvoidancePlan coverage_step(
    CoverageState *state,
    float front_distance,
    float left_distance,
    float right_distance,
    float min_distance
);

#endif
