#ifndef AUDIO_CATALOG_H
#define AUDIO_CATALOG_H
int audio_catalog_playlist(char *json, unsigned long cap);
int audio_catalog_path(int id, char *path, unsigned long cap);
int audio_catalog_next(int current);
#endif
