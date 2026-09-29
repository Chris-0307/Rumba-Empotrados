#include <stdio.h>
#include "route_map.h"

int main(void)
{
    RouteMap map;

    route_map_init(&map);

    printf("Mapa inicial:\n");
    route_map_print(&map);

    printf("\nEl robot avanza dos celdas:\n");

    route_map_move_forward(&map);
    route_map_move_forward(&map);

    route_map_print(&map);

    printf("\nSe detecta un obstaculo al frente:\n");

    route_map_mark_obstacle_front(&map);

    route_map_print(&map);

    printf("\nEl robot gira a la derecha y avanza:\n");

    route_map_turn_right(&map);
    route_map_move_forward(&map);

    route_map_print(&map);

    return 0;
}
