#include <cassert>
#include <cstdio>
#include "../src/DirectMousePointer.h"
int main(){
 DirectMousePointer::State pointer;MagicMouse::Frame frame;
 frame.pointerX=-32768;frame.pointerY=32767;frame.buttons=1;
 auto result=pointer.Feed(frame);assert(result.x==-32768&&result.y==32767&&result.down==1&&!result.up);
 frame.pointerX=frame.pointerY=0;result=pointer.Feed(frame);assert(!result.Any());
 frame.buttons=2;result=pointer.Feed(frame);assert(result.up==1&&result.down==2);
 result=pointer.Release();assert(result.up==2&&!result.down);
 assert(!pointer.Release().Any());frame.buttons=0;assert(!pointer.Feed(frame).Any());
 frame.buttons=3;assert(pointer.Feed(frame).down==3);assert(pointer.Release().up==3);
 puts("PASS pointer movement, button transitions, no repeated downs and idempotent disconnect release");
}
