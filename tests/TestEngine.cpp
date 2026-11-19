#include "../src/MagicMouseEngine.h"
#include <cassert>
#include <cstdio>
using namespace MagicMouse;
Frame one(double t,double x,double y,unsigned b=0){Frame f;f.time=t;f.buttons=b;f.contacts={{0,x,y}};return f;}
Frame two(double t,double x){Frame f;f.time=t;f.contacts={{0,x,0},{1,x+50,0}};return f;}
Frame release(double t){Frame f;f.time=t;return f;}
bool has(const std::vector<Event>&v,Action a){for(auto e:v)if(e.action==a)return true;return false;}
double coast(int hz){Engine e;e.Feed(one(0,0,0));for(int i=1;i<=12;i++)e.Feed(one(i*.01,0,i*10));e.Feed(release(.13));assert(e.Coasting());double total=0;for(int i=0;i<3*hz;i++)for(auto o:e.Tick(1.0/hz))total+=o.y;return total;}
int main(){Engine e;e.Feed(one(0,0,0));auto scroll=e.Feed(one(.02,0,60));assert(has(scroll,Action::Wheel)&&scroll[0].y>0);e.Feed(release(.08));assert(e.Coasting());e.Feed(one(.09,0,0));assert(!e.Coasting());
 e.Reset();e.Feed(one(1,0,0));e.Feed(release(1.05));e.Feed(one(1.15,2,2));assert(has(e.Feed(release(1.2)),Action::SmartZoom));
 e.Reset();e.Feed(one(2,0,0,1));e.Feed(release(2.05));e.Feed(one(2.15,0,0,1));assert(!has(e.Feed(release(2.2)),Action::SmartZoom));
 e.Reset();e.Feed(one(3,0,0));e.Feed(one(3.1,200,0));assert(has(e.Feed(release(3.12)),Action::PageBack));
 e.Reset();e.Feed(two(4,0));assert(has(e.Feed(two(4.1,-200)),Action::DesktopNext));assert(!has(e.Feed(two(4.11,-400)),Action::DesktopNext));assert(!has(e.Feed(release(4.12)),Action::DesktopNext));
 e.Reset();e.Feed(two(5,0));e.Feed(release(5.05));e.Feed(two(5.15,0));assert(has(e.Feed(release(5.2)),Action::WindowOverview));
 assert(std::abs(coast(30)-coast(165))<2);
 e.Reset();e.Feed(two(6,0));auto single=one(6.05,25,0);assert(e.Feed(single).empty());assert(!has(e.Feed(release(6.07)),Action::PageBack));
 e.Reset();e.Feed(one(7,0,0));e.Feed(one(7.04,0,60));auto moved=release(7.08);moved.pointerX=20;e.Feed(moved);assert(!e.Coasting());
 uint8_t bad[15]={0x12};Frame f;assert(!Parse(bad,15,0,f));assert(Parse(bad,8,0,f)&&!f.hasContacts);assert(Parse(bad,14,0,f)&&f.contacts.empty());
 uint8_t realPhase[22]={0x12};realPhase[21]=0x61;assert(Parse(realPhase,22,0,f)&&f.contacts.size()==1);realPhase[21]=0;assert(Parse(realPhase,22,0,f)&&f.contacts.empty());
 e.Reset();e.Feed(one(10,0,0));assert(e.Feed(one(10.04,-13,14)).empty());e.Feed(release(10.09));e.Feed(one(10.17,-50,-2));e.Feed(one(10.22,-33,2));assert(has(e.Feed(release(10.26)),Action::SmartZoom));
 e.Reset();e.Feed(two(11,0));e.Feed(release(11.09));e.Feed(one(11.17,1020,0));e.Feed(two(11.18,50));assert(has(e.Feed(release(11.26)),Action::WindowOverview));
 puts("PASS natural scroll, frame-rate independent momentum, touch-to-stop, double-tap zoom, click rejection, page swipe, desktop swipe, two-finger overview and report validation");}
