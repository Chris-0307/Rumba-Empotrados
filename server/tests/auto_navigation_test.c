#include "auto_navigation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void advance(AutoNavigation *n,double now) {
    assert(auto_navigation_step(n,now,50,1)==AUTO_STOP);
    assert(auto_navigation_step(n,now+.1,50,1)==AUTO_STOP);
    assert(auto_navigation_step(n,now+.2,50,1)==AUTO_ADVANCE);
}
int main(void) {
    AutoNavigation n;auto_navigation_init(&n,123,30,25,6);
    assert(auto_navigation_step(&n,1,50,1)==AUTO_STOP);
    auto_navigation_start(&n);assert(auto_navigation_step(&n,1,50,0)==AUTO_STOP);
    advance(&n,1.1);
    assert(auto_navigation_step(&n,1.4,22,1)==AUTO_ADVANCE); /* Histeresis en avance. */
    assert(auto_navigation_step(&n,1.5,19,1)==AUTO_STOP && n.phase==AUTO_BRAKING);
    assert(auto_navigation_step(&n,1.6,19,1)==AUTO_STOP);
    AutoAction turn=auto_navigation_step(&n,1.7,19,1);assert(turn==AUTO_LEFT || turn==AUTO_RIGHT);
    assert(n.turn_speed==25 && n.deadline==7.7);
    for(int i=0;i<20;i++)assert(auto_navigation_step(&n,1.8+i*.1,10,1)==turn);
    assert(auto_navigation_step(&n,4,40,1)==turn);
    assert(auto_navigation_step(&n,4.1,25,1)==turn); /* >25 estricto y reinicia confirmaciones. */
    assert(auto_navigation_step(&n,4.2,40,1)==turn);
    assert(auto_navigation_step(&n,4.3,NAN,1)==AUTO_STOP && n.phase==AUTO_TURNING);
    assert(n.turn_action==turn && n.deadline==7.7); /* No nuevo lado ni nuevo plazo. */
    assert(auto_navigation_step(&n,4.4,40,1)==turn);
    assert(auto_navigation_step(&n,4.5,40,1)==turn);
    assert(auto_navigation_step(&n,4.6,40,1)==AUTO_STOP && n.phase==AUTO_WAITING);
    assert(!strcmp(n.reason,"checking_front"));advance(&n,4.7);
    assert(auto_navigation_step(&n,5,50,0)==AUTO_STOP && n.phase==AUTO_PAUSED);
    assert(auto_navigation_step(&n,5.1,50,1)==AUTO_STOP);
    auto_navigation_start(&n);advance(&n,10);
    assert(auto_navigation_step(&n,10.3,10,1)==AUTO_STOP);
    turn=auto_navigation_step(&n,10.5,10,1);
    assert(auto_navigation_step(&n,11,NAN,1)==AUTO_STOP);
    assert(auto_navigation_step(&n,16.5,NAN,1)==AUTO_STOP && n.phase==AUTO_PAUSED);
    assert(!strcmp(n.reason,"turn_timeout"));assert(auto_navigation_step(&n,17,100,1)==AUTO_STOP);
    auto_navigation_start(&n);advance(&n,20);auto_navigation_pause(&n,"user_stop");
    assert(auto_navigation_step(&n,21,50,1)==AUTO_STOP);
    auto_navigation_start(&n);advance(&n,22);assert(auto_navigation_step(&n,23,NAN,1)==AUTO_STOP);advance(&n,23.1);
    auto_navigation_cancel(&n);assert(auto_navigation_step(&n,24,50,1)==AUTO_STOP);
    int left=0,right=0;
    for(int i=0;i<50;i++) {
        auto_navigation_start(&n);assert(auto_navigation_step(&n,30+i,10,1)==AUTO_STOP);
        turn=auto_navigation_step(&n,30.2+i,10,1);left+=turn==AUTO_LEFT;right+=turn==AUTO_RIGHT;
        assert(auto_navigation_step(&n,30.3+i,10,1)==turn);
    }
    assert(left && right);
    puts("OK: mismo lado, tres muestras >25, histeresis, eco invalido, timeout, suelo y pausa.");
}
