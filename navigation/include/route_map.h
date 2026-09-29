#ifndef ROUTE_MAP_H
#define ROUTE_MAP_H

#define MAP_WIDTH 11
#define MAP_HEIGHT 11

typedef enum
{
    CELL_UNKNOWN = 0,
    CELL_VISITED,
    CELL_OBSTACLE
} MapCell;

/*
 * Ocho orientaciones para representar los giros cortos de aproximadamente
 * 45 grados usados con el unico sensor frontal HC-SR04.
 */
typedef enum
{
    DIR_NORTH = 0,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST
} RobotDirection;

typedef struct
{
    MapCell cells[MAP_HEIGHT][MAP_WIDTH];

    int robot_x;
    int robot_y;

    RobotDirection direction;

} RouteMap;

void route_map_init(RouteMap *map);

void route_map_move_forward(RouteMap *map);

void route_map_move_backward(RouteMap *map);

/* Cada llamada representa un giro estimado de 45 grados. */
void route_map_turn_left(RouteMap *map);

void route_map_turn_right(RouteMap *map);

void route_map_mark_obstacle_front(RouteMap *map);

void route_map_print(const RouteMap *map);

#endif
