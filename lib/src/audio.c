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
#include <pthread.h>
#include <spawn.h>
#include <stdatomic.h>
extern char **environ;
#ifndef ROBOT_APLAY_PATH
#define ROBOT_APLAY_PATH "/usr/bin/aplay"
#endif
static pthread_mutex_t audio_lock=PTHREAD_MUTEX_INITIALIZER;
static atomic_int published_state, cancel_restore;
static int audio_init_locked(void);
static int audio_pause_locked(void);
static int audio_stop_locked(void);
static int audio_play_locked(const char *filename);
static int audio_set_volume_locked(int value);
static pid_t child = -1;
static int input = -1, output = -1, state, volume = 100;
static unsigned events, volumes, greetings, errors, jumps;
static long current_frame;
static char line[2048];
static size_t used;
static void audio_cleanup_locked(void) {
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
    child=-1;state=0;used=0;current_frame=0;
}
static void parse_line(void) {
    int value;
    long frame;
    if (sscanf(line,"@F %ld",&frame)==1) {current_frame=frame;return;}
    if (!strncmp(line,"@J ",3)) {jumps++;return;}
    if (sscanf(line,"@P %d",&value)==1) {
        state=value==2?2:value==1?1:0;events++;
    } else if (!strncmp(line,"@V ",3)) volumes++;
    else if (!strncmp(line,"@R ",3)) greetings++;
    else if (!strncmp(line,"@E ",3)) {
        fprintf(stderr,"Audio mpg123: %s\n",line);errors++;state=0;
    }
}
static void audio_poll_locked(void) {
    if (output<0) return;
    char block[4096]; ssize_t n;
    while ((n=read(output,block,sizeof block))>0) {
        for (ssize_t i=0;i<n;i++) {
            if (block[i]=='\n') { line[used]=0;parse_line();used=0; }
            else if (used<sizeof line-1) line[used++]=block[i];
        }
    }
    if (n==0 || (n<0 && errno!=EAGAIN && errno!=EINTR)) audio_cleanup_locked();
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
        audio_poll_locked();
        if (output<0 || errors!=prior_errors) return -1;
        if (*counter!=before && (expected<0 || state==expected)) return 0;
        struct pollfd p={output,POLLIN,0};
        if (poll(&p,1,20)<0 && errno!=EINTR) return -1;
    }
    return -1;
}
static int start(void) {
    audio_poll_locked(); if (child>0) return 0;
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
    if (await_event(&greetings,before,-1)) { audio_cleanup_locked();return -1; }
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
static int audio_init_locked(void) {
    if (initialized) return ROBOT_OK;
    struct sigaction action={0}; action.sa_handler=SIG_IGN;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGPIPE,&action,NULL)) return ROBOT_ERROR;
    volume=100; initialized=1; return ROBOT_OK;
}
static int audio_set_volume_locked(int value) {
    if (value<0 || value>100) return ROBOT_INVALID_ARGUMENT;
    if (audio_init_locked()!=ROBOT_OK) return ROBOT_ERROR;
    audio_poll_locked();
    if (child>0 && set_volume(value)) { audio_cleanup_locked();return ROBOT_ERROR; }
    volume=value;return ROBOT_OK;
}
static int audio_pause_locked(void) {
    audio_poll_locked();
    if (!state) return ROBOT_ERROR;
    if (state==1) return ROBOT_OK;
    unsigned before=events;
    if (send_command("PAUSE\n") || await_event(&events,before,1)) { audio_cleanup_locked();return ROBOT_ERROR; }
    return ROBOT_OK;
}
static int audio_stop_locked(void) {
    audio_cleanup_locked();current_file[0]=0;return ROBOT_OK;
}
static int audio_play_locked(const char *filename) {
    struct stat st;
    if (!filename || !*filename || strlen(filename)>=sizeof current_file ||
        strchr(filename,'\n') || strchr(filename,'\r') || stat(filename,&st) ||
        !S_ISREG(st.st_mode) || st.st_size<=0 || access(filename,R_OK)) return ROBOT_INVALID_ARGUMENT;
    const char *ext=strrchr(filename,'.');
    if (!ext || strcasecmp(ext,".mp3")) return ROBOT_INVALID_ARGUMENT;
    if (audio_init_locked()!=ROBOT_OK) return ROBOT_ERROR;
    audio_poll_locked();
    if (state==1 && !strcmp(filename,current_file)) {
        unsigned before=events;
        if (send_command("PAUSE\n") || await_event(&events,before,2)) { audio_cleanup_locked();return ROBOT_ERROR; }
        return ROBOT_OK;
    }
    audio_cleanup_locked();
    if (start() || set_volume(volume)) { audio_cleanup_locked();return ROBOT_ERROR; }
    char cmd[PATH_MAX+16];snprintf(cmd,sizeof cmd,"LOAD %s\n",filename);
    unsigned before=events;
    if (send_command(cmd) || await_event(&events,before,2)) { audio_cleanup_locked();return ROBOT_ERROR; }
    strcpy(current_file,filename);return ROBOT_OK;
}


/* Reproduccion de avisos en hilo separado. Solo este mutex serializa audio,
 * nunca el mutex que protege sensores y motores. */
