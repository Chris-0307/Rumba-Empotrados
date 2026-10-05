#include "map_estimate.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GRID 21
#define PI 3.14159265358979323846
static unsigned char cells[GRID][GRID];
static double x, y, heading, cell_cm, speed_cm_s, turn_deg_s;
static int left_power,right_power;
static double setting(const char *name, double fallback) {
    const char *s=getenv(name); char *end; double value;
    if (!s || !*s) return fallback;
    value=strtod(s,&end);
    return end!=s && !*end && isfinite(value) && value>0 && value<=1000 ? value : fallback;
}
static int coordinate(double value) { return (int)floor(value+0.5); }
void map_estimate_init(void) {
    memset(cells,0,sizeof cells); x=y=GRID/2; heading=-PI/2;
    left_power=right_power=0;
    cell_cm=setting("ROBOT_MAP_CELL_CM",20);
    speed_cm_s=setting("ROBOT_MAP_SPEED_CM_S",20);
    turn_deg_s=setting("ROBOT_MAP_TURN_DEG_S",90);
    cells[coordinate(y)][coordinate(x)]=1;
}
void map_estimate_motor_speeds(int left,int right) {left_power=left;right_power=right;}
void map_estimate_motion(const char *direction,int speed) {
    int l=speed,r=speed;
    if (!strcmp(direction,"stop")) l=r=0;
    else {
        if (!strcmp(direction,"backward") || !strcmp(direction,"left")) l=-l;
        if (!strcmp(direction,"backward") || !strcmp(direction,"right")) r=-r;
    }
    map_estimate_motor_speeds(l,r);
}
void map_estimate_advance(double seconds) {
    if (!isfinite(seconds) || seconds<=0 || (!left_power && !right_power)) return;
    double travel=seconds*speed_cm_s/cell_cm*(left_power+right_power)/200.0;
    double angle=seconds*turn_deg_s*PI/180*(left_power-right_power)/200.0;
    int steps=(int)ceil(fmax(fabs(travel)/.25,fabs(angle)/(PI/36)));
    if (steps<1) steps=1;
    if (steps>10000) steps=10000;
    for(int i=0;i<steps;i++) {
        double half=angle/(2*steps),mid=heading+half;
        double nx=x+cos(mid)*travel/steps,ny=y+sin(mid)*travel/steps;
        heading=fmod(heading+angle/steps,2*PI);
        if (nx>=0 && nx<=GRID-1 && ny>=0 && ny<=GRID-1) {x=nx;y=ny;cells[coordinate(y)][coordinate(x)]=1;}
    }
}
void map_estimate_obstacle(double cm) {
    if (!isfinite(cm) || cm<=0) return;
    double ox=x+cos(heading)*cm/cell_cm;
    double oy=y+sin(heading)*cm/cell_cm;
    if(ox<0 || ox>GRID-1 || oy<0 || oy>GRID-1) return;
    int cx=coordinate(ox),cy=coordinate(oy);
    if(cx==coordinate(x) && cy==coordinate(y)) return;
    cells[cy][cx]=2;
}
int map_estimate_json(char *json, unsigned long capacity) {
    static const char *names[]={"unknown","visited","obstacle"};
    if(!json || !capacity) return -1;
    int n=snprintf(json,capacity,"{\"width\":%d,\"height\":%d,\"estimated\":true,\"cell_cm\":%.2f,\"robot\":{\"x\":%d,\"y\":%d,\"heading_deg\":%.2f},\"cells\":[",GRID,GRID,cell_cm,coordinate(x),coordinate(y),heading*180/PI);
    if(n<0 || (unsigned long)n>=capacity) return -1;
    unsigned long used=(unsigned long)n;
    for(int row=0;row<GRID;row++) {
        n=snprintf(json+used,capacity-used,"%s[",row?",":"");
        if(n<0 || (unsigned long)n>=capacity-used) return -1;
        used+=(unsigned long)n;
        for(int col=0;col<GRID;col++) {
            n=snprintf(json+used,capacity-used,"%s\"%s\"",col?",":"",names[cells[row][col]]);
            if(n<0 || (unsigned long)n>=capacity-used) return -1;
            used+=(unsigned long)n;
        }
        n=snprintf(json+used,capacity-used,"]");
        if(n<0 || (unsigned long)n>=capacity-used) return -1;
        used+=(unsigned long)n;
    }
    n=snprintf(json+used,capacity-used,"]}");
    return n>=0 && (unsigned long)n<capacity-used ? 0:-1;
}
