#include "robot_hw.h"


int robot_audio_play(const char *filename)
{
    if (filename == 0)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}


int robot_audio_pause(void)
{
    return ROBOT_OK;
}


int robot_audio_stop(void)
{
    return ROBOT_OK;
}


int robot_audio_set_volume(int volume)
{
    if (volume < 0 || volume > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    return ROBOT_OK;
}