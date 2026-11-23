#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bluetoothapis.h>
#include <shellapi.h>
#include <string>
#include "MouseConnectionPipe.h"
#include "MouseConnectionState.h"
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR argument,int){
 bool connect=wcscmp(argument,L"--connect")==0;if(!connect&&wcscmp(argument,L"--disconnect")!=0)return 64;
 HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\GlideMouse.ConnectionCommand");if(!mutex)return 1;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);return 0;}
 wchar_t local[MAX_PATH];if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH)){CloseHandle(mutex);return 2;}std::wstring file=std::wstring(local)+L"\\IslandMagicMouse\\settings.ini";
 wchar_t text[32];GetPrivateProfileStringW(L"Mouse",L"Address",L"0",text,32,file.c_str());auto address=wcstoull(text,nullptr,16);if(!address||address>0xffffffffffffULL){CloseHandle(mutex);return 3;}
 auto save=[&](const wchar_t* key,const std::wstring& value){WritePrivateProfileStringW(L"Mouse",key,value.c_str(),file.c_str());};
 save(L"ConnectionStarted",std::to_wstring(GetTickCount64()));save(L"ConnectionBusy",L"1");save(L"ConnectionMessage",connect?L"Connecting...":L"Disconnecting...");
 if(connect&&GetPrivateProfileIntW(L"Mouse",L"HostSwitchEnabled",0,file.c_str())!=0){
  std::wstring agent=std::wstring(local)+L"\\GlideMouseSwitch\\app\\GlideMouseSwitch.exe";
  std::wstring command=L"\""+agent+L"\" take";STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
  bool started=CreateProcessW(agent.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE;
  DWORD exitCode=ERROR_FILE_NOT_FOUND;
  if(started){DWORD waited=WaitForSingleObject(process.hProcess,180000);if(waited==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&exitCode);else exitCode=ERROR_TIMEOUT;CloseHandle(process.hThread);CloseHandle(process.hProcess);}
  BLUETOOTH_DEVICE_INFO switched{};switched.dwSize=sizeof(switched);
  bool ready=exitCode==0&&MouseConnectionState::Query(address,switched)==ERROR_SUCCESS&&switched.fConnected&&ConnectionPipe::Command(5)==ERROR_SUCCESS;
  save(L"ConnectionBusy",L"0");save(L"ConnectionMessage",ready?L"Connected":started?L"The mouse switch did not finish. Open GlideMouse Switch for details.":L"Open GlideMouse Switch to finish setting up host switching.");
  CloseHandle(mutex);return ready?0:int(exitCode?exitCode:ERROR_DEVICE_NOT_AVAILABLE);
 }
 BLUETOOTH_DEVICE_INFO info{};info.dwSize=sizeof(info);info.Address.ullLong=address;DWORD query=MouseConnectionState::Query(address,info);DWORD result=ERROR_NOT_FOUND;bool matched=false;
 if(query==ERROR_SUCCESS&&info.fAuthenticated){
  {result=ConnectionPipe::Request(connect);if(result==ERROR_SUCCESS||result==ERROR_SERVICE_ALREADY_RUNNING){for(int i=0;i<60;i++){Sleep(125);info.dwSize=sizeof(info);if(MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&bool(info.fConnected)==connect){matched=true;break;}}}}
 }
 if(connect&&matched){result=ConnectionPipe::Command(5);matched=result==ERROR_SUCCESS;}
 save(L"ConnectionBusy",L"0");save(L"ConnectionMessage",matched?(connect?L"Connected":L"Disconnected"):(connect?(result==ERROR_SUCCESS_REBOOT_REQUIRED?L"GlideMouse driver activation requires a Windows restart. Pairing is preserved.":L"GlideMouse is not ready. If the mouse is connected to your Mac, disconnect it there and try again. Pairing has not been changed."):L"Could not disconnect. Open Windows Bluetooth settings."));CloseHandle(mutex);return matched?0:int(result?result:ERROR_TIMEOUT);
}
