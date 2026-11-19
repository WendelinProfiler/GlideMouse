#pragma once
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <cmath>
#include <string>
#include <memory>
#include <algorithm>
#include "MouseSettings.h"
#include "MouseWidgetLayout.h"
#include "MouseConnection.h"
namespace GlidePopup {
inline HWND window=nullptr,owner=nullptr;
inline std::wstring profile,name,message;
inline ULONGLONG dismissed=0;
inline bool connected=false,fresh=false;
inline int battery=-1,dragging=-1;
inline float drawScale=1;
inline MouseSettings::Values controls;
inline ULONGLONG lastRead=0;
inline std::unique_ptr<Gdiplus::Bitmap> product;
inline bool productChecked=false;
inline void DrawProduct(Gdiplus::Graphics& g){if(!productChecked){wchar_t module[MAX_PATH];GetModuleFileNameW(nullptr,module,MAX_PATH);std::wstring path=module;path=path.substr(0,path.find_last_of(L"\\")+1)+L"MagicMouse-animation.png";product.reset(Gdiplus::Bitmap::FromFile(path.c_str()));productChecked=true;}if(product&&product->GetLastStatus()==Gdiplus::Ok&&product->GetWidth()==3456&&product->GetHeight()==1920){UINT frame=UINT(GetTickCount64()/83)%48;g.DrawImage(product.get(),Gdiplus::RectF(20,79,170,71),float(frame%6)*576,float(frame/6)*240,576.f,240.f,Gdiplus::UnitPixel);}}
inline int Setting(const wchar_t* key,int fallback=0){return GetPrivateProfileIntW(L"Mouse",key,fallback,profile.c_str());}
inline void Text(Gdiplus::Graphics& g,const std::wstring& text,float x,float y,float w,float h,float size,Gdiplus::Color color,bool bold=false){Gdiplus::Font font(L"Segoe UI",size,bold?Gdiplus::FontStyleBold:Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);Gdiplus::SolidBrush brush(color);Gdiplus::StringFormat format;format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);g.DrawString(text.c_str(),-1,&font,Gdiplus::RectF(x,y,w,h),&format,&brush);}
inline void Card(Gdiplus::Graphics& g,float x,float y,float w,float h,Gdiplus::Color color){using namespace Gdiplus;GraphicsPath p;float d=12;p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();SolidBrush brush(color);g.FillPath(&brush,&p);}
inline void Slider(Gdiplus::Graphics& g,float y,float f){using namespace Gdiplus;Pen track(Color(255,62,70,82),4.f);track.SetStartCap(LineCapRound);track.SetEndCap(LineCapRound);g.DrawLine(&track,34.f,y,356.f,y);Pen fill(Color(255,239,247,255),4.f);g.DrawLine(&fill,34.f,y,34.f+322*f,y);SolidBrush knob(Color(255,239,247,255));g.FillEllipse(&knob,28.f+322*f,y-6.f,12.f,12.f);}
inline void Switch(Gdiplus::Graphics& g,float y,bool on){using namespace Gdiplus;SolidBrush track(on?Color(255,10,132,255):Color(255,61,70,84));g.FillEllipse(&track,316.f,y,24.f,24.f);g.FillEllipse(&track,336.f,y,24.f,24.f);g.FillRectangle(&track,328.f,y,20.f,24.f);SolidBrush knob(Color(255,245,248,253));g.FillEllipse(&knob,on?340.f:320.f,y+4.f,16.f,16.f);}
inline void Draw(Gdiplus::Graphics& g){using namespace Gdiplus;g.SetSmoothingMode(SmoothingModeAntiAlias);g.Clear(Color(255,21,24,30));Color white(255,241,245,251),muted(255,149,161,179),blue(255,35,184,255);
 if(dragging<0&&GetTickCount64()-lastRead>=500){controls=MouseSettings::Get(profile);lastRead=GetTickCount64();}
 Text(g,L"Magic Mouse",22,19,280,30,21,white,true);Text(g,name.empty()?L"GlideMouse controls":name,22,51,326,22,12,muted);Text(g,L"\u2699",312,16,28,32,20,muted);Text(g,L"\u00d7",350,16,28,32,24,muted);
 if(connected)DrawProduct(g);else Text(g,L"1  Switch the mouse off, then on.\n2  Disconnect it from your Mac.\n3  Pair in Windows Bluetooth.",22,91,192,72,10,muted);
 Card(g,220,81,146,25,connected?Color(255,43,48,57):Color(255,28,57,77));Text(g,MouseConnection::Busy(profile)?L"Please wait...":connected?L"Disconnect":L"Connect",250,85,118,23,12,connected?white:blue);
 auto color=battery<0?muted:battery<=10?Color(255,255,98,90):battery<=20?Color(255,255,204,72):Color(255,104,214,154);Text(g,battery<0?L"Battery unavailable":std::to_wstring(battery)+L"%",220,113,138,33,battery<0?13:26,color,true);Text(g,fresh?L"Battery \u00b7 current":L"Battery \u00b7 last known",220,146,148,20,10,muted);
 Card(g,16,176,358,101,Color(255,30,34,41));Card(g,16,284,358,152,Color(255,30,34,41));Card(g,16,444,358,50,Color(255,30,34,41));
 Text(g,L"Scroll direction",32,188,114,24,13,white);Card(g,146,182,216,30,Color(255,37,42,51));Card(g,controls.natural?148.f:255.f,184,105,26,Color(255,64,72,85));Text(g,L"Natural",174,187,80,22,12,controls.natural?white:muted);Text(g,L"Standard",275,187,85,22,12,controls.natural?muted:white);
 Text(g,L"Momentum",32,232,260,24,13,white);Text(g,controls.momentum?std::to_wstring(controls.momentum)+L"%":L"Off",317,234,48,24,12,muted);Slider(g,262,controls.momentum/100.f);
 Text(g,L"Scroll speed",32,290,240,24,13,white);Text(g,controls.lines==WHEEL_PAGESCROLL?L"One screen":std::to_wstring(controls.lines)+L" lines",299,292,67,24,12,muted);Slider(g,320,(std::clamp(controls.lines,1u,20u)-1)/19.f);
 Text(g,L"Tracking speed",32,348,240,24,13,white);Text(g,std::to_wstring(int(std::round((controls.pointer-1)*100./19.)))+L"%",317,350,48,24,12,muted);Slider(g,378,(controls.pointer-1)/19.f);
 Text(g,L"Pointer acceleration",32,406,272,24,13,white);Switch(g,403,controls.acceleration);
 Text(g,L"Tap to click",32,450,260,24,13,white);bool tap=Setting(L"TapToClick")!=0;Text(g,tap?L"Physical clicks always work":L"Off \u00b7 double-tap zooms instead",32,476,300,18,10,muted);Switch(g,451,tap);
 auto heartbeat=wcstoull(MouseSettings::Read(profile,L"InputHeartbeat",L"0").c_str(),nullptr,10),now=GetTickCount64();bool ready=MouseSettings::Read(profile,L"InputMode")==L"active"&&now>=heartbeat&&now-heartbeat<2500;Text(g,message.empty()?(ready?L"Connected \u00b7 Ready":L"Preparing mouse controls..."):message,24,519,342,22,10,message.empty()?muted:Color(255,255,204,72));
}

