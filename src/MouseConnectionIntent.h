#pragma once
// An explicit release is held only until an explicit connect or service restart.
// This does not disable the HID profile, delete pairing or alter device drivers.
class MouseConnectionIntent {
 bool released=false;
public:
 void RequestConnect(){released=false;}
 void RequestDisconnect(){released=true;}
 bool ShouldRelease(bool connected)const{return released&&connected;}
};
