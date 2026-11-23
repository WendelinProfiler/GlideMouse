#pragma once
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <setupapi.h>
#include <string>
#include <vector>
#include <algorithm>
namespace MouseDriverChain {
struct State {std::wstring instance;bool registered=false,active=false;};
// Podglow briefly exposes its physical radio for Intel firmware loading. Never
// attach the legacy Apple filter to that temporary twin: the virtual radio is
// the input owner. Standalone GlideMouse (no running Podglow service) is unchanged.
inline bool NativePodglowTwin(const wchar_t* instance){
 SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);if(!scm)return false;
 SC_HANDLE service=OpenServiceW(scm,L"WPodsService",SERVICE_QUERY_STATUS);SERVICE_STATUS_PROCESS state{};DWORD bytes=0;
 bool managed=service&&QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<BYTE*>(&state),sizeof(state),&bytes)&&state.dwCurrentState==SERVICE_RUNNING;
 if(service)CloseServiceHandle(service);CloseServiceHandle(scm);if(!managed)return false;
 wchar_t physical[512]{};DWORD size=sizeof(physical);if(RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WPods\\Service",L"RadioInstanceId",RRF_RT_REG_SZ,nullptr,physical,&size)!=ERROR_SUCCESS)return false;
 DEVINST node=0;if(CM_Locate_DevNodeW(&node,const_cast<wchar_t*>(instance),CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS)return false;
 for(unsigned i=0;i<10;++i){wchar_t id[MAX_DEVICE_ID_LEN]{};if(CM_Get_Device_IDW(node,id,_countof(id),0)==CR_SUCCESS&&_wcsicmp(id,physical)==0)return true;DEVINST parent=0;if(CM_Get_Parent(&parent,node,0)!=CR_SUCCESS)break;node=parent;}
 return false;
}
inline bool Contains(const std::vector<wchar_t>& list,const wchar_t* wanted){for(auto p=list.data();p&&*p;p+=wcslen(p)+1)if(_wcsicmp(p,wanted)==0)return true;return false;}
inline State Query(unsigned long long target){
 State state;wchar_t address[16];swprintf_s(address,L"%012llX",target);ULONG length=0;constexpr ULONG flags=CM_GETIDLIST_FILTER_ENUMERATOR|CM_GETIDLIST_FILTER_PRESENT;
 if(CM_Get_Device_ID_List_SizeW(&length,L"BTHENUM",flags)!=CR_SUCCESS||length<2)return state;std::vector<wchar_t> ids(length+2);
 if(CM_Get_Device_ID_ListW(L"BTHENUM",ids.data(),length,flags)!=CR_SUCCESS)return state;
 for(auto id=ids.data();*id;id+=wcslen(id)+1){std::wstring upper=id;std::transform(upper.begin(),upper.end(),upper.begin(),towupper);if(upper.rfind(L"BTHENUM\\{00001124-",0)!=0||upper.find(L"PID&0323")==std::wstring::npos||upper.find(std::wstring(address)+L"_C")==std::wstring::npos)continue;if(NativePodglowTwin(id))continue;if(!state.instance.empty())return {};state.instance=id;DEVINST node=0;if(CM_Locate_DevNodeW(&node,id,CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS)continue;
  if(NativePodglowTwin(id)){state={};continue;}
  auto property=[&](const DEVPROPKEY& key){DEVPROPTYPE type=0;ULONG size=0;CM_Get_DevNode_PropertyW(node,&key,&type,nullptr,&size,0);std::vector<wchar_t> data(size/sizeof(wchar_t)+2);if(CM_Get_DevNode_PropertyW(node,&key,&type,reinterpret_cast<PBYTE>(data.data()),&size,0)!=CR_SUCCESS||type!=DEVPROP_TYPE_STRING_LIST)data.assign(2,0);return data;};
  state.registered=Contains(property(DEVPKEY_Device_LowerFilters),L"applewirelessmouse");state.active=Contains(property(DEVPKEY_Device_Stack),L"\\Driver\\applewirelessmouse");
 }return state;
}
// Reapply only this mouse's registered filter. No unpair, profile toggle,
// adapter reset, test-signing change, global class filter or Windows reboot.
inline DWORD RestoreRegistration(unsigned long long target){
 auto state=Query(target);if(state.instance.empty())return ERROR_NOT_FOUND;if(state.active)return ERROR_SUCCESS;
 HDEVINFO set=SetupDiCreateDeviceInfoList(nullptr,nullptr);if(set==INVALID_HANDLE_VALUE)return GetLastError();SP_DEVINFO_DATA info{sizeof(info)};DWORD result=ERROR_SUCCESS;
 if(!SetupDiOpenDeviceInfoW(set,state.instance.c_str(),nullptr,0,&info)){result=GetLastError();SetupDiDestroyDeviceInfoList(set);return result;}
 if(!state.registered){DWORD type=0,size=0;SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_LOWERFILTERS,&type,nullptr,0,&size);std::vector<wchar_t> old(size/sizeof(wchar_t)+2);if(size&&!SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_LOWERFILTERS,&type,reinterpret_cast<PBYTE>(old.data()),DWORD(old.size()*sizeof(wchar_t)),nullptr)){result=GetLastError();}else{std::vector<wchar_t> filters;for(auto p=old.data();*p;p+=wcslen(p)+1)filters.insert(filters.end(),p,p+wcslen(p)+1);constexpr wchar_t apple[]=L"applewirelessmouse";filters.insert(filters.end(),apple,apple+_countof(apple));filters.push_back(0);if(!SetupDiSetDeviceRegistryPropertyW(set,&info,SPDRP_LOWERFILTERS,reinterpret_cast<PBYTE>(filters.data()),DWORD(filters.size()*sizeof(wchar_t))))result=GetLastError();}}
 if(result==ERROR_SUCCESS&&!SetupDiRestartDevices(set,&info))result=GetLastError();SetupDiDestroyDeviceInfoList(set);if(result!=ERROR_SUCCESS)return result;
 return Query(target).active?ERROR_SUCCESS:ERROR_SUCCESS_REBOOT_REQUIRED;
}
}
