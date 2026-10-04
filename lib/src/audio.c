#define _POSIX_C_SOURCE 200809L
#include "robot_hw.h"
#include "audio.h"
#include "hardware_config.h"
#ifndef ROBOT_AUDIO_PLAYER_PATH
#define ROBOT_AUDIO_PLAYER_PATH AUDIO_PLAYER_PATH
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static pid_t child = -1;
static int input = -1, output = -1, state, volume = 100;
static unsigned events, volumes, greetings, errors;
static char line[2048];
static size_t used;
void robot_audio_cleanup(void) {
    if (input>=0) close(input);
    if (output>=0) close(output);
    input=output=-1;
    if (child>0) {
        /* Owned child only. Bounded termination, including a hung decoder. */
        kill(child,SIGTERM);
        for (int i=0;i<20;i++) {
            pid_t r=waitpid(child,NULL,WNOHANG);
            if (r==child || (r<0 && errno==ECHILD)) { child=-1;break; }
            struct timespec delay={0,10000000}; nanosleep(&delay,NULL);
        }
        if (child>0) { kill(child,SIGKILL); while (waitpid(child,NULL,0)<0 && errno==EINTR) {} }
    }
    child=-1;state=0;used=0;
}
static void parse_line(void) {
    int value;
    if (sscanf(line,"@P %d",&value)==1) {
        state=value==2?2:value==1?1:0;events++;
    } else if (!strncmp(line,"@V ",3)) volumes++;
    else if (!strncmp(line,"@R ",3)) greetings++;
    else if (!strncmp(line,"@E ",3)) {
        fprintf(stderr,"Audio mpg123: %s\n",line);errors++;state=0;
    }
}
void robot_audio_poll(void) {
    if (output<0) return;
    char block[4096]; ssize_t n;
    while ((n=read(output,block,sizeof block))>0) {
        for (ssize_t i=0;i<n;i++) {
            if (block[i]=='\n') { line[used]=0;parse_line();used=0; }
            else if (used<sizeof line-1) line[used++]=block[i];
        }
    }
    if (n==0 || (n<0 && errno!=EAGAIN && errno!=EINTR)) robot_audio_cleanup();
}
static int send_command(const char *cmd) {
    size_t count=strlen(cmd);
    while (count) {
        ssize_t n=write(input,cmd,count);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return -1;
        cmd+=n; count-=(size_t)n;
    }
    return 0;
}
static double seconds(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec/1e9;
}
static int await_event(unsigned *counter, unsigned before, int expected) {
    unsigned prior_errors=errors; double deadline=seconds()+1.0;
    while (seconds()<deadline) {
        robot_audio_poll();
        if (output<0 || errors!=prior_errors) return -1;
        if (*counter!=before && (expected<0 || state==expected)) return 0;
        struct pollfd p={output,POLLIN,0};
        if (poll(&p,1,20)<0 && errno!=EINTR) return -1;
    }
    return -1;
}
static int start(void) {
    robot_audio_poll(); if (child>0) return 0;
    int a[2],b[2];
    if (pipe(a)) return -1;
    if (pipe(b)) { close(a[0]);close(a[1]);return -1; }
    for (int i=0;i<2;i++) { fcntl(a[i],F_SETFD,FD_CLOEXEC);fcntl(b[i],F_SETFD,FD_CLOEXEC); }
    unsigned before=greetings;
    child=fork();
    if (!child) {
        dup2(a[0],STDIN_FILENO);dup2(b[1],STDOUT_FILENO);
        close(a[0]);close(a[1]);close(b[0]);close(b[1]);
        const char *device=getenv("ROBOT_AUDIO_DEVICE");
        if (!device || !*device) device="plughw:CARD=Device,DEV=0";
        execl(ROBOT_AUDIO_PLAYER_PATH,ROBOT_AUDIO_PLAYER_PATH,"-R","-o","alsa","-a",device,"-m","-f","32768",(char *)NULL);
        perror("mpg123");_exit(127);
    }
    close(a[0]);close(b[1]);
    if (child<0) { close(a[1]);close(b[0]);return -1; }
    input=a[1];output=b[0];used=0;
    fcntl(input,F_SETFL,O_NONBLOCK);fcntl(output,F_SETFL,O_NONBLOCK);
    if (await_event(&greetings,before,-1) || send_command("SILENCE\n")) { robot_audio_cleanup();return -1; }
    return 0;
}
static int set_volume(int value) {
    char cmd[64];snprintf(cmd,sizeof cmd,"VOLUME %d\n",value);
    unsigned before=volumes;
    if (send_command(cmd) || await_event(&volumes,before,-1)) return -1;
    volume=value;return 0;
}

static char current_file[PATH_MAX];
static int initialized;
int robot_audio_init(void) {
    if (initialized) return ROBOT_OK;
    struct sigaction action={0}; action.sa_handler=SIG_IGN;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGPIPE,&action,NULL)) return ROBOT_ERROR;
    volume=100; initialized=1; return ROBOT_OK;
}
int robot_audio_set_volume(int value) {
    if (value<0 || value>100) return ROBOT_INVALID_ARGUMENT;
    if (robot_audio_init()!=ROBOT_OK) return ROBOT_ERROR;
    robot_audio_poll();
    if (child>0 && set_volume(value)) { robot_audio_cleanup();return ROBOT_ERROR; }
    volume=value;return ROBOT_OK;
}
int robot_audio_pause(void) {
    robot_audio_poll();
    if (!state) return ROBOT_ERROR;
    if (state==1) return ROBOT_OK;
    unsigned before=events;
    if (send_command("PAUSE\n") || await_event(&events,before,1)) { robot_audio_cleanup();return ROBOT_ERROR; }
    return ROBOT_OK;
}
int robot_audio_stop(void) {
    robot_audio_cleanup();current_file[0]=0;return ROBOT_OK;
}
int robot_audio_play(const char *filename) {
    struct stat st;
    if (!filename || !*filename || strlen(filename)>=sizeof current_file ||
        strchr(filename,'\n') || strchr(filename,'\r') || stat(filename,&st) ||
        !S_ISREG(st.st_mode) || st.st_size<=0 || access(filename,R_OK)) return ROBOT_INVALID_ARGUMENT;
    const char *ext=strrchr(filename,'.');
    if (!ext || strcasecmp(ext,".mp3")) return ROBOT_INVALID_ARGUMENT;
    if (robot_audio_init()!=ROBOT_OK) return ROBOT_ERROR;
    robot_audio_poll();
    if (state==1 && !strcmp(filename,current_file)) {
        unsigned before=events;
        if (send_command("PAUSE\n") || await_event(&events,before,2)) { robot_audio_cleanup();return ROBOT_ERROR; }
        return ROBOT_OK;
    }
    robot_audio_cleanup();
    if (start() || set_volume(volume)) { robot_audio_cleanup();return ROBOT_ERROR; }
    char cmd[PATH_MAX+16];snprintf(cmd,sizeof cmd,"LOAD %s\n",filename);
    unsigned before=events;
    if (send_command(cmd) || await_event(&events,before,2)) { robot_audio_cleanup();return ROBOT_ERROR; }
    strcpy(current_file,filename);return ROBOT_OK;
}
const char *robot_audio_get_state(void) {
    robot_audio_poll();return state==2?"playing":state==1?"paused":"stopped";
}
