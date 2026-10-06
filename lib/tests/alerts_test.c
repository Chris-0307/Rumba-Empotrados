#define _POSIX_C_SOURCE 200809L
#include "robot_hw.h"
#include "audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static void delay(long ms) {struct timespec p={ms/1000,(ms%1000)*1000000};nanosleep(&p,NULL);}
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void active(void) {for(int i=0;i<200;i++){if(!access("lib/tests/fixtures/effect-active",F_OK))return;delay(5);}assert(0);}
static void done(void) {for(int i=0;i<300;i++){if(access("lib/tests/fixtures/effect-active",F_OK)){for(int j=0;j<300;j++){if(robot_audio_set_volume(70)==0)return;delay(5);}assert(0);}delay(5);}assert(0);}
int main(void) {
 setenv("ROBOT_AUDIO_DEVICE","test-device",1);setenv("ALERT_TEST_ROOT","lib/tests/fixtures",1);
 setenv("ROBOT_ALERTS_DIR","lib/sounds",1);
 unlink("lib/tests/fixtures/commands.log");unlink("lib/tests/fixtures/alerts.log");
 assert(robot_audio_init()==0);assert(robot_alerts_init()==0);
 assert(robot_audio_set_volume(70)==0);assert(robot_audio_play("lib/tests/fixtures/song.mp3")==0);
 assert(!strcmp(robot_audio_get_state(),"playing"));
 double t=now();assert(robot_audio_alert(ROBOT_ALERT_START)==0);assert(now()-t<.05);
 active();t=now();assert(!strcmp(robot_audio_get_state(),"playing"));assert(now()-t<.05);
 assert(robot_audio_alert(ROBOT_ALERT_START)==0); /* Duplicado activo no se encola. */
 done();assert(!strcmp(robot_audio_get_state(),"playing"));
 assert(robot_audio_pause()==0);assert(robot_audio_alert(ROBOT_ALERT_MANUAL)==0);
 active();done();assert(!strcmp(robot_audio_get_state(),"paused"));
 assert(robot_audio_play("lib/tests/fixtures/song.mp3")==0);
 assert(robot_audio_alert(ROBOT_ALERT_AUTOMATIC)==0);active();
 t=now();assert(robot_audio_stop()==0);assert(now()-t<.05);
 done();assert(!strcmp(robot_audio_get_state(),"stopped")); /* No reanudar si usuario paro. */
 setenv("ALERT_TEST_FAIL","1",1);assert(robot_audio_play("lib/tests/fixtures/song.mp3")==0);
 assert(robot_audio_alert(ROBOT_ALERT_OBSTACLE)==0);active();done();assert(!strcmp(robot_audio_get_state(),"playing"));
 unsetenv("ALERT_TEST_FAIL");
 assert(robot_audio_alert(ROBOT_ALERT_CYCLE_END)==0);active();done();assert(!strcmp(robot_audio_get_state(),"playing"));
 setenv("ALERT_TEST_HANG","1",1);
 assert(robot_audio_alert(ROBOT_ALERT_START)==0);active();t=now();
 robot_alerts_cleanup();assert(now()-t<3.5);robot_audio_stop();
 unlink("lib/tests/fixtures/effect-active");unsetenv("ALERT_TEST_HANG");
 assert(robot_audio_alert(ROBOT_ALERT_START)==ROBOT_NOT_INITIALIZED);
 FILE *f=fopen("lib/tests/fixtures/commands.log","r");assert(f);char buf[8192];size_t n=fread(buf,1,sizeof buf-1,f);buf[n]=0;fclose(f);
 assert(strstr(buf,"LOADPAUSED lib/tests/fixtures/song.mp3"));assert(strstr(buf,"JUMP 42"));assert(strstr(buf,"VOLUME 70"));
 f=fopen("lib/tests/fixtures/alerts.log","r");assert(f);n=fread(buf,1,sizeof buf-1,f);buf[n]=0;fclose(f);
 assert(strstr(buf,"inicio.wav") && strstr(buf,"manual.wav") && strstr(buf,"automatico.wav") && strstr(buf,"obstaculo.wav") && strstr(buf,"fin_ciclo.wav"));
 puts("OK: cola asincrona, consultas de audio sin espera, liberar salida, recuperar posicion/pausa, stop y fallo de aplay.");
}
