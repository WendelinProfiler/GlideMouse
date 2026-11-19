#pragma once
#include <windows.h>
#include <shellapi.h>
#include <bluetoothapis.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <cmath>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include "MousePopup.h"
namespace MagicMouseTray {
struct State {bool connected=false,fresh=false;int battery=-1;};
inline std::wstring config;inline HANDLE stopEvent=nullptr;inline NOTIFYICONDATAW notification{};inline HICON icon=nullptr;inline State previous;inline bool added=false;
inline bool previousPopup=false;
inline UINT callback=WM_APP+31,taskbarCreated=0;
inline std::wstring Read(const wchar_t* key,const wchar_t* fallback=L""){wchar_t value[256];GetPrivateProfileStringW(L"Mouse",key,fallback,value,256,config.c_str());return value;}
inline unsigned long long NowFileTime(){FILETIME file;GetSystemTimeAsFileTime(&file);ULARGE_INTEGER value;value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;return value.QuadPart;}
inline int NativeBattery(unsigned long long address){
 if(!address)return -1;wchar_t mac[20];swprintf_s(mac,L"%012llX",address);
 constexpr DEVPROPKEY key{{0x104ea319,0x6ee2,0x4701,{0xbd,0x47,0x8d,0xdb,0xf4,0x25,0xbb,0xe5}},2};ULONG flags=CM_GETIDLIST_FILTER_ENUMERATOR|CM_GETIDLIST_FILTER_PRESENT,size=0;
 if(CM_Get_Device_ID_List_SizeW(&size,L"BTHENUM",flags)!=CR_SUCCESS||size<2)return -1;
 std::vector<wchar_t> list(size);if(CM_Get_Device_ID_ListW(L"BTHENUM",list.data(),size,flags)!=CR_SUCCESS)return -1;
 for(auto* id=list.data();*id;id+=wcslen(id)+1){std::wstring upper=id;std::transform(upper.begin(),upper.end(),upper.begin(),towupper);if(upper.find(mac)==std::wstring::npos)continue;DEVINST node=0;DEVPROPTYPE type=0;BYTE value=255;ULONG bytes=1;if(CM_Locate_DevNodeW(&node,id,CM_LOCATE_DEVNODE_NORMAL)==CR_SUCCESS&&CM_Get_DevNode_PropertyW(node,&key,&type,&value,&bytes,0)==CR_SUCCESS&&type==DEVPROP_TYPE_BYTE&&value<=100)return value;}
 return -1;
}
inline State Query(){
 State state;auto address=wcstoull(Read(L"Address",L"0").c_str(),nullptr,16);BLUETOOTH_DEVICE_INFO device{};device.dwSize=sizeof(device);device.Address.ullLong=address;state.connected=address&&BluetoothGetDeviceInfo(nullptr,&device)==ERROR_SUCCESS&&device.fConnected;
 int native=state.connected?NativeBattery(address):-1;auto text=Read(L"Battery");if(!text.empty())state.battery=_wtoi(text.c_str());if(state.battery<0||state.battery>100)state.battery=-1;
 auto stamp=wcstoull(Read(L"BatteryTime",L"0").c_str(),nullptr,10),now=NowFileTime();
 if(native>=0){state.battery=native;state.fresh=true;if(!stamp||now<stamp||now-stamp>300000000ULL){WritePrivateProfileStringW(L"Mouse",L"Battery",std::to_wstring(native).c_str(),config.c_str());WritePrivateProfileStringW(L"Mouse",L"BatteryTime",std::to_wstring(now).c_str(),config.c_str());}}
 else state.fresh=state.connected&&stamp&&now>=stamp&&now-stamp<=3000000000ULL;
 return state;
}
inline Gdiplus::Color BatteryColor(const State& state){
 if(state.battery<0||!state.connected)return {150,146,151,159};BYTE alpha=state.fresh?255:145;
 return state.battery<=10?Gdiplus::Color(alpha,255,98,90):state.battery<=20?Gdiplus::Color(alpha,255,204,72):Gdiplus::Color(alpha,104,214,154);
}
inline void Draw(Gdiplus::Bitmap& bitmap,const State& state){
 using namespace Gdiplus;Graphics g(&bitmap);g.SetSmoothingMode(SmoothingModeAntiAlias);g.Clear(Color(0,0,0,0));
 GraphicsPath mouse;mouse.StartFigure();mouse.AddBezier(64.f,7.f,39.f,7.f,30.f,19.f,30.f,43.f);mouse.AddBezier(30.f,43.f,30.f,64.f,33.f,90.f,39.f,104.f);mouse.AddBezier(39.f,104.f,44.f,117.f,53.f,121.f,64.f,121.f);mouse.AddBezier(64.f,121.f,75.f,121.f,84.f,117.f,89.f,104.f);mouse.AddBezier(89.f,104.f,95.f,90.f,98.f,64.f,98.f,43.f);mouse.AddBezier(98.f,43.f,98.f,19.f,89.f,7.f,64.f,7.f);mouse.CloseFigure();
 LinearGradientBrush body(Point(30,7),Point(98,121),state.connected?Color(100,0,174,255):Color(55,145,151,160),Color(8,120,160,200));g.FillPath(&body,&mouse);
 Pen outline(state.connected?Color(255,0,174,255):Color(255,145,151,160),9.f);outline.SetLineJoin(LineJoinRound);g.DrawPath(&outline,&mouse);
}

inline void Update(HWND window,bool force=false){
 State state=Query();GlidePopup::connected=state.connected;GlidePopup::battery=state.battery;GlidePopup::fresh=state.fresh;bool popup=IsWindowVisible(FindWindowW(L"GlideMousePopup",nullptr))||IsWindowVisible(FindWindowW(L"WindhawkMacIslandDockPanel",L"Island Magic Mouse"));if(!force&&added&&popup==previousPopup&&state.connected==previous.connected&&state.battery==previous.battery&&state.fresh==previous.fresh)return;
 Gdiplus::Bitmap bitmap(128,128,PixelFormat32bppARGB);Draw(bitmap,state);HICON next=nullptr;if(bitmap.GetHICON(&next)!=Gdiplus::Ok||!next)return;
 notification.cbSize=sizeof(notification);notification.hWnd=window;notification.uID=1;notification.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;notification.uCallbackMessage=callback;notification.hIcon=next;
 std::wstring tip=L"Magic Mouse";if(state.connected){BLUETOOTH_DEVICE_INFO device{};device.dwSize=sizeof(device);device.Address.ullLong=wcstoull(Read(L"Address",L"0").c_str(),nullptr,16);if(BluetoothGetDeviceInfo(nullptr,&device)==ERROR_SUCCESS&&device.szName[0])tip+=L"\n"+std::wstring(device.szName);tip+=state.battery>=0?L"\nBattery: "+std::to_wstring(state.battery)+L" %"+(state.fresh?L"":L" (last known)"):L"\nBattery: unavailable";}else tip+=L"\nDisconnected";wcsncpy_s(notification.szTip,popup?L"Magic Mouse":tip.c_str(),_TRUNCATE);
 bool success=Shell_NotifyIconW(added?NIM_MODIFY:NIM_ADD,&notification)!=FALSE;
 if(success){if(!added){notification.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&notification);}added=true;previous=state;previousPopup=popup;if(icon)DestroyIcon(icon);icon=next;}else DestroyIcon(next);
}
inline void OpenWidget(POINT anchor){
 struct NativeSearch {POINT point;HWND window=nullptr;};NativeSearch native{anchor};
 EnumWindows([](HWND w,LPARAM data)->BOOL{auto& match=*reinterpret_cast<NativeSearch*>(data);if(!GetPropW(w,L"IslandMagicMouseNativePanel"))return TRUE;RECT r{};GetWindowRect(w,&r);if(!match.window)match.window=w;if(match.point.x>=r.left&&match.point.x<=r.right){match.window=w;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&native));
 if(native.window){PostMessageW(native.window,WM_APP+191,0,0);return;}

 State state=Query();BLUETOOTH_DEVICE_INFO device{};device.dwSize=sizeof(device);device.Address.ullLong=wcstoull(Read(L"Address",L"0").c_str(),nullptr,16);BluetoothGetDeviceInfo(nullptr,&device);
 GlidePopup::Toggle(notification.hWnd,anchor,config,state.connected,state.battery,state.fresh,device.szName);

 wcscpy_s(notification.szTip,L"Magic Mouse");notification.uFlags=NIF_TIP;Shell_NotifyIconW(NIM_MODIFY,&notification);
}

inline void OpenWidget(){POINT anchor{};GetCursorPos(&anchor);OpenWidget(anchor);}
inline LRESULT CALLBACK WindowProc(HWND window,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==taskbarCreated){added=false;Update(window,true);return 0;}
 if(msg==WM_CREATE){SetTimer(window,1,2000,nullptr);Update(window,true);return 0;}
 if(msg==WM_TIMER){Update(window);return 0;}if(msg==WM_APP+32){Update(window,true);return 0;}
 if(msg==callback){UINT action=LOWORD(lp);if(action==NIN_SELECT||action==NIN_KEYSELECT){POINT anchor{short(LOWORD(wp)),short(HIWORD(wp))};if(action==NIN_KEYSELECT)GetCursorPos(&anchor);OpenWidget(anchor);return 0;}if(action==WM_CONTEXTMENU||action==WM_RBUTTONUP){POINT point;GetCursorPos(&point);HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Mouse settings");bool tap=Read(L"TapToClick",L"0")==L"1";AppendMenuW(menu,MF_STRING|(tap?MF_CHECKED:0),2,L"Tap to click");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,3,L"Quit GlideMouse");SetForegroundWindow(window);int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);DestroyMenu(menu);if(command==1)OpenWidget();else if(command==2)WritePrivateProfileStringW(L"Mouse",L"TapToClick",tap?L"0":L"1",config.c_str());else if(command==3)SetEvent(stopEvent);PostMessageW(window,WM_NULL,0,0);return 0;}}
 if(msg==WM_DESTROY){GlidePopup::Destroy();Shell_NotifyIconW(NIM_DELETE,&notification);added=false;if(icon){DestroyIcon(icon);icon=nullptr;}return 0;}
 return DefWindowProcW(window,msg,wp,lp);
}
inline std::thread Start(HANDLE stop,const std::wstring& profile){return std::thread([stop,profile]{
 config=profile;stopEvent=stop;ULONG_PTR gdiplus=0;Gdiplus::GdiplusStartupInput startup;if(Gdiplus::GdiplusStartup(&gdiplus,&startup,nullptr)!=Gdiplus::Ok)return;
 taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=WindowProc;cls.lpszClassName=L"IslandMagicMouseInputTray";RegisterClassW(&cls);HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"Magic Mouse Input",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);
 if(window){bool done=false;while(!done){DWORD wait=MsgWaitForMultipleObjects(1,&stop,FALSE,INFINITE,QS_ALLINPUT);if(wait==WAIT_OBJECT_0)break;MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){done=true;break;}TranslateMessage(&message);DispatchMessageW(&message);}}DestroyWindow(window);}Gdiplus::GdiplusShutdown(gdiplus);
 });}
}
