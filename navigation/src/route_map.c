#include <stdio.h>
#include "route_map.h"

static int position_is_valid(int x, int y)
{
    return x >= 0 &&
           x < MAP_WIDTH &&
           y >= 0 &&
           y < MAP_HEIGHT;
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
    int new_x = map->robot_x;
    int new_y = map->robot_y;

    switch (map->direction)
    {
        case DIR_NORTH:
            new_y--;
            break;

        case DIR_EAST:
            new_x++;
            break;

        case DIR_SOUTH:
            new_y++;
            break;

        case DIR_WEST:
            new_x--;
            break;
    }

    if (position_is_valid(new_x, new_y))
    {
        map->robot_x = new_x;
        map->robot_y = new_y;

        map->cells[new_y][new_x] = CELL_VISITED;
    }
}

void route_map_move_backward(RouteMap *map)
{
    int new_x = map->robot_x;
    int new_y = map->robot_y;

    switch (map->direction)
    {
        case DIR_NORTH:
            new_y++;
            break;

        case DIR_EAST:
            new_x--;
            break;

        case DIR_SOUTH:
            new_y--;
            break;

        case DIR_WEST:
            new_x++;
            break;
    }

    if (position_is_valid(new_x, new_y))
    {
        map->robot_x = new_x;
        map->robot_y = new_y;

        map->cells[new_y][new_x] = CELL_VISITED;
    }
}

void route_map_turn_left(RouteMap *map)
{
    if (map->direction == DIR_NORTH)
    {
        map->direction = DIR_WEST;
    }
    else
    {
        map->direction--;
    }
}

void route_map_turn_right(RouteMap *map)
{
    map->direction = (map->direction + 1) % 4;
}

void route_map_mark_obstacle_front(RouteMap *map)
{
    int obstacle_x = map->robot_x;
    int obstacle_y = map->robot_y;

    switch (map->direction)
    {
        case DIR_NORTH:
            obstacle_y--;
            break;

        case DIR_EAST:
            obstacle_x++;
            break;

        case DIR_SOUTH:
            obstacle_y++;
            break;

        case DIR_WEST:
            obstacle_x--;
            break;
    }

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
