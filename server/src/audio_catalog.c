#include "audio_catalog.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *directory(void) {
    const char *v=getenv("ROBOT_MUSIC_DIR");
    return v && *v ? v : "/home/root/music";
}
static int path_for(int id, char *path, size_t cap) {
    if (id<1 || id>3 || strchr(directory(),'\n') || strchr(directory(),'\r')) return -1;
    int n=snprintf(path,cap,"%s/cancion%d.mp3",directory(),id);
    return n<0 || (size_t)n>=cap ? -1 : 0;
}
/* Only the three named files form this playlist. Missing files are omitted. */
static int inventory(int present[4]) {
    uint64_t total=0;
    memset(present,0,4*sizeof *present);
    for (int id=1;id<=3;id++) {
        char path[1024]; struct stat st;
        if (path_for(id,path,sizeof path)) return -1;
        if (stat(path,&st)) { if (errno==ENOENT) continue; return -1; }
        if (!S_ISREG(st.st_mode) || st.st_size<=0 || access(path,R_OK)) return -1;
        total+=(uint64_t)st.st_size;
        if (total>60000000) { fprintf(stderr,"Audio: canciones superan 60000000 bytes\n"); return -1; }
        present[id]=1;
    }
    return 0;
}
int audio_catalog_playlist(char *json, unsigned long cap) {
    int present[4]; if (inventory(present)) return -1;
    size_t pos=0;
    if (cap<3) return -1;
    json[pos++]='[';
    for (int id=1;id<=3;id++) if (present[id]) {
        int n=snprintf(json+pos,cap-pos,"%s{\"id\":%d,\"title\":\"Canción %d\"}",pos>1?",":"",id,id);
        if (n<0 || (unsigned long)n>=cap-pos) return -1;
        pos+=(size_t)n;
    }
    if (pos+2>cap) return -1;
    json[pos++]=']';json[pos]=0;return 0;
}

int audio_catalog_path(int id, char *path, unsigned long cap) {
    int present[4];
    if (inventory(present) || id<1 || id>3 || !present[id]) return -1;
    return path_for(id,path,cap);
}
int audio_catalog_next(int current) {
    int present[4];if (inventory(present)) return -1;
    for (int step=1;step<=3;step++) { int id=(current+step-1)%3+1;if(present[id]) return id; }
    return -1;
}
