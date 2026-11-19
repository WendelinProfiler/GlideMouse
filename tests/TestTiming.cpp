#include "../src/MagicMouseEngine.h"
#include <cassert>
#include <cstdio>
using namespace MagicMouse;
static Frame one(double t,double x,double y){Frame f;f.time=t;f.contacts={{0,x,y,0x40}};return f;}
static Frame two(double t,double x){auto f=one(t,x,0);f.contacts.push_back({1,x+50,0,0x40});return f;}
static Frame lift(double t){Frame f;f.time=t;return f;}
static bool has(const std::vector<Event>& events,Action a){return std::any_of(events.begin(),events.end(),[&](auto e){return e.action==a;});}
int main(){
 // A real fast stroke delivered as a USB burst must still coast, at a bounded
 // speed rather than dividing the physical motion by microsecond arrival gaps.
 Engine e;e.Feed(one(0,0,0));for(int i=1;i<=8;i++)e.Feed(one(i*.0001,0,i*80));e.Feed(lift(.02));assert(e.Coasting());auto tail=e.Tick(.008);assert(has(tail,Action::Wheel));assert(std::abs(tail[0].y)<200);
 // Losing one touch briefly must not erase all progress towards a two-finger
 // swipe, and the same stroke must trigger only once.
 e.Reset();e.Feed(two(1,0));assert(!has(e.Feed(two(1.03,-80)),Action::DesktopNext));e.Feed(one(1.04,-90,0));e.Feed(two(1.05,-110));e.Feed(two(1.06,-140));assert(has(e.Feed(two(1.07,-180)),Action::DesktopNext));assert(!has(e.Feed(two(1.08,-250)),Action::DesktopNext));assert(!has(e.Feed(lift(1.09)),Action::DesktopNext));
 // A finger placed solely to brake coasting must not turn into a click on the
 // content below it, even when optional tap-to-click is enabled.
 e.Reset();e.settings.tapToClick=true;auto start=one(2,0,0);start.contacts[0].phase=0x30;e.Feed(start);for(int i=1;i<=8;i++)e.Feed(one(2+i*.01,0,i*80));e.Feed(lift(2.1));assert(e.Coasting());auto brake=one(2.12,-100,0);brake.contacts[0].phase=0x30;e.Feed(brake);assert(!e.Coasting());assert(!has(e.Feed(lift(2.18)),Action::LeftClick));
 auto tap=one(2.4,-100,0);tap.contacts[0].phase=0x30;e.Feed(tap);assert(has(e.Feed(lift(2.46)),Action::LeftClick));
 // Real swipes naturally nudge the body; pointer jitter must not permanently
 // poison the recognizer. Resting fingers before movement are equally valid.
 e.Reset();e.Feed(two(3,0));auto nudge=two(3.04,-60);nudge.pointerX=-28;nudge.pointerY=12;e.Feed(nudge);assert(has(e.Feed(two(3.08,-220)),Action::DesktopNext));
 e.Reset();e.Feed(two(4,0));auto rest=two(8,0);rest.pointerX=150;e.Feed(rest);assert(has(e.Feed(two(8.1,230)),Action::DesktopPrevious));
 // Mouse-body movement still cannot masquerade as a desktop gesture.
 e.Reset();e.Feed(two(9,0));auto body=two(9.1,180);body.pointerX=300;assert(!has(e.Feed(body),Action::DesktopPrevious));assert(!has(e.Feed(lift(9.2)),Action::DesktopPrevious));
 puts("PASS batched scroll timing, bounded momentum speed, touch-dropout swipe continuity, single-fire swipe, braking without accidental click and subsequent intentional tap");
}
