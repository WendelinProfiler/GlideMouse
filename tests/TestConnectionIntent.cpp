#include "../src/MouseConnectionIntent.h"
#include <cassert>
#include <cstdio>
int main(){MouseConnectionIntent state;assert(!state.ShouldRelease(true));state.RequestDisconnect();assert(state.ShouldRelease(true));assert(!state.ShouldRelease(false));state.RequestDisconnect();assert(state.ShouldRelease(true));state.RequestConnect();assert(!state.ShouldRelease(true));assert(!state.ShouldRelease(false));MouseConnectionIntent afterRestart;assert(!afterRestart.ShouldRelease(true));puts("PASS explicit disconnect holds through native reconnect; connect releases hold; restart defaults to normal connection");}
