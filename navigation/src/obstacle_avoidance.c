#include "obstacle_avoidance.h"

AvoidancePlan obstacle_avoidance_plan(
    float front_distance,
    float left_distance,
    float right_distance,
    float min_distance
)
{
    AvoidancePlan plan;
    plan.count = 0;

// Si hay obstaculo al frente, hace stop, retrocede y gira hacie el lado con mas espacio.
if (front_distance < min_distance)
{
    plan.actions[0] = ACTION_STOP;
    plan.actions[1] = ACTION_BACKWARD;
    
    // Podemos cambiarlo para que gire siempre hacia la izquierda o derecha hasta que no haya obstaculo, mas sencillo pero menos inteligente.
        if (left_distance > right_distance)
        {
            plan.actions[2] = ACTION_TURN_LEFT;
        }
        else
        {
            plan.actions[2] = ACTION_TURN_RIGHT;
        }

        plan.count = 3;
    }


    // Si el obstaculo esta al lado izquierdo, giramos hacia la derecha.
    else if (left_distance < min_distance)
    {
        plan.actions[0] = ACTION_TURN_RIGHT;
        plan.count = 1;
    }

    // Si el obstaculo esta al lado derecho, giramos hacia la izquierda.
    else if (right_distance < min_distance)
    {
        plan.actions[0] = ACTION_TURN_LEFT;
        plan.count = 1;
    }

    else
    {
        plan.actions[0] = ACTION_FORWARD;
        plan.count = 1;
    }

    return plan;
}

const char *robot_action_to_string(RobotAction action)
{
    switch (action)
    {
        case ACTION_FORWARD:
            return "AVANZAR";

        case ACTION_STOP:
            return "DETENER";

        case ACTION_BACKWARD:
            return "RETROCEDER";

        case ACTION_TURN_LEFT:
            return "GIRAR IZQUIERDA";

        case ACTION_TURN_RIGHT:
            return "GIRAR DERECHA";

        default:
            return "DESCONOCIDA";
    }
}