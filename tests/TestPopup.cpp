#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <string>
#include <algorithm>
#include <cstdio>
#include "../src/MousePopup.h"
int main(){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 1;
 POINT point{0,0};MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTOPRIMARY),&monitor);
 unsigned cases=0;
 for(bool bottom:{true,false}){
  point={monitor.rcWork.right-12,bottom?monitor.rcWork.bottom:monitor.rcWork.top};
  GlidePopup::dismissed=0;GlidePopup::Toggle(nullptr,point,L"",true,34,true,L"Magic Mouse");
  RECT r{};GetWindowRect(GlidePopup::window,&r);
  if(!IsWindowVisible(GlidePopup::window)||r.left<monitor.rcWork.left||r.top<monitor.rcWork.top||r.right>monitor.rcWork.right||r.bottom>monitor.rcWork.bottom)return 2;
  UpdateWindow(GlidePopup::window);GlidePopup::Toggle(nullptr,point,L"",true,34,true,L"Magic Mouse");if(IsWindowVisible(GlidePopup::window))return 3;
  GlidePopup::dismissed=0;GlidePopup::Toggle(nullptr,point,L"",false,-1,false,L"Magic Mouse");SendMessageW(GlidePopup::window,WM_KEYDOWN,VK_ESCAPE,0);if(IsWindowVisible(GlidePopup::window))return 4;cases++;
 }
 GlidePopup::Destroy();Gdiplus::GdiplusShutdown(token);
 printf("PASS standalone popup: %u taskbar edges, monitor bounds, toggle, Escape; no capture, settings changes or gesture injection\n",cases);return 0;
}
