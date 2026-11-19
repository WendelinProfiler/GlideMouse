#include "../src/MagicMouseEngine.h"
#include <cassert>
#include <cstdio>
using namespace MagicMouse;
static Frame single(double t,double x,unsigned buttons=0){Frame f;f.time=t;f.buttons=buttons;f.contacts={{0,x,0}};return f;}
static Frame pair(double t){Frame f;f.time=t;f.contacts={{0,-600,0},{1,600,0}};return f;}
static Frame up(double t){Frame f;f.time=t;return f;}
static bool has(const std::vector<Event>& events,Action action){for(auto e:events)if(e.action==action)return true;return false;}
int main(){
 Engine e;assert(!e.settings.tapToClick);e.Feed(single(0,-600));assert(e.Feed(up(.08)).empty());e.Feed(single(.17,-600));auto events=e.Feed(up(.25));assert(has(events,Action::SmartZoom)&&!has(events,Action::LeftClick));
 e.Reset();e.settings.tapToClick=true;e.Feed(single(1,-600));events=e.Feed(up(1.08));assert(has(events,Action::LeftClick)&&!has(events,Action::SmartZoom));e.Feed(single(1.17,-600));events=e.Feed(up(1.25));assert(has(events,Action::LeftClick)&&!has(events,Action::SmartZoom));
 e.Reset();e.Feed(single(2,600));assert(has(e.Feed(up(2.025)),Action::RightClick));
 e.Reset();e.Feed(single(3,-600,1));assert(!has(e.Feed(up(3.08)),Action::LeftClick));
 e.Reset();e.Feed(single(4,-600));auto f=pair(4.5);e.Feed(f);f=single(4.6,-600);events=e.Feed(f);assert(has(events,Action::RightClick)&&!has(events,Action::LeftClick));assert(e.Feed(up(4.8)).empty());
 e.Reset();e.Feed(pair(5));assert(e.Feed(up(5.08)).empty());e.Feed(pair(5.17));events=e.Feed(up(5.25));assert(has(events,Action::WindowOverview)&&!has(events,Action::LeftClick)&&!has(events,Action::RightClick));
 e.Reset();e.Feed(single(6,-600));f=single(6.04,-580);f.pointerX=40;assert(e.Feed(f).empty());assert(e.Feed(up(6.08)).empty());
 e.Reset();e.Feed(single(7,-600));assert(e.Feed(up(7.01)).empty());
 e.Reset();f=single(8,-600);f.contacts[0].phase=0x70;e.Feed(f);assert(e.Feed(up(8.08)).empty());
 e.Reset();e.Feed(single(9,-600));f=up(9.04);f.hasContacts=false;f.pointerX=35;e.Feed(f);assert(e.Feed(up(9.1)).empty());
 e.Reset();e.Feed(pair(10));f=pair(10.1);for(auto& c:f.contacts)c.x-=200;assert(has(e.Feed(f),Action::DesktopNext));e.Feed(up(10.12));e.Feed(single(10.3,-600));assert(has(e.Feed(up(10.4)),Action::LeftClick));
 puts("PASS standard zoom by default, optional left/right tap, native double click, mechanical rejection, resting finger, two-finger overview, pointer rejection, short-contact rejection");
}
