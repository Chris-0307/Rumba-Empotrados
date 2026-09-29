#ifndef REACTIVE_COVERAGE_H
#define REACTIVE_COVERAGE_H

#include "obstacle_avoidance.h"

typedef struct
{
    unsigned int cycles;
    unsigned int obstacles_detected;

    /* Direccion preferida para el proximo obstaculo nuevo. */
    int turn_left_next;

    /* Mientras siga bloqueado, mantiene la misma direccion de busqueda. */
    int searching_direction;
    int current_turn_left;
} CoverageState;

void coverage_init(CoverageState *state);

AvoidancePlan coverage_step(
    CoverageState *state,
    float front_distance,
    float min_distance
);

#endif
