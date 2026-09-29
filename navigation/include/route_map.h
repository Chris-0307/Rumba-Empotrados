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

typedef enum
{
    DIR_NORTH = 0,
    DIR_EAST,
    DIR_SOUTH,
    DIR_WEST
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

void route_map_turn_left(RouteMap *map);

void route_map_turn_right(RouteMap *map);

void route_map_mark_obstacle_front(RouteMap *map);

void route_map_print(const RouteMap *map);

#endif
