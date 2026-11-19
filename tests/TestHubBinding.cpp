#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#include <cstdint>
#include "../src/MouseUsbTransport.h"
int wmain(int argc,wchar_t** argv){if(argc!=2)return 64;adapterHub=argv[1];constexpr GUID hubClass{0xf18a0e88,0xc30c,0x11d0,{0x88,0x15,0x00,0xa0,0xc9,0x06,0xbe,0xd8}};ULONG count=0;if(CM_Get_Device_Interface_List_SizeW(&count,const_cast<GUID*>(&hubClass),nullptr,CM_GET_DEVICE_INTERFACE_LIST_PRESENT)!=CR_SUCCESS)return 1;std::vector<wchar_t> paths(count);if(CM_Get_Device_Interface_ListW(const_cast<GUID*>(&hubClass),nullptr,paths.data(),count,CM_GET_DEVICE_INTERFACE_LIST_PRESENT)!=CR_SUCCESS)return 2;unsigned matched=0;for(auto path=paths.data();*path;path+=wcslen(path)+1)if(SameHub(path))matched++;printf("Exact selected-hub interface matches=%u (metadata only; no capture)\n",matched);return matched==1?0:3;}
