#pragma once
// Disconnect holds ownership remotely. A local claim pauses background work
// without disconnecting the authentication handshake it is about to create.
class MouseConnectionIntent {
 enum class Mode { Automatic, Released, Claiming };
 Mode mode=Mode::Automatic;
public:
 void RequestConnect(){mode=Mode::Automatic;}
 void RequestDisconnect(){mode=Mode::Released;}
 void BeginClaim(){mode=Mode::Claiming;}
 bool IsClaiming()const{return mode==Mode::Claiming;}
 bool AutomaticAllowed()const{return mode==Mode::Automatic;}
 bool ShouldRelease(bool connected)const{return mode==Mode::Released&&connected;}
};
