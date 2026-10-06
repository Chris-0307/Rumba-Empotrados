#define _POSIX_C_SOURCE 200809L
#include "robot_hw.h"
#include "audio.h"
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define QUEUE_MAX 8
static pthread_mutex_t queue_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_ready=PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static int started, quitting, count, disabled, active=-1;
static RobotAudioAlert queue[QUEUE_MAX];
static char sound_dir[PATH_MAX];
static const char *files[]={"inicio.wav","manual.wav","automatico.wav","obstaculo.wav","fin_ciclo.wav"};
static void *play_alerts(void *unused) {
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&queue_lock);
        if (!count && !quitting) {
            struct timespec deadline;clock_gettime(CLOCK_REALTIME,&deadline);
            deadline.tv_nsec+=100000000;
            if(deadline.tv_nsec>=1000000000) {deadline.tv_sec++;deadline.tv_nsec-=1000000000;}
            (void)pthread_cond_timedwait(&queue_ready,&queue_lock,&deadline);
        }
        if (quitting) {pthread_mutex_unlock(&queue_lock);break;}
        if (!count) {
            pthread_mutex_unlock(&queue_lock);
            /* Drenar progreso mpg123 aunque la web este cerrada. */
            (void)robot_audio_get_state();
            continue;
        }
        RobotAudioAlert alert=queue[0];count--;
        memmove(queue,queue+1,(size_t)count*sizeof queue[0]);active=alert;
        pthread_mutex_unlock(&queue_lock);
        char path[PATH_MAX];
        int n=snprintf(path,sizeof path,"%s/%s",sound_dir,files[alert]);
        if (n<0 || (size_t)n>=sizeof path || robot_audio_alert_file(path)!=ROBOT_OK)
            fprintf(stderr,"Aviso de audio fallo: %s\n",files[alert]);
        pthread_mutex_lock(&queue_lock);active=-1;pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}
int robot_alerts_init(void) {
    pthread_mutex_lock(&queue_lock);
    if (started) {pthread_mutex_unlock(&queue_lock);return ROBOT_OK;}
    const char *enabled=getenv("ROBOT_ALERTS");
    disabled=enabled && !strcmp(enabled,"0");
    const char *dir=getenv("ROBOT_ALERTS_DIR");
    if (!dir || !*dir) dir="/usr/share/robot/sounds";
    if (strlen(dir)>=sizeof sound_dir) {pthread_mutex_unlock(&queue_lock);return ROBOT_ERROR;}
    strcpy(sound_dir,dir);quitting=0;count=0;active=-1;
    int result=pthread_create(&worker,NULL,play_alerts,NULL);
    if (!result) started=1;
    pthread_mutex_unlock(&queue_lock);return result?ROBOT_ERROR:ROBOT_OK;
}
int robot_audio_alert(RobotAudioAlert alert) {
    if (alert<ROBOT_ALERT_START || alert>ROBOT_ALERT_CYCLE_END) return ROBOT_INVALID_ARGUMENT;
    pthread_mutex_lock(&queue_lock);
    if (!started || quitting) {pthread_mutex_unlock(&queue_lock);return ROBOT_NOT_INITIALIZED;}
    if (disabled) {pthread_mutex_unlock(&queue_lock);return ROBOT_OK;}
    if (active==(int)alert) {pthread_mutex_unlock(&queue_lock);return ROBOT_OK;}
    for(int i=0;i<count;i++) if(queue[i]==alert) {pthread_mutex_unlock(&queue_lock);return ROBOT_OK;}
    if (count>=QUEUE_MAX) {pthread_mutex_unlock(&queue_lock);return ROBOT_ERROR;}
    queue[count++]=alert;pthread_cond_signal(&queue_ready);
    pthread_mutex_unlock(&queue_lock);return ROBOT_OK;
}
void robot_alerts_cleanup(void) {
    pthread_mutex_lock(&queue_lock);
    if (!started) {pthread_mutex_unlock(&queue_lock);return;}
    quitting=1;count=0;pthread_cond_signal(&queue_ready);
    pthread_mutex_unlock(&queue_lock);
    pthread_join(worker,NULL);
    pthread_mutex_lock(&queue_lock);started=0;active=-1;pthread_mutex_unlock(&queue_lock);
}