inline void Paint(HWND hwnd,HDC target=nullptr){using namespace Gdiplus;PAINTSTRUCT ps{};HDC dc=target?target:BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);Bitmap image(std::max(1L,r.right),std::max(1L,r.bottom),PixelFormat32bppARGB);Graphics g(&image);g.ScaleTransform(drawScale,drawScale);Draw(g);Graphics out(dc);out.DrawImage(&image,0,0);if(!target)EndPaint(hwnd,&ps);}
inline void Drag(float x,bool commit){float f=MouseWidgetLayout::SliderFraction(x);bool ok=true;if(dragging==MouseWidgetLayout::Momentum){controls.momentum=int(std::round(f*100));if(commit)ok=MouseSettings::Momentum(profile,controls.momentum);}if(dragging==MouseWidgetLayout::ScrollSpeed){controls.lines=1+UINT(std::round(f*19));if(commit)ok=MouseSettings::Lines(controls.lines);}if(dragging==MouseWidgetLayout::PointerSpeed){controls.pointer=1+int(std::round(f*19));if(commit)ok=MouseSettings::Pointer(controls.pointer);}if(commit)message=ok?L"":L"The setting could not be updated";}
inline void Activate(MouseWidgetLayout::Control action){using namespace MouseWidgetLayout;bool ok=true;if(action==Close)ShowWindow(window,SW_HIDE);else if(action==Natural||action==Standard)ok=MouseSettings::Direction(window,action==Natural);else if(action==Tap)ok=WritePrivateProfileStringW(L"Mouse",L"TapToClick",Setting(L"TapToClick")?L"0":L"1",profile.c_str())!=FALSE;else if(action==Acceleration)ok=MouseSettings::Acceleration(!controls.acceleration);else if(action==Connect){if(!MouseConnection::Busy(profile))ok=MouseConnection::Start(window,!connected);}else if(action==Windows||action==Bluetooth)ok=reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",action==Bluetooth?L"ms-settings:bluetooth":L"ms-settings:mousetouchpad",nullptr,nullptr,SW_SHOWNORMAL))>32;else if(action==Battery&&owner)PostMessageW(owner,WM_APP+32,0,0);message=ok?L"":L"The setting could not be updated";InvalidateRect(window,nullptr,FALSE);}
inline LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){using namespace MouseWidgetLayout;
 if(msg==WM_PRINTCLIENT){Paint(hwnd,reinterpret_cast<HDC>(wp));return 0;}if(msg==WM_PAINT){Paint(hwnd);return 0;}if(msg==WM_ERASEBKGND)return 1;if(msg==WM_TIMER){if(wp==1||(connected&&IsWindowVisible(hwnd)))InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 if(msg==WM_CLOSE||(msg==WM_KEYDOWN&&wp==VK_ESCAPE)){dragging=-1;ReleaseCapture();ShowWindow(hwnd,SW_HIDE);return 0;}if(msg==WM_ACTIVATE&&LOWORD(wp)==WA_INACTIVE){dragging=-1;ReleaseCapture();dismissed=GetTickCount64();ShowWindow(hwnd,SW_HIDE);return 0;}
 if(msg==WM_KEYDOWN&&wp==VK_SPACE){Activate(Tap);return 0;}
 if(msg==WM_LBUTTONDOWN){auto action=Hit(GET_X_LPARAM(lp)/drawScale,GET_Y_LPARAM(lp)/drawScale,connected);if(action==Momentum||action==ScrollSpeed||action==PointerSpeed){dragging=action;SetCapture(hwnd);Drag(GET_X_LPARAM(lp)/drawScale,false);InvalidateRect(hwnd,nullptr,FALSE);}return 0;}
 if(msg==WM_MOUSEMOVE&&dragging>=0){Drag(GET_X_LPARAM(lp)/drawScale,false);InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(msg==WM_CAPTURECHANGED){dragging=-1;return 0;}
 if(msg==WM_LBUTTONUP){if(dragging>=0){Drag(GET_X_LPARAM(lp)/drawScale,true);dragging=-1;ReleaseCapture();}else Activate(Hit(GET_X_LPARAM(lp)/drawScale,GET_Y_LPARAM(lp)/drawScale,connected));InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 return DefWindowProcW(hwnd,msg,wp,lp);
}
inline void Toggle(HWND parent,POINT anchor,const std::wstring& settings,bool online,int percent,bool current,const std::wstring& deviceName){using namespace MouseWidgetLayout;profile=settings;owner=parent;connected=online;battery=percent;fresh=current;name=deviceName;
 if(!window){WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=Proc;cls.lpszClassName=L"GlideMousePopup";cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));RegisterClassW(&cls);window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,cls.lpszClassName,L"GlideMouse",WS_POPUP,0,0,390,548,parent,nullptr,cls.hInstance,nullptr);if(!window)return;BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));DWORD round=2;DwmSetWindowAttribute(window,33,&round,sizeof(round));SetTimer(window,1,1000,nullptr);SetTimer(window,2,83,nullptr);}
 if(IsWindowVisible(window)||GetTickCount64()-dismissed<250){ShowWindow(window,SW_HIDE);return;}auto monitor=MonitorFromPoint(anchor,MONITOR_DEFAULTTONEAREST);MONITORINFO info{};info.cbSize=sizeof(info);GetMonitorInfoW(monitor,&info);UINT dx=96,dy=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dx,&dy);RECT work=info.rcWork;drawScale=std::min({dx/96.f,(work.right-work.left-16)/Width,(work.bottom-work.top-16)/Height});int width=int(Width*drawScale),height=int(Height*drawScale),gap=8;
 int x=std::clamp(int(anchor.x)-width/2,int(work.left)+gap,int(work.right)-gap-width);int y=anchor.y>work.top+(work.bottom-work.top)/2?int(anchor.y)-gap-height:int(anchor.y)+gap;y=std::clamp(y,int(work.top)+gap,int(work.bottom)-gap-height);SetWindowPos(window,HWND_TOPMOST,x,y,width,height,SWP_SHOWWINDOW);SetForegroundWindow(window);InvalidateRect(window,nullptr,FALSE);
}
inline void Destroy(){if(window){DestroyWindow(window);window=nullptr;}product.reset();productChecked=false;}
}

