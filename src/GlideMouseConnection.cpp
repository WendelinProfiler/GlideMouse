#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bluetoothapis.h>
#include <shellapi.h>
#include <string>
#include "MouseConnectionPipe.h"
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR argument,int){
 bool connect=wcscmp(argument,L"--connect")==0;if(!connect&&wcscmp(argument,L"--disconnect")!=0)return 64;
 HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\GlideMouse.ConnectionCommand");if(!mutex)return 1;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);return 0;}
 wchar_t local[MAX_PATH];if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH)){CloseHandle(mutex);return 2;}std::wstring file=std::wstring(local)+L"\\IslandMagicMouse\\settings.ini";
 wchar_t text[32];GetPrivateProfileStringW(L"Mouse",L"Address",L"0",text,32,file.c_str());auto address=wcstoull(text,nullptr,16);if(!address||address>0xffffffffffffULL){CloseHandle(mutex);return 3;}
 auto save=[&](const wchar_t* key,const std::wstring& value){WritePrivateProfileStringW(L"Mouse",key,value.c_str(),file.c_str());};
 save(L"ConnectionStarted",std::to_wstring(GetTickCount64()));save(L"ConnectionBusy",L"1");save(L"ConnectionMessage",connect?L"Connecting...":L"Disconnecting...");
 BLUETOOTH_DEVICE_INFO info{};info.dwSize=sizeof(info);info.Address.ullLong=address;DWORD query=BluetoothGetDeviceInfo(nullptr,&info);DWORD result=ERROR_NOT_FOUND;bool matched=false;
 if(query==ERROR_SUCCESS&&info.fAuthenticated){
  if(bool(info.fConnected)==connect){result=ERROR_SUCCESS;matched=true;}
  else{result=ConnectionPipe::Request(connect);if(result==ERROR_SUCCESS||result==ERROR_SERVICE_ALREADY_RUNNING){for(int i=0;i<60;i++){Sleep(125);info.dwSize=sizeof(info);if(BluetoothGetDeviceInfo(nullptr,&info)==ERROR_SUCCESS&&bool(info.fConnected)==connect){matched=true;break;}}}}
 }else if(connect)ShellExecuteW(nullptr,L"open",L"ms-settings:bluetooth",nullptr,nullptr,SW_SHOWNORMAL);
 save(L"ConnectionBusy",L"0");save(L"ConnectionMessage",matched?(connect?L"Connected":L"Disconnected"):(connect?L"Not connected. Switch the mouse off/on, disconnect it from your Mac, then pair in Windows.":L"Could not disconnect. Open Windows Bluetooth settings."));CloseHandle(mutex);return matched?0:int(result?result:ERROR_TIMEOUT);
}
