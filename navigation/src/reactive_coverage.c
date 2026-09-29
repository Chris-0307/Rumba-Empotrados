#include "reactive_coverage.h"

void coverage_init(CoverageState *state)
{
    state->cycles = 0;
    state->obstacles_detected = 0;
    state->turn_left_next = 1;
    state->searching_direction = 0;
    state->current_turn_left = 1;
}

AvoidancePlan coverage_step(
    CoverageState *state,
    float front_distance,
    float min_distance
)
{
    int blocked;

    state->cycles++;

    blocked = front_distance >= 0.0f &&
              front_distance < min_distance;

    if (blocked)
    {
        if (!state->searching_direction)
        {
            /* Nuevo obstaculo: fija un lado de busqueda y alterna el lado
             * que se utilizara la proxima vez que aparezca otro obstaculo.
             */
            state->obstacles_detected++;
            state->searching_direction = 1;
            state->current_turn_left = state->turn_left_next;
            state->turn_left_next = !state->turn_left_next;
        }
    }
    else
    {
        /* La nueva direccion ya quedo libre. */
        state->searching_direction = 0;
    }

    return obstacle_avoidance_plan(
        front_distance,
        min_distance,
        state->current_turn_left
    );
}
