#pragma once
#include <windows.h>
#include <winioctl.h>
#include <usbioctl.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cfgmgr32.h>
#include <devpkey.h>
#include "../third_party/USBPcap.h"
static uint64_t Target=0;
static USHORT adapterVendor=0,adapterProduct=0;
static ULONG adapterPort=0;
static std::wstring adapterHub;
static bool SameHub(std::wstring path){
 if(adapterHub.empty())return false;
 if(path.rfind(L"\\??\\",0)==0)path=L"\\\\?\\"+path.substr(4);
 constexpr DEVPROPKEY instanceKey{{0x78c34fc8,0x104a,0x4aca,{0x9e,0xa4,0x52,0x4d,0x52,0x99,0x6e,0x57}},256};wchar_t id[2048]{};ULONG bytes=sizeof(id);DEVPROPTYPE type=0;
 return CM_Get_Device_Interface_PropertyW(path.c_str(),&instanceKey,&type,reinterpret_cast<BYTE*>(id),&bytes,0)==CR_SUCCESS&&type==DEVPROP_TYPE_STRING&&_wcsicmp(id,adapterHub.c_str())==0;
}
struct Transport {std::wstring path,hub;USHORT address=0;};
static bool Io(HANDLE h,DWORD code,void* in,DWORD n,void* out,DWORD cap,DWORD& got){return DeviceIoControl(h,code,in,n,out,cap,&got,nullptr)!=FALSE;}
static bool describe=false;
static USHORT Adapter(const std::wstring& path){HANDLE hub=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(hub==INVALID_HANDLE_VALUE)return 0;std::vector<BYTE> memory(sizeof(USB_NODE_CONNECTION_INFORMATION_EX)+32*sizeof(USB_PIPE_INFO));auto* info=reinterpret_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(memory.data());info->ConnectionIndex=adapterPort;DWORD got=0;BOOL ok=DeviceIoControl(hub,IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,info,DWORD(memory.size()),info,DWORD(memory.size()),&got,nullptr);USHORT address=ok&&info->ConnectionStatus==DeviceConnected&&info->DeviceDescriptor.idVendor==adapterVendor&&info->DeviceDescriptor.idProduct==adapterProduct?info->DeviceAddress:0;if(describe&&address){printf("ADAPTER vendor=%04x product=%04x address=%u pipes=%lu\n",info->DeviceDescriptor.idVendor,info->DeviceDescriptor.idProduct,address,info->NumberOfOpenPipes);for(unsigned i=0;i<std::min<unsigned>(info->NumberOfOpenPipes,32);i++){auto& p=info->PipeList[i].EndpointDescriptor;printf("ENDPOINT address=%02x transfer=%u packetSize=%u interval=%u\n",p.bEndpointAddress,p.bmAttributes&3,p.wMaxPacketSize,p.bInterval);}}CloseHandle(hub);return address;}
static Transport Locate(){Transport result;for(int n=1;n<=64;n++){std::wstring path=L"\\\\.\\USBPcap"+std::to_wstring(n);HANDLE file=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)continue;wchar_t hub[2048]{};DWORD got=0;bool ok=Io(file,IOCTL_USBPCAP_GET_HUB_SYMLINK,nullptr,0,hub,sizeof(hub),got);CloseHandle(file);if(!ok||got<4)continue;hub[2047]=0;if(!SameHub(hub))continue;USHORT address=Adapter(hub);if(address){if(result.address)return {};result={path,hub,address};}}return result;}

