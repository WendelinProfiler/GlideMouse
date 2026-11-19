#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
int wmain(int argc,wchar_t** argv){if(argc<3||argc>4)return 64;bool install=argc==4&&wcscmp(argv[3],L"--install")==0;bool restore=argc==3&&wcscmp(argv[2],L"--restore-default")==0;if(argc==4&&!install)return 64;std::wstring target=argv[1],inf=argv[2];std::wstring upper=target;std::transform(upper.begin(),upper.end(),upper.begin(),towupper);if(upper.rfind(L"BTHENUM\\{00001124-",0)!=0||upper.find(L"PID&0323")==std::wstring::npos)return 65;
 if(install){wchar_t staged[MAX_PATH]{};if(!SetupCopyOEMInfW(inf.c_str(),nullptr,SPOST_PATH,0,staged,MAX_PATH,nullptr,nullptr)){printf("Stage signed package error=%lu\n",GetLastError());return 8;}inf=staged;}
 HDEVINFO set=SetupDiCreateDeviceInfoList(nullptr,nullptr);if(set==INVALID_HANDLE_VALUE)return 1;SP_DEVINFO_DATA node{};node.cbSize=sizeof(node);if(!SetupDiOpenDeviceInfoW(set,target.c_str(),nullptr,0,&node)){printf("OpenDevice error=%lu\n",GetLastError());return 2;}
 if(restore){using Install=BOOL(WINAPI*)(HWND,HDEVINFO,PSP_DEVINFO_DATA,PSP_DRVINFO_DATA_W,DWORD,PBOOL);HMODULE library=LoadLibraryExW(L"newdev.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);auto apply=library?reinterpret_cast<Install>(GetProcAddress(library,"DiInstallDevice")):nullptr;if(!apply)return 7;BOOL reboot=FALSE;BOOL ok=apply(nullptr,set,&node,nullptr,0,&reboot);DWORD error=ok?0:GetLastError();printf("Restore default driver success=%d error=%lu needReboot=%d\n",ok,error,reboot);SetupDiDestroyDeviceInfoList(set);return ok?(reboot?3010:0):int(error);}
 SP_DEVINSTALL_PARAMS_W parameters{};parameters.cbSize=sizeof(parameters);if(!SetupDiGetDeviceInstallParamsW(set,&node,&parameters))return 3;parameters.Flags|=DI_ENUMSINGLEINF|DI_QUIETINSTALL;parameters.FlagsEx|=DI_FLAGSEX_ALLOWEXCLUDEDDRVS;wcsncpy_s(parameters.DriverPath,inf.c_str(),_TRUNCATE);if(!SetupDiSetDeviceInstallParamsW(set,&node,&parameters)){printf("SetParams error=%lu\n",GetLastError());return 4;}if(!SetupDiBuildDriverInfoList(set,&node,SPDIT_CLASSDRIVER)){printf("BuildList error=%lu\n",GetLastError());return 5;}
 SP_DRVINFO_DATA_W selected{};bool found=false;
 for(DWORD index=0;;index++){SP_DRVINFO_DATA_W driver{};driver.cbSize=sizeof(driver);if(!SetupDiEnumDriverInfoW(set,&node,SPDIT_CLASSDRIVER,index,&driver))break;DWORD needed=0;SetupDiGetDriverInfoDetailW(set,&node,&driver,nullptr,0,&needed);std::vector<BYTE> data(needed+sizeof(SP_DRVINFO_DETAIL_DATA_W));auto* detail=reinterpret_cast<SP_DRVINFO_DETAIL_DATA_W*>(data.data());detail->cbSize=sizeof(*detail);if(!SetupDiGetDriverInfoDetailW(set,&node,&driver,detail,DWORD(data.size()),nullptr))continue;wprintf(L"Driver node: %ls / %ls / %ls\n",driver.ProviderName,detail->SectionName,detail->HardwareID);std::wstring hardware=detail->HardwareID;if(std::wstring(detail->SectionName).rfind(L"AppleWirelessMouse",0)==0){selected=driver;found=true;}}
 if(!found){puts("No original Apple Magic Mouse 2 driver node found");return 6;}if(!install){puts("PLAN ONLY: specific original signed driver node found; no driver installed");SetupDiDestroyDeviceInfoList(set);return 0;}
 using Install=BOOL(WINAPI*)(HWND,HDEVINFO,PSP_DEVINFO_DATA,PSP_DRVINFO_DATA_W,DWORD,PBOOL);HMODULE library=LoadLibraryExW(L"newdev.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);auto apply=library?reinterpret_cast<Install>(GetProcAddress(library,"DiInstallDevice")):nullptr;if(!apply)return 7;BOOL reboot=FALSE;BOOL ok=apply(nullptr,set,&node,&selected,0,&reboot);DWORD error=ok?0:GetLastError();printf("DiInstallDevice success=%d error=%lu needReboot=%d\n",ok,error,reboot);SetupDiDestroyDeviceInfoList(set);return ok?(reboot?3010:0):int(error);
}
