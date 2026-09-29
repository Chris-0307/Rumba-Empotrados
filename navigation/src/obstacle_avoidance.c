#include "obstacle_avoidance.h"

AvoidancePlan obstacle_avoidance_plan(
    float front_distance,
    float min_distance,
    int turn_left
)
{
    AvoidancePlan plan;
    plan.count = 0;

    /*
     * Con un solo HC-SR04 frontal no se conoce que lado esta mas libre.
     * Si hay un obstaculo se detiene y realiza un giro corto. En el
     * siguiente ciclo se vuelve a medir para comprobar la nueva direccion.
     */
    if (front_distance >= 0.0f && front_distance < min_distance)
    {
        plan.actions[0] = ACTION_STOP;
        plan.actions[1] = turn_left ? ACTION_TURN_LEFT : ACTION_TURN_RIGHT;
        plan.count = 2;
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