static int wav_once(const char *path) {
    const char *device=getenv("ROBOT_AUDIO_DEVICE");
    if (!device || !*device) device="plughw:CARD=Device,DEV=0";
    char *args[]={(char *)ROBOT_APLAY_PATH,"-q","-D",(char *)device,(char *)path,NULL};
    pid_t player;
    int error=posix_spawn(&player,ROBOT_APLAY_PATH,NULL,NULL,args,environ);
    if (error) {fprintf(stderr,"Aviso: no se pudo iniciar aplay (%d)\n",error);return ROBOT_ERROR;}
    double deadline=seconds()+2.0;
    int status;
    while (seconds()<deadline) {
        pid_t done=waitpid(player,&status,WNOHANG);
        if (done==player) return WIFEXITED(status) && WEXITSTATUS(status)==0?ROBOT_OK:ROBOT_ERROR;
        if (done<0 && errno!=EINTR) return ROBOT_ERROR;
        struct timespec pause={0,10000000};nanosleep(&pause,NULL);
    }
    kill(player,SIGKILL);while(waitpid(player,&status,0)<0 && errno==EINTR) {}
    fprintf(stderr,"Aviso: aplay excedio 2 segundos\n");return ROBOT_ERROR;
}
static int restore_song(const char *path,long frame,int previous_state) {
    if (start() || set_volume(volume)) goto fail;
    char command[PATH_MAX+32];unsigned before=events;
    snprintf(command,sizeof command,"LOADPAUSED %s\n",path);
    if (send_command(command) || await_event(&events,before,1)) goto fail;
    if (frame>0) {
        before=jumps;snprintf(command,sizeof command,"JUMP %ld\n",frame);
        if (send_command(command) || await_event(&jumps,before,-1)) goto fail;
    }
    strcpy(current_file,path);
    if (previous_state==2) {
        before=events;
        if (send_command("PAUSE\n") || await_event(&events,before,2)) goto fail;
    }
    return ROBOT_OK;
fail:
    audio_cleanup_locked();fprintf(stderr,"Aviso: no se pudo recuperar la cancion\n");return ROBOT_ERROR;
}
int robot_audio_alert_file(const char *path) {
    struct stat st;
    if (stat(path,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || access(path,R_OK)) return ROBOT_ERROR;
    pthread_mutex_lock(&audio_lock);
    audio_poll_locked();
    int previous_state=state;
    atomic_store(&published_state,state);atomic_store(&cancel_restore,0);
    char saved[PATH_MAX];strcpy(saved,current_file);
    if (state==2 && audio_pause_locked()!=ROBOT_OK) {
        pthread_mutex_unlock(&audio_lock);return ROBOT_ERROR;
    }
    audio_poll_locked();long frame=current_frame;
    /* PAUSE solo no asegura liberar ALSA; cerrar mpg123 antes de aplay. */
    audio_cleanup_locked();
    int result=wav_once(path);
    if (!atomic_load(&cancel_restore) && previous_state && saved[0] && restore_song(saved,frame,previous_state)!=ROBOT_OK) result=ROBOT_ERROR;
    if (atomic_load(&cancel_restore)) {audio_stop_locked();}
    atomic_store(&published_state,state);
    pthread_mutex_unlock(&audio_lock);return result;
}
int robot_audio_init(void) {pthread_mutex_lock(&audio_lock);int r=audio_init_locked();pthread_mutex_unlock(&audio_lock);return r;}
void robot_audio_cleanup(void) {pthread_mutex_lock(&audio_lock);audio_cleanup_locked();atomic_store(&published_state,0);pthread_mutex_unlock(&audio_lock);}
int robot_audio_set_volume(int value) {
    if (value<0 || value>100) return ROBOT_INVALID_ARGUMENT;
    if (pthread_mutex_trylock(&audio_lock)) return ROBOT_ERROR;
    int r=audio_set_volume_locked(value);atomic_store(&published_state,state);pthread_mutex_unlock(&audio_lock);return r;
}
int robot_audio_play(const char *path) {
    if (pthread_mutex_trylock(&audio_lock)) return ROBOT_ERROR;
    int r=audio_play_locked(path);atomic_store(&published_state,state);pthread_mutex_unlock(&audio_lock);return r;
}
int robot_audio_pause(void) {
    if (pthread_mutex_trylock(&audio_lock)) return ROBOT_ERROR;
    int r=audio_pause_locked();atomic_store(&published_state,state);pthread_mutex_unlock(&audio_lock);return r;
}
int robot_audio_stop(void) {
    if (pthread_mutex_trylock(&audio_lock)) {
        atomic_store(&cancel_restore,1);atomic_store(&published_state,0);return ROBOT_OK;
    }
    int r=audio_stop_locked();atomic_store(&published_state,state);pthread_mutex_unlock(&audio_lock);return r;
}
const char *robot_audio_get_state(void) {
    if (!pthread_mutex_trylock(&audio_lock)) {
        audio_poll_locked();atomic_store(&published_state,state);pthread_mutex_unlock(&audio_lock);
    }
    int current=atomic_load(&published_state);
    return current==2?"playing":current==1?"paused":"stopped";
}
