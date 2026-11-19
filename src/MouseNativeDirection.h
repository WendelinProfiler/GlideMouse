#pragma once
#include <windows.h>
#include <cfgmgr32.h>
#include <string>
#include <vector>
#include <algorithm>
namespace NativeDirection {
inline DWORD Apply(uint64_t target,bool natural){
 wchar_t mac[20];swprintf_s(mac,L"%012llX",static_cast<unsigned long long>(target));constexpr wchar_t mouseClass[]=L"{4d36e96f-e325-11ce-bfc1-08002be10318}";constexpr ULONG flags=CM_GETIDLIST_FILTER_CLASS|CM_GETIDLIST_FILTER_PRESENT;ULONG count=0;if(CM_Get_Device_ID_List_SizeW(&count,mouseClass,flags)!=CR_SUCCESS)return ERROR_NOT_FOUND;std::vector<wchar_t> ids(count);if(CM_Get_Device_ID_ListW(mouseClass,ids.data(),count,flags)!=CR_SUCCESS)return ERROR_NOT_FOUND;
 struct Device {DEVINST node;std::wstring key;};std::vector<Device> devices;
 for(auto id=ids.data();*id;id+=wcslen(id)+1){DEVINST node=0;if(CM_Locate_DevNodeW(&node,id,CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS)continue;DEVINST ancestor=node;bool belongs=false;for(int depth=0;depth<12;depth++){wchar_t text[MAX_DEVICE_ID_LEN]{};if(CM_Get_Device_IDW(ancestor,text,MAX_DEVICE_ID_LEN,0)!=CR_SUCCESS)break;std::wstring upper=text;std::transform(upper.begin(),upper.end(),upper.begin(),towupper);if(upper.rfind(L"BTHENUM\\{00001124-",0)==0&&upper.find(L"PID&0323")!=std::wstring::npos&&upper.find(std::wstring(mac)+L"_C")!=std::wstring::npos){belongs=true;break;}DEVINST parent=0;if(CM_Get_Parent(&parent,ancestor,0)!=CR_SUCCESS)break;ancestor=parent;}if(belongs)devices.push_back({node,L"SYSTEM\\CurrentControlSet\\Enum\\"+std::wstring(id)+L"\\Device Parameters"});}
 if(devices.size()!=1)return ERROR_NOT_FOUND;auto& device=devices[0];HKEY key=nullptr;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,device.key.c_str(),0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key)!=ERROR_SUCCESS)return ERROR_ACCESS_DENIED;DWORD before[2]{},desired=natural?1:0;bool existed[2]{};const wchar_t* names[]{L"FlipFlopWheel",L"FlipFlopHScroll"};for(int i=0;i<2;i++){DWORD bytes=sizeof(DWORD);existed[i]=RegGetValueW(key,nullptr,names[i],RRF_RT_REG_DWORD,nullptr,&before[i],&bytes)==ERROR_SUCCESS;}
 auto restore=[&]{for(int i=0;i<2;i++){if(existed[i])RegSetValueExW(key,names[i],0,REG_DWORD,reinterpret_cast<BYTE*>(&before[i]),sizeof(DWORD));else RegDeleteValueW(key,names[i]);}};
 for(int i=0;i<2;i++)if(RegSetValueExW(key,names[i],0,REG_DWORD,reinterpret_cast<BYTE*>(&desired),sizeof(desired))!=ERROR_SUCCESS){restore();RegCloseKey(key);return ERROR_ACCESS_DENIED;}
 // Rebuild only the mouse-class child, never the Bluetooth HID parent/profile.
 auto disabled=CM_Disable_DevNode(device.node,CM_DISABLE_UI_NOT_OK);if(disabled!=CR_SUCCESS){restore();RegCloseKey(key);return ERROR_NOT_SUPPORTED;}Sleep(100);auto enabled=CM_Enable_DevNode(device.node,0);if(enabled!=CR_SUCCESS){restore();CM_Enable_DevNode(device.node,0);RegCloseKey(key);return ERROR_GEN_FAILURE;}RegCloseKey(key);return ERROR_SUCCESS;
}
}

