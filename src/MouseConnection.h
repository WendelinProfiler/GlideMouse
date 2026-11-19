#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
namespace MouseConnection {
inline bool Busy(const std::wstring& file){auto stamp=wcstoull(MouseSettings::Read(file,L"ConnectionStarted",L"0").c_str(),nullptr,10);auto now=GetTickCount64();return MouseSettings::Read(file,L"ConnectionBusy")==L"1"&&now>=stamp&&now-stamp<15000;}
inline bool Start(HWND owner,bool connect){wchar_t module[MAX_PATH];GetModuleFileNameW(nullptr,module,MAX_PATH);std::wstring path=module;path=path.substr(0,path.find_last_of(L"\\")+1)+L"GlideMouseConnection.exe";if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES){wchar_t folder[MAX_PATH];if(!GetEnvironmentVariableW(L"ProgramW6432",folder,MAX_PATH)&&!GetEnvironmentVariableW(L"ProgramFiles",folder,MAX_PATH))return false;path=std::wstring(folder)+L"\\IslandMagicMouse\\GlideMouseConnection.exe";}return reinterpret_cast<INT_PTR>(ShellExecuteW(owner,L"open",path.c_str(),connect?L"--connect":L"--disconnect",nullptr,SW_HIDE))>32;}
}
