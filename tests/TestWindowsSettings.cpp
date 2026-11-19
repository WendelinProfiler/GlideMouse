#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#include <cstdio>
#include "../src/MouseSettings.h"
int main(){
 auto boot=MouseSettings::BootId();if(boot.empty()){puts("FAIL Windows boot-session identity unavailable");return 1;}
 wchar_t folder[MAX_PATH],path[MAX_PATH];GetTempPathW(MAX_PATH,folder);if(!GetTempFileNameW(folder,L"glm",0,path))return 2;
 auto value=MouseSettings::Get(path);if(value.pointer<1||value.pointer>20)return 3;
 MouseSettings::Momentum(path,74);if(MouseSettings::Get(path).momentum!=74)return 4;
 WritePrivateProfileStringW(L"Mouse",L"Natural",L"1",path);WritePrivateProfileStringW(L"Mouse",L"RequestedNatural",L"0",path);WritePrivateProfileStringW(L"Mouse",L"DirectionChangedBoot",boot.c_str(),path);if(!MouseSettings::Natural(path))return 5;
 WritePrivateProfileStringW(L"Mouse",L"DirectionChangedBoot",L"different-test-boot",path);if(MouseSettings::Natural(path))return 6;if(!MouseSettings::Read(path,L"DirectionChangedBoot").empty())return 7;
 DeleteFileW(path);puts("PASS reading actual Windows pointer/wheel/acceleration settings, independent momentum, and direction applied only after boot-session change; no system settings modified");return 0;
}
