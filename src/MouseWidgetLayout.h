#pragma once
namespace MouseWidgetLayout {
constexpr float Width=390,Height=548,SliderLeft=34,SliderRight=356;
enum Control {None=-1,Close,Connect,Battery,Tap,Natural,Standard,Momentum,ScrollSpeed,PointerSpeed,Acceleration,Windows,Bluetooth};
inline Control Hit(float x,float y,bool connected){
 if(x<12||x>378||y<8||y>540)return None;
 if(x>=344&&y<=46)return Close;
 if(x>=305&&x<344&&y<=48)return Windows;
 if(x>=216&&y>=78&&y<=109)return Connect;
 if(!connected&&x<=211&&y>=88&&y<=165)return Bluetooth;
 if(x>=216&&y>=112&&y<=162)return Battery;
 if(y>=181&&y<=215&&x>=146)return x<255?Natural:Standard;
 if(y>=249&&y<=275)return Momentum;
 if(y>=307&&y<=333)return ScrollSpeed;
 if(y>=365&&y<=391)return PointerSpeed;
 if(y>=399&&y<=436)return Acceleration;
 if(y>=444&&y<=494)return Tap;
 return None;
}
inline float SliderFraction(float x){return x<=SliderLeft?0:x>=SliderRight?1:(x-SliderLeft)/(SliderRight-SliderLeft);}
}
