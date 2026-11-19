#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace MagicMouse {
struct Contact {int id=0;double x=0,y=0;uint8_t phase=0x30;};
struct Frame {double time=0;unsigned buttons=0;int pointerX=0,pointerY=0;bool hasContacts=true;std::vector<Contact> contacts;};
enum class Action {Wheel,PageBack,PageForward,DesktopPrevious,DesktopNext,SmartZoom,WindowOverview,LeftClick,RightClick};
struct Event {Action action;double x=0,y=0;};
struct Settings {bool natural=true,momentum=true,swipes=true,taps=true,tapToClick=false;double sensitivity=2.0,decay=.28,tapSlop=40,scrollSlop=48,tapRightBoundary=0;};
inline int Signed12(unsigned value){value&=4095;return value&2048?int(value)-4096:int(value);}
// USB-C/Mouse 2 wire format. Protocol facts only; no kernel or commercial code.
inline bool Parse(const uint8_t* bytes,size_t size,double time,Frame& frame){
 if(!bytes||!std::isfinite(time)||size<8||bytes[0]!=0x12)return false;
 if(size!=8&&(size<14||(size-14)%8||size>14+16*8))return false;
 frame={};frame.time=time;frame.buttons=bytes[1]&3;
 frame.pointerX=int16_t(unsigned(bytes[2])|unsigned(bytes[3])<<8);frame.pointerY=int16_t(unsigned(bytes[4])|unsigned(bytes[5])<<8);
 // Compact reports update pointer/buttons only. The real captures carry finger
 // releases as full touch records with state NONE; do not invent lifts between
 // full reports when the user merely moves the mouse body.
 frame.hasContacts=size!=8;std::array<bool,16> seen{};
 for(size_t offset=14;offset+8<=size;offset+=8){auto p=bytes+offset;unsigned phase=p[7]&0xf0;int id=((p[6]<<2)|(p[5]>>6))&15;
 // 0x50/0x60 also occur on the physical USB-C mouse. A contact is down until
 // state NONE (0), matching the protocol's tracking state, not just scroll states.
 if(phase==0)continue;if(seen[id])return false;seen[id]=true;
 frame.contacts.push_back({id,double(Signed12(unsigned(p[0])|unsigned(p[1])<<8)),-double(Signed12(unsigned(p[1])>>4|unsigned(p[2])<<4)),uint8_t(phase)});}
 return true;
}
class Engine {
public:
 Settings settings;
 std::vector<Event> Feed(const Frame& frame){
  std::vector<Event> out;if(!std::isfinite(frame.time)||(lastTime>=0&&frame.time<lastTime))return out;
  double dt=lastTime<0?.008:std::clamp(frame.time-lastTime,.008,.1);lastTime=frame.time;
  TrackTapClicks(frame,out);
  if(frame.buttons||frame.pointerX||frame.pointerY){StopMomentum();if(active&&frame.buttons)clicked=true;}
  if(active){pointerX+=frame.pointerX;pointerY+=frame.pointerY;pointerTravel+=std::hypot(frame.pointerX,frame.pointerY);pointerMode|=std::hypot(pointerX,pointerY)>24||pointerTravel>48;}
  if(!frame.hasContacts)return out;
  lastContactFrame=frame.time;
  if(frame.contacts.empty()){
   if(active){double duration=frame.time-startTime;
    if(!clicked && !pointerMode && !frame.pointerX && !frame.pointerY && mode==Mode::Scroll && settings.momentum && (duration>.05||(scrollMotionSamples>=3&&maxTravel>settings.scrollSlop*2)) && std::hypot(vx,vy)>40){tailX=vx;tailY=vy;tailAge=0;}
    else if(tapBegan&&tapConfirmed&&!clicked&&!pointerMode&&mode!=Mode::Scroll&&maxTravel<settings.tapSlop&&duration>=.020&&duration<.22&&settings.taps&&(maxContacts==1||coordinated)&&(!settings.tapToClick||maxContacts>=2)){
     if(lastTapCount==maxContacts&&frame.time-lastTap<.34&&std::hypot(startX-lastTapX,startY-lastTapY)<120){out.push_back({maxContacts==1?Action::SmartZoom:Action::WindowOverview});lastTap=-10;lastTapCount=0;}
     else{lastTap=frame.time;lastTapCount=maxContacts;lastTapX=startX;lastTapY=startY;}
    }else if(!swipeFired&&!clicked&&!pointerMode&&mode==Mode::Swipe&&settings.swipes&&duration<1.3&&std::abs(totalX)>130&&std::abs(totalX)>std::abs(totalY)*1.5){
     out.push_back({maxContacts>=2?(totalX>0?Action::DesktopPrevious:Action::DesktopNext):(totalX>0?Action::PageBack:Action::PageForward)});lastTap=-10;
    }
   }
   active=false;swipeFired=false;swipeStart=-1;mode=Mode::Undecided;return out;
  }
  StopMomentum();double x=0,y=0;for(auto c:frame.contacts){x+=c.x;y+=c.y;}x/=frame.contacts.size();y/=frame.contacts.size();
  if(!active){active=true;swipeFired=false;swipeStart=-1;swipePointerBase=0;mode=Mode::Undecided;startTime=frame.time;startX=previousX=x;startY=previousY=y;totalX=totalY=vx=vy=maxTravel=0;scrollMotionSamples=0;maxContacts=lastContacts=(int)frame.contacts.size();clicked=frame.buttons!=0;pointerX=frame.pointerX;pointerY=frame.pointerY;pointerTravel=std::hypot(pointerX,pointerY);pointerMode=pointerTravel>24;coordinated=true;tapBegan=tapConfirmed=false;for(auto c:frame.contacts){tapBegan|=c.phase==0x20||c.phase==0x30;tapConfirmed|=c.phase==0x30||c.phase==0x40;}return out;}
  for(auto c:frame.contacts)tapConfirmed|=c.phase==0x30||c.phase==0x40;
  clicked|=frame.buttons!=0;
  if((int)frame.contacts.size()!=lastContacts){bool added=(int)frame.contacts.size()>lastContacts;bool firstMulti=added&&maxContacts<2&&frame.contacts.size()>=2;lastContacts=(int)frame.contacts.size();maxContacts=std::max(maxContacts,lastContacts);previousX=x;previousY=y;if(firstMulti){swipeStart=-1;swipePointerBase=0;coordinated&=frame.time-startTime<.060;startX=x;startY=y;totalX=totalY=0;vx=vy=0;mode=Mode::Undecided;}return out;}
  double dx=x-previousX,dy=y-previousY;previousX=x;previousY=y;totalX+=dx;totalY+=dy;maxTravel=std::max(maxTravel,std::hypot(totalX,totalY));
  // A two-finger swipe is touch motion, not a perfectly motionless mouse body.
  // Start its clock at deliberate motion, so fingers may rest before swiping.
  const bool multiHorizontal=maxContacts>=2&&std::abs(totalX)>settings.scrollSlop&&std::abs(totalX)>std::abs(totalY)*1.5;
  if(multiHorizontal&&swipeStart<0){swipeStart=frame.time;swipePointerBase=pointerTravel-std::hypot(frame.pointerX,frame.pointerY);}
  const bool swipeDominates=multiHorizontal&&pointerTravel-swipePointerBase<=std::max(64.,std::abs(totalX)*.5);
  if(clicked||(pointerMode&&!swipeDominates)){mode=Mode::Undecided;lastTap=-10;return out;}
  if(mode==Mode::Undecided&&maxTravel>settings.scrollSlop){
   if(std::abs(totalY)>std::abs(totalX)*1.25&&maxContacts==1){mode=Mode::Scroll;dx=totalX;dy=totalY;}
   else if(std::abs(totalX)>std::abs(totalY)*1.5&&std::abs(totalX)>24)mode=Mode::Swipe;
  }
  if(!swipeFired&&mode==Mode::Swipe&&settings.swipes&&maxContacts>=2&&swipeStart>=0&&frame.time-swipeStart<1.3&&std::abs(totalX)>130&&std::abs(totalX)>std::abs(totalY)*1.5){
   out.push_back({totalX>0?Action::DesktopPrevious:Action::DesktopNext});swipeFired=true;lastTap=-10;tapCandidates={};return out;
  }
  if(mode==Mode::Swipe&&maxContacts==1&&frame.time-startTime>.16&&std::abs(totalX)/(frame.time-startTime)<600){mode=Mode::Scroll;dx=totalX;dy=totalY;}
  if(mode==Mode::Scroll){
   double sx=(settings.natural?-1:1)*dx*settings.sensitivity,sy=(settings.natural?1:-1)*dy*settings.sensitivity;
   double blend=1-std::exp(-dt/.04);vx+=(sx/dt-vx)*blend;vy+=(sy/dt-vy)*blend;
   if(sx||sy){scrollMotionSamples++;out.push_back({Action::Wheel,sx,sy});}lastTap=-10;
  }
  return out;
 }
 std::vector<Event> Tick(double dt){
  std::vector<Event> out;if(!std::isfinite(dt)||dt<=0)return out;dt=std::min(dt,.1);
  if(!settings.momentum){StopMomentum();return out;}
  if(active||(!tailX&&!tailY))return out;
  double tau=std::clamp(settings.decay,.08,.6),factor=std::exp(-dt/tau);
  double dx=tailX*tau*(1-factor),dy=tailY*tau*(1-factor);tailX*=factor;tailY*=factor;tailAge+=dt;
  if(dx||dy)out.push_back({Action::Wheel,dx,dy});
  if(std::hypot(tailX,tailY)<5||tailAge>2)StopMomentum();return out;
 }
 void StopMomentum(){tailX=tailY=0;tailAge=0;}
 void Reset(){active=false;swipeFired=false;swipeStart=-1;mode=Mode::Undecided;vx=vy=0;lastTap=-10;lastTapCount=0;lastTime=-1;tapCandidates={};pointerMode=false;StopMomentum();}
 bool Coasting()const{return tailX!=0||tailY!=0;}
#ifdef MAGIC_MOUSE_DIAGNOSTICS
 struct Debug {int mode,contacts,lastTapContacts;bool clicked;double travel,x,y,tapX,tapY,tapTime;};
 Debug Inspect()const{return {int(mode),maxContacts,lastTapCount,clicked,maxTravel,startX,startY,lastTapX,lastTapY,lastTap};}
#endif
private:
 struct TapCandidate {bool active=false,combined=false,clicked=false,confirmed=false;double time=0,x=0,y=0,travel=0,px=0,py=0,pmax=0;};
 std::array<TapCandidate,16> tapCandidates{};
 void TrackTapClicks(const Frame& frame,std::vector<Event>& out){
  if(!settings.tapToClick||swipeFired){tapCandidates={};return;}if(!frame.hasContacts){for(auto& c:tapCandidates)if(c.active){c.combined|=Coasting();c.clicked|=frame.buttons!=0;c.px+=frame.pointerX;c.py+=frame.pointerY;c.pmax=std::max(c.pmax,std::hypot(c.px,c.py));}return;}
  std::array<bool,16> seen{};
  for(auto contact:frame.contacts){if(contact.id<0||contact.id>=16)continue;seen[contact.id]=true;auto& c=tapCandidates[contact.id];if(!c.active){if(contact.phase!=0x20&&contact.phase!=0x30)continue;c={};c.active=true;c.time=frame.time;c.x=contact.x;c.y=contact.y;}c.confirmed|=contact.phase==0x30||contact.phase==0x40;c.travel=std::max(c.travel,std::hypot(contact.x-c.x,contact.y-c.y));}
  for(auto& c:tapCandidates)if(c.active){c.combined|=Coasting();c.clicked|=frame.buttons!=0;c.px+=frame.pointerX;c.py+=frame.pointerY;c.pmax=std::max(c.pmax,std::hypot(c.px,c.py));}
  // A resting finger must not turn a new single-finger tap into a two-finger
  // gesture. Only contacts beginning together form a coordinated tap.
  for(int a=0;a<16;a++)for(int b=a+1;b<16;b++)if(tapCandidates[a].active&&tapCandidates[b].active&&seen[a]&&seen[b]&&std::abs(tapCandidates[a].time-tapCandidates[b].time)<.060){tapCandidates[a].combined=tapCandidates[b].combined=true;}
  for(int id=0;id<16;id++){auto& c=tapCandidates[id];if(!c.active||seen[id])continue;double duration=frame.time-c.time;
   if(c.confirmed&&!c.clicked&&!c.combined&&duration>=.020&&duration<.22&&c.travel<settings.tapSlop&&c.pmax<=24)out.push_back({c.x>settings.tapRightBoundary?Action::RightClick:Action::LeftClick});
   c={};
  }
 }
 enum class Mode{Undecided,Scroll,Swipe};Mode mode=Mode::Undecided;bool active=false,clicked=false;
 bool pointerMode=false,coordinated=true,tapBegan=false,tapConfirmed=false,swipeFired=false;double pointerX=0,pointerY=0,pointerTravel=0;
 unsigned scrollMotionSamples=0;
 int maxContacts=0,lastContacts=0,lastTapCount=0;double lastTime=-1,lastContactFrame=0,startTime=0,startX=0,startY=0,previousX=0,previousY=0,totalX=0,totalY=0,vx=0,vy=0,maxTravel=0;
 double swipeStart=-1,swipePointerBase=0;
 double tailX=0,tailY=0,tailAge=0,lastTap=-10,lastTapX=0,lastTapY=0;
};
}
