#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "robot_hw.h"
#include "audio.h"
#include "hardware_config.h"


static pid_t audio_pid = -1;
static int audio_pipe = -1;
static int audio_initialized = 0;
static int audio_paused = 0;
static int audio_volume = 75;
static char current_file[PATH_MAX];

static struct sigaction old_sigpipe_action;
static int sigpipe_action_saved = 0;


static void reset_player_state(void)
{
    if (audio_pipe >= 0)
    {
        close(audio_pipe);
        audio_pipe = -1;
    }

    audio_pid = -1;
    audio_paused = 0;
    current_file[0] = '\0';
}


static int player_is_running(void)
{
    int status;
    pid_t result;

    if (audio_pid <= 0)
    {
        return 0;
    }

    result = waitpid(audio_pid, &status, WNOHANG);

    if (result == 0)
    {
        return 1;
    }

    reset_player_state();

    return 0;
}


static int write_all(int fd, const char *buffer, size_t length)
{
    size_t written = 0;

    while (written < length)
    {
        ssize_t result = write(fd, buffer + written, length - written);

        if (result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return ROBOT_ERROR;
        }

        written += (size_t)result;
    }

    return ROBOT_OK;
}


static int send_command(const char *command)
{
    size_t length;

    if (command == NULL || !player_is_running() || audio_pipe < 0)
    {
        return ROBOT_ERROR;
    }

    length = strlen(command);

    if (write_all(audio_pipe, command, length) != ROBOT_OK ||
        write_all(audio_pipe, "\n", 1) != ROBOT_OK)
    {
        reset_player_state();
        return ROBOT_ERROR;
    }

    return ROBOT_OK;
}


static int start_player(void)
{
    int pipe_fd[2];
    pid_t pid;

    if (player_is_running())
    {
        return ROBOT_OK;
    }

    if (access(AUDIO_PLAYER_PATH, X_OK) != 0)
    {
        return ROBOT_ERROR;
    }

    if (pipe(pipe_fd) != 0)
    {
        return ROBOT_ERROR;
    }

    pid = fork();

    if (pid < 0)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return ROBOT_ERROR;
    }

    if (pid == 0)
    {
        int dev_null;

        close(pipe_fd[1]);

        if (dup2(pipe_fd[0], STDIN_FILENO) < 0)
        {
            _exit(127);
        }

        close(pipe_fd[0]);

        dev_null = open("/dev/null", O_WRONLY);

        if (dev_null >= 0)
        {
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);

            if (dev_null > STDERR_FILENO)
            {
                close(dev_null);
            }
        }

        execl(
            AUDIO_PLAYER_PATH,
            AUDIO_PLAYER_PATH,
            "-R",
            "--quiet",
            (char *)NULL
        );

        _exit(127);
    }

    close(pipe_fd[0]);

    audio_pid = pid;
    audio_pipe = pipe_fd[1];
    audio_paused = 0;
    current_file[0] = '\0';

    return ROBOT_OK;
}


static int valid_audio_filename(const char *filename)
{
    size_t length;
    const char *extension;

    if (filename == NULL || filename[0] == '\0')
    {
        return 0;
    }

    if (strchr(filename, '\n') != NULL || strchr(filename, '\r') != NULL)
    {
        return 0;
    }

    length = strlen(filename);

    if (length >= sizeof(current_file))
    {
        return 0;
    }

    extension = strrchr(filename, '.');

    if (extension == NULL || strlen(extension) != 4)
    {
        return 0;
    }

    if (tolower((unsigned char)extension[1]) != 'm' ||
        tolower((unsigned char)extension[2]) != 'p' ||
        extension[3] != '3')
    {
        return 0;
    }

    if (access(filename, R_OK) != 0)
    {
        return 0;
    }

    return 1;
}


int robot_audio_init(void)
{
    struct sigaction ignore_action;

    if (audio_initialized)
    {
        return ROBOT_OK;
    }

    memset(&ignore_action, 0, sizeof(ignore_action));
    ignore_action.sa_handler = SIG_IGN;
    sigemptyset(&ignore_action.sa_mask);

    if (sigaction(SIGPIPE, &ignore_action, &old_sigpipe_action) == 0)
    {
        sigpipe_action_saved = 1;
    }

    audio_pid = -1;
    audio_pipe = -1;
    audio_paused = 0;
    audio_volume = AUDIO_DEFAULT_VOLUME;
    current_file[0] = '\0';
    audio_initialized = 1;

    return ROBOT_OK;
}


void robot_audio_cleanup(void)
{
    if (player_is_running())
    {
        kill(audio_pid, SIGTERM);
        waitpid(audio_pid, NULL, 0);
    }

    reset_player_state();

    if (sigpipe_action_saved)
    {
        sigaction(SIGPIPE, &old_sigpipe_action, NULL);
        sigpipe_action_saved = 0;
    }

    audio_initialized = 0;
}


int robot_audio_play(const char *filename)
{
    char command[PATH_MAX + 16];

    if (!valid_audio_filename(filename))
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    if (!audio_initialized)
    {
        if (robot_audio_init() != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }
    }

    if (start_player() != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    if (audio_paused && strcmp(current_file, filename) == 0)
    {
        if (send_command("PAUSE") != ROBOT_OK)
        {
            return ROBOT_ERROR;
        }

        audio_paused = 0;
        return ROBOT_OK;
    }

    snprintf(command, sizeof(command), "VOLUME %d", audio_volume);

    if (send_command(command) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    snprintf(command, sizeof(command), "LOAD %s", filename);

    if (send_command(command) != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    strncpy(current_file, filename, sizeof(current_file) - 1);
    current_file[sizeof(current_file) - 1] = '\0';
    audio_paused = 0;

    return ROBOT_OK;
}


int robot_audio_pause(void)
{
    if (!audio_initialized || current_file[0] == '\0' || !player_is_running())
    {
        return ROBOT_ERROR;
    }

    if (audio_paused)
    {
        return ROBOT_OK;
    }

    if (send_command("PAUSE") != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    audio_paused = 1;

    return ROBOT_OK;
}


int robot_audio_stop(void)
{
    if (!audio_initialized)
    {
        return ROBOT_OK;
    }

    if (!player_is_running() || current_file[0] == '\0')
    {
        current_file[0] = '\0';
        audio_paused = 0;
        return ROBOT_OK;
    }

    if (send_command("STOP") != ROBOT_OK)
    {
        return ROBOT_ERROR;
    }

    current_file[0] = '\0';
    audio_paused = 0;

    return ROBOT_OK;
}


int robot_audio_set_volume(int volume)
{
    char command[32];

    if (volume < 0 || volume > 100)
    {
        return ROBOT_INVALID_ARGUMENT;
    }

    audio_volume = volume;

    if (!audio_initialized || !player_is_running())
    {
        return ROBOT_OK;
    }

    snprintf(command, sizeof(command), "VOLUME %d", volume);

    return send_command(command);
}
