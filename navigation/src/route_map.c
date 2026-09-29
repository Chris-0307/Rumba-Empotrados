#include <stdio.h>
#include "route_map.h"

static int position_is_valid(int x, int y)
{
    return x >= 0 &&
           x < MAP_WIDTH &&
           y >= 0 &&
           y < MAP_HEIGHT;
}

static void direction_delta(
    RobotDirection direction,
    int *dx,
    int *dy
)
{
    *dx = 0;
    *dy = 0;

    switch (direction)
    {
        case DIR_NORTH:
            *dy = -1;
            break;

        case DIR_NORTHEAST:
            *dx = 1;
            *dy = -1;
            break;

        case DIR_EAST:
            *dx = 1;
            break;

        case DIR_SOUTHEAST:
            *dx = 1;
            *dy = 1;
            break;

        case DIR_SOUTH:
            *dy = 1;
            break;

        case DIR_SOUTHWEST:
            *dx = -1;
            *dy = 1;
            break;

        case DIR_WEST:
            *dx = -1;
            break;

        case DIR_NORTHWEST:
            *dx = -1;
            *dy = -1;
            break;
    }
}

void route_map_init(RouteMap *map)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            map->cells[y][x] = CELL_UNKNOWN;
        }
    }

    map->robot_x = MAP_WIDTH / 2;
    map->robot_y = MAP_HEIGHT / 2;

    map->direction = DIR_NORTH;

    map->cells[map->robot_y][map->robot_x] = CELL_VISITED;
}

void route_map_move_forward(RouteMap *map)
{
    int dx;
    int dy;
    int new_x;
    int new_y;

    direction_delta(map->direction, &dx, &dy);

    new_x = map->robot_x + dx;
    new_y = map->robot_y + dy;

    if (position_is_valid(new_x, new_y))
    {
        map->robot_x = new_x;
        map->robot_y = new_y;
        map->cells[new_y][new_x] = CELL_VISITED;
    }
}

void route_map_move_backward(RouteMap *map)
{
    int dx;
    int dy;
    int new_x;
    int new_y;

    direction_delta(map->direction, &dx, &dy);

    new_x = map->robot_x - dx;
    new_y = map->robot_y - dy;

    if (position_is_valid(new_x, new_y))
    {
        map->robot_x = new_x;
        map->robot_y = new_y;
        map->cells[new_y][new_x] = CELL_VISITED;
    }
}

void route_map_turn_left(RouteMap *map)
{
    map->direction =
        (RobotDirection)((map->direction + 7) % 8);
}

void route_map_turn_right(RouteMap *map)
{
    map->direction =
        (RobotDirection)((map->direction + 1) % 8);
}

void route_map_mark_obstacle_front(RouteMap *map)
{
    int dx;
    int dy;
    int obstacle_x;
    int obstacle_y;

    direction_delta(map->direction, &dx, &dy);

    obstacle_x = map->robot_x + dx;
    obstacle_y = map->robot_y + dy;

    if (position_is_valid(obstacle_x, obstacle_y))
    {
        map->cells[obstacle_y][obstacle_x] = CELL_OBSTACLE;
    }
}

void route_map_print(const RouteMap *map)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            if (x == map->robot_x && y == map->robot_y)
            {
                printf("R ");
            }
            else
            {
                switch (map->cells[y][x])
                {
                    case CELL_VISITED:
                        printf(". ");
                        break;

                    case CELL_OBSTACLE:
                        printf("X ");
                        break;

                    default:
                        printf("? ");
                        break;
                }
            }
        }

        printf("\n");
    }
}
