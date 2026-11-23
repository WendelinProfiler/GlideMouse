#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "Protocol.h"
#include "MagicMouseTray.h"
#include "MouseUsbShared.h"
#include "MouseSettings.h"
#include "DirectMousePointer.h"
#include "MagicMouseEngine.h"
static unsigned long long eventCounts[9]{},sendFailures=0,frameCount=0,sessionStarts=0,readRetries=0,statusRetries=0;static DWORD lastInputError=0;static double maxLoopGapMs=0;static std::wstring statusPath,settingsPath;static bool tapMode=false,signedUsb=false;
static void WriteStatus(const char* mode,bool coasting=false){
 if(statusPath.empty())return;WritePrivateProfileStringW(L"Mouse",L"InputMode",std::wstring(mode,mode+strlen(mode)).c_str(),settingsPath.c_str());WritePrivateProfileStringW(L"Mouse",L"InputHeartbeat",std::to_wstring(GetTickCount64()).c_str(),settingsPath.c_str());HANDLE f=CreateFileW(statusPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return;
 char json[1024];int n=snprintf(json,sizeof(json),"{\"mode\":\"%s\",\"pid\":%lu,\"frames\":%llu,\"coasting\":%s,\"passiveDriver\":true,\"transport\":\"%s\",\"maxLoopGapMs\":%.3f,\"wheel\":%llu,\"back\":%llu,\"forward\":%llu,\"desktopPrevious\":%llu,\"desktopNext\":%llu,\"zoom\":%llu,\"overview\":%llu,\"leftTap\":%llu,\"rightTap\":%llu,\"tapToClick\":%s,\"sendFailures\":%llu,\"lastInputError\":%lu,\"sessionStarts\":%llu,\"readRetries\":%llu,\"statusRetries\":%llu}",mode,(unsigned long)GetCurrentProcessId(),frameCount,coasting?"true":"false","signed-usb",maxLoopGapMs,eventCounts[0],eventCounts[1],eventCounts[2],eventCounts[3],eventCounts[4],eventCounts[5],eventCounts[6],eventCounts[7],eventCounts[8],tapMode?"true":"false",sendFailures,(unsigned long)lastInputError,sessionStarts,readRetries,statusRetries);DWORD wrote=0;if(n>0&&n<int(sizeof(json)))WriteFile(f,json,DWORD(n),&wrote,nullptr);CloseHandle(f);
}
static void Inject(UINT count,INPUT* input){if(SendInput(count,input,sizeof(INPUT))!=count){sendFailures++;lastInputError=GetLastError();}}
static void EmitPointer(const DirectMousePointer::Delta& delta){
 if(!delta.Any())return;INPUT input{};input.type=INPUT_MOUSE;input.mi.dwExtraInfo=0x494d5431;
 input.mi.dx=delta.x;input.mi.dy=delta.y;if(delta.x||delta.y)input.mi.dwFlags|=MOUSEEVENTF_MOVE;
 if(delta.down&1)input.mi.dwFlags|=MOUSEEVENTF_LEFTDOWN;if(delta.up&1)input.mi.dwFlags|=MOUSEEVENTF_LEFTUP;
 if(delta.down&2)input.mi.dwFlags|=MOUSEEVENTF_RIGHTDOWN;if(delta.up&2)input.mi.dwFlags|=MOUSEEVENTF_RIGHTUP;
 Inject(1,&input);
}
static void KeyChord(std::initializer_list<WORD> keys){
 for(WORD modifier:{WORD(VK_SHIFT),WORD(VK_CONTROL),WORD(VK_MENU),WORD(VK_LWIN),WORD(VK_RWIN)})if(GetAsyncKeyState(modifier)&0x8000)return;
 std::vector<INPUT> input;for(WORD key:keys){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=key;input.push_back(i);}
 for(auto it=keys.end();it!=keys.begin();){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=*--it;i.ki.dwFlags=KEYEVENTF_KEYUP;input.push_back(i);}
 UINT sent=SendInput(UINT(input.size()),input.data(),sizeof(INPUT));if(sent!=input.size()){sendFailures++;lastInputError=GetLastError();}
 if(sent!=input.size()){std::vector<INPUT> release;for(WORD key:keys){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=key;i.ki.dwFlags=KEYEVENTF_KEYUP;release.push_back(i);}SendInput(UINT(release.size()),release.data(),sizeof(INPUT));}
}
static void Emit(const std::vector<MagicMouse::Event>& events,double& remainderX,double& remainderY){
 for(auto e:events){eventCounts[int(e.action)]++;switch(e.action){
  case MagicMouse::Action::Wheel:{remainderX+=e.x;remainderY+=e.y;LONG x=LONG(std::trunc(remainderX)),y=LONG(std::trunc(remainderY));remainderX-=x;remainderY-=y;INPUT i{};i.type=INPUT_MOUSE;i.mi.dwExtraInfo=0x494d5431;if(y){i.mi.dwFlags=MOUSEEVENTF_WHEEL;i.mi.mouseData=DWORD(y);Inject(1,&i);}if(x){i.mi.dwFlags=MOUSEEVENTF_HWHEEL;i.mi.mouseData=DWORD(x);Inject(1,&i);}break;}
  case MagicMouse::Action::LeftClick:case MagicMouse::Action::RightClick:{INPUT input[2]{};for(auto& i:input){i.type=INPUT_MOUSE;i.mi.dwExtraInfo=0x494d5431;}bool right=e.action==MagicMouse::Action::RightClick;input[0].mi.dwFlags=right?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=right?MOUSEEVENTF_RIGHTUP:MOUSEEVENTF_LEFTUP;Inject(2,input);break;}
  case MagicMouse::Action::PageBack:case MagicMouse::Action::PageForward:{DWORD_PTR ignored=0;HWND target=GetForegroundWindow();if(target)SendMessageTimeoutW(target,WM_APPCOMMAND,WPARAM(target),MAKELPARAM(0,(e.action==MagicMouse::Action::PageBack?APPCOMMAND_BROWSER_BACKWARD:APPCOMMAND_BROWSER_FORWARD)),SMTO_ABORTIFHUNG,50,&ignored);break;}
  case MagicMouse::Action::DesktopPrevious:KeyChord({VK_LWIN,VK_CONTROL,VK_LEFT});break;
  case MagicMouse::Action::DesktopNext:KeyChord({VK_LWIN,VK_CONTROL,VK_RIGHT});break;
  case MagicMouse::Action::WindowOverview:KeyChord({VK_LWIN,VK_TAB});break;
  case MagicMouse::Action::SmartZoom:{// Standard Windows app zoom, not macOS's semantic Smart Zoom.
   static HWND zoomWindow=nullptr;static bool zoomed=false;HWND foreground=GetForegroundWindow();if(foreground!=zoomWindow){zoomWindow=foreground;zoomed=false;}
   if(zoomed)KeyChord({VK_CONTROL,WORD('0')});else KeyChord({VK_CONTROL,VK_OEM_PLUS});zoomed=!zoomed;break;}
 }}
}
static int RunSession(bool observe,HANDLE stop){
 // Hidden per-user input component: no settings window, tray icon or UI required.
 // A Session-0 Windows service cannot correctly SendInput to the user's desktop.
 signedUsb=true;
 MouseUsbReader usb;
 if(!usb.Open(wcstoull(MagicMouseTray::Read(L"Address",L"0").c_str(),nullptr,16)))return 2;
 auto statusQuery=[&](IMT_STATUS& status,DWORD& bytes){bytes=sizeof(status);return usb.Status(status);};
 auto readQuery=[&](IMT_READ& query,IMT_BATCH& batch,DWORD& bytes){return usb.Read(query,batch,bytes);};
 IMT_STATUS status{};DWORD size=0;if(!statusQuery(status,size)||size!=sizeof(status)||status.version!=IMT_VERSION||status.qpcFrequency<=0){return 3;}
 // Read-only handle: this component cannot acquire the kernel takeover lease.
 // Apple's pointer, mechanical buttons and finger-wheel path stay untouched.
 auto preferred=MouseSettings::Read(settingsPath,L"PreferredNatural",L"1");auto native=MouseSettings::NativeNatural(settingsPath);bool wanted=preferred!=L"0";
 if(!native||*native!=wanted)MouseSettings::Direction(nullptr,wanted);
 bool directPointer=GetPrivateProfileIntW(L"Mouse",L"DirectPointer",0,settingsPath.c_str())!=0;DirectMousePointer::State pointer;
 sessionStarts++;MagicMouse::Engine engine;tapMode=GetPrivateProfileIntW(L"Mouse",L"TapToClick",0,settingsPath.c_str())!=0;engine.settings.tapToClick=tapMode;int momentumStrength=std::clamp<int>(GetPrivateProfileIntW(L"Mouse",L"Momentum",38,settingsPath.c_str()),0,100);engine.settings.momentum=momentumStrength>0;engine.settings.decay=.08+.52*momentumStrength/100.;engine.settings.natural=MouseSettings::Natural(settingsPath);bool waitForLift=false;IMT_READ read{IMT_VERSION,0,status.frames};double remainderX=0,remainderY=0;LARGE_INTEGER last{},now{};QueryPerformanceCounter(&last);ULONGLONG heartbeat=GetTickCount64(),start=heartbeat,lastStatus=0;ULONGLONG lastHealthyRead=heartbeat,lastHealthyStatus=heartbeat;unsigned disconnects=0;WriteStatus(observe?"observe":"active");
 while(WaitForSingleObject(stop,0)==WAIT_TIMEOUT&&GetTickCount64()-start< (observe?30000ULL:UINT64_MAX)){
  IMT_BATCH batch{};
  if(!readQuery(read,batch,size)){readRetries++;if(GetTickCount64()-lastHealthyRead>500)break;Sleep(2);continue;}lastHealthyRead=GetTickCount64();
  if(batch.version!=IMT_VERSION||batch.count>IMT_BATCH_SIZE||size!=offsetof(IMT_BATCH,records)+batch.count*sizeof(IMT_RECORD))break;
  if(batch.missed){if(directPointer&&!observe)EmitPointer(pointer.Release());waitForLift=true;engine.Reset();remainderX=remainderY=0;}
  for(unsigned i=0;i<batch.count;i++){
   auto& record=batch.records[i];if(record.sequence<=read.afterSequence||record.length>IMT_MAX_REPORT)continue;read.afterSequence=record.sequence;if(directPointer&&!status.devices)continue;
   MagicMouse::Frame frame;if(MagicMouse::Parse(record.bytes,record.length,double(record.qpc)/status.qpcFrequency,frame)){frameCount++;if(directPointer&&!observe)EmitPointer(pointer.Feed(frame));if(waitForLift){if(!frame.hasContacts||!frame.contacts.empty())continue;waitForLift=false;}auto events=engine.Feed(frame);if(!directPointer)std::erase_if(events,[](const auto& event){return event.action==MagicMouse::Action::Wheel;});if(!observe)Emit(events,remainderX,remainderY);}
  }
  QueryPerformanceCounter(&now);maxLoopGapMs=std::max(maxLoopGapMs,1000.*double(now.QuadPart-last.QuadPart)/status.qpcFrequency);if(!observe)Emit(engine.Tick(double(now.QuadPart-last.QuadPart)/status.qpcFrequency),remainderX,remainderY);last=now;
  if(GetTickCount64()-heartbeat>=80){
   IMT_STATUS nextStatus{};
   if(!statusQuery(nextStatus,size)){statusRetries++;if(GetTickCount64()-lastHealthyStatus>500)break;heartbeat=GetTickCount64();continue;}
   if(nextStatus.version!=IMT_VERSION||nextStatus.qpcFrequency<=0)break;
   status=nextStatus;lastHealthyStatus=GetTickCount64();
   if(!status.devices){if(directPointer&&!observe)EmitPointer(pointer.Release());engine.Reset();remainderX=remainderY=0;if(++disconnects>100)break;}else disconnects=0;
   heartbeat=GetTickCount64();
  }
  if(GetTickCount64()-lastStatus>=500){momentumStrength=std::clamp<int>(GetPrivateProfileIntW(L"Mouse",L"Momentum",38,settingsPath.c_str()),0,100);engine.settings.momentum=momentumStrength>0;engine.settings.decay=.08+.52*momentumStrength/100.;bool actualNatural=MouseSettings::Natural(settingsPath);if(engine.settings.natural!=actualNatural){engine.Reset();engine.settings.natural=actualNatural;engine.settings.tapToClick=tapMode;remainderX=remainderY=0;waitForLift=true;}bool configured=GetPrivateProfileIntW(L"Mouse",L"TapToClick",0,settingsPath.c_str())!=0;if(configured!=tapMode){tapMode=configured;engine.Reset();engine.settings.tapToClick=tapMode;remainderX=remainderY=0;waitForLift=true;}WriteStatus(observe?"observe":"active",engine.Coasting());lastStatus=GetTickCount64();}
  Sleep(8);
 }
 if(directPointer&&!observe)EmitPointer(pointer.Release());
 // There is no takeover lease or modified native report in this input path.
 return 0;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR command,int){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 wchar_t local[MAX_PATH];if(GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH)){auto folder=std::wstring(local)+L"\\IslandMagicMouse";CreateDirectoryW(folder.c_str(),nullptr);statusPath=folder+L"\\input-status.json";settingsPath=folder+L"\\settings.ini";}
 constexpr wchar_t StopName[]=L"Local\\IslandMagicInput.Stop";
 if(wcsstr(command,L"--stop")){HANDLE stop=OpenEventW(EVENT_MODIFY_STATE,FALSE,StopName);if(stop){SetEvent(stop);CloseHandle(stop);}return 0;}
 bool observe=wcsstr(command,L"--observe")!=nullptr;if(!observe&&!wcsstr(command,L"--run"))return 64;
 HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\IslandMagicInput.Singleton");if(!singleton)return 5;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(singleton);return 0;}
 HANDLE stop=CreateEventW(nullptr,TRUE,FALSE,StopName);if(!stop){CloseHandle(singleton);return 5;}
 auto trayThread=MagicMouseTray::Start(stop,settingsPath);
 int result=0;
 do{result=RunSession(observe,stop);if(observe||result==4)break;WriteStatus("waiting-for-driver");}while(WaitForSingleObject(stop,1000)==WAIT_TIMEOUT);
 WriteStatus(result==4?"lease-unavailable":"stopped");SetEvent(stop);trayThread.join();CloseHandle(stop);CloseHandle(singleton);return result;
}

