#pragma once
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <string>
#include <algorithm>
#include <cfgmgr32.h>
#include <optional>
#include <vector>
#include "MouseConnectionPipe.h"
namespace MouseSettings {
inline std::wstring BootId(){
 struct Boot {GUID id;ULONG firmware;ULONGLONG flags;};Boot info{};
 using Query=LONG(WINAPI*)(ULONG,PVOID,ULONG,PULONG);
 auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQuerySystemInformation"));
 if(!query||query(90,&info,sizeof(info),nullptr)<0)return {};
 wchar_t text[64];swprintf_s(text,L"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",info.id.Data1,info.id.Data2,info.id.Data3,info.id.Data4[0],info.id.Data4[1],info.id.Data4[2],info.id.Data4[3],info.id.Data4[4],info.id.Data4[5],info.id.Data4[6],info.id.Data4[7]);return text;
}
inline std::wstring Read(const std::wstring& file,const wchar_t* key,const wchar_t* fallback=L""){wchar_t value[256];GetPrivateProfileStringW(L"Mouse",key,fallback,value,256,file.c_str());return value;}
inline std::optional<bool> NativeNatural(const std::wstring& file){
 auto address=Read(file,L"Address");if(address.size()!=12)return std::nullopt;std::transform(address.begin(),address.end(),address.begin(),towupper);
 constexpr wchar_t mouseClass[]=L"{4d36e96f-e325-11ce-bfc1-08002be10318}";constexpr ULONG flags=CM_GETIDLIST_FILTER_CLASS|CM_GETIDLIST_FILTER_PRESENT;ULONG size=0;
 if(CM_Get_Device_ID_List_SizeW(&size,mouseClass,flags)!=CR_SUCCESS||size<2)return std::nullopt;std::vector<wchar_t> list(size);if(CM_Get_Device_ID_ListW(mouseClass,list.data(),size,flags)!=CR_SUCCESS)return std::nullopt;
 std::optional<bool> result;
 for(auto id=list.data();*id;id+=wcslen(id)+1){DEVINST node=0;if(CM_Locate_DevNodeW(&node,id,CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS)continue;bool belongs=false;
  for(int depth=0;depth<12;depth++){wchar_t parentId[MAX_DEVICE_ID_LEN]{};if(CM_Get_Device_IDW(node,parentId,MAX_DEVICE_ID_LEN,0)!=CR_SUCCESS)break;std::wstring upper=parentId;std::transform(upper.begin(),upper.end(),upper.begin(),towupper);if(upper.rfind(L"BTHENUM\\{00001124-",0)==0&&upper.find(L"PID&0323")!=std::wstring::npos&&upper.find(address+L"_C")!=std::wstring::npos){belongs=true;break;}DEVINST parent=0;if(CM_Get_Parent(&parent,node,0)!=CR_SUCCESS)break;node=parent;}
  if(!belongs)continue;std::wstring path=L"SYSTEM\\CurrentControlSet\\Enum\\"+std::wstring(id)+L"\\Device Parameters";DWORD value=0,bytes=sizeof(value);LONG error=RegGetValueW(HKEY_LOCAL_MACHINE,path.c_str(),L"FlipFlopWheel",RRF_RT_REG_DWORD,nullptr,&value,&bytes);if(error!=ERROR_SUCCESS&&error!=ERROR_FILE_NOT_FOUND)continue;bool natural=value!=0;if(result&&*result!=natural)return std::nullopt;result=natural;
 }return result;
}
inline bool Natural(const std::wstring& file){
 auto pending=Read(file,L"DirectionChangedBoot"),boot=BootId();
 bool natural=Read(file,L"Natural",L"1")==L"1";
 if(!pending.empty()&&!boot.empty()&&pending!=boot){natural=NativeNatural(file).value_or(Read(file,L"RequestedNatural",L"1")==L"1");if(WritePrivateProfileStringW(L"Mouse",L"Natural",natural?L"1":L"0",file.c_str()))WritePrivateProfileStringW(L"Mouse",L"DirectionChangedBoot",nullptr,file.c_str());}
 else if(pending.empty()){auto native=NativeNatural(file);if(native&&*native!=natural){natural=*native;WritePrivateProfileStringW(L"Mouse",L"Natural",natural?L"1":L"0",file.c_str());}}
 return natural;
}
struct Values {int momentum=38,pointer=10;UINT lines=3;bool acceleration=false,natural=true,pending=false;};
inline Values Get(const std::wstring& file){Values value;value.momentum=std::clamp<int>(GetPrivateProfileIntW(L"Mouse",L"Momentum",38,file.c_str()),0,100);SystemParametersInfoW(SPI_GETMOUSESPEED,0,&value.pointer,0);SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&value.lines,0);int mouse[3]{};SystemParametersInfoW(SPI_GETMOUSE,0,mouse,0);value.acceleration=mouse[2]!=0;value.natural=Natural(file);value.pending=!Read(file,L"DirectionChangedBoot").empty();return value;}
inline bool Momentum(const std::wstring& file,int value){return WritePrivateProfileStringW(L"Mouse",L"Momentum",std::to_wstring(std::clamp(value,0,100)).c_str(),file.c_str())!=FALSE;}
inline bool Pointer(int speed){return SystemParametersInfoW(SPI_SETMOUSESPEED,0,reinterpret_cast<PVOID>(INT_PTR(std::clamp(speed,1,20))),SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)!=FALSE;}
inline bool Lines(UINT lines){return SystemParametersInfoW(SPI_SETWHEELSCROLLLINES,std::clamp(lines,1u,100u),nullptr,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)!=FALSE;}
inline bool Acceleration(bool enabled){int mouse[3]{};if(!SystemParametersInfoW(SPI_GETMOUSE,0,mouse,0))return false;if(enabled&&mouse[0]==0&&mouse[1]==0){mouse[0]=6;mouse[1]=10;}mouse[2]=enabled?1:0;return SystemParametersInfoW(SPI_SETMOUSE,0,mouse,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)!=FALSE;}
inline bool Direction(HWND owner,bool natural){
 (void)owner;DWORD result=ConnectionPipe::Command(natural?3:4);if(result!=ERROR_SUCCESS){SetLastError(result);return false;}
 wchar_t local[MAX_PATH];if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH))return false;std::wstring file=std::wstring(local)+L"\\IslandMagicMouse\\settings.ini";
 WritePrivateProfileStringW(L"Mouse",L"PreferredNatural",natural?L"1":L"0",file.c_str());
 bool saved=WritePrivateProfileStringW(L"Mouse",L"Natural",natural?L"1":L"0",file.c_str())!=FALSE;
 WritePrivateProfileStringW(L"Mouse",L"RequestedNatural",nullptr,file.c_str());WritePrivateProfileStringW(L"Mouse",L"DirectionChangedBoot",nullptr,file.c_str());return saved;
}
}
