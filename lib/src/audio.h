#ifndef AUDIO_H
#define AUDIO_H

int robot_audio_init(void);
void robot_audio_cleanup(void);

int robot_alerts_init(void);
void robot_alerts_cleanup(void);
int robot_audio_alert_file(const char *path);

#endif
