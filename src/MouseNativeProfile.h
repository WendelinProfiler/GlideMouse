#pragma once
#include <windows.h>
#include <bluetoothapis.h>
#include <cstdint>

namespace MouseNativeProfile {
// The privileged broker supplies its fixed target; callers cannot select an
// address, a profile, a driver or an executable through its command pipe.
inline DWORD Disable(uint64_t target) {
    BLUETOOTH_FIND_RADIO_PARAMS args{sizeof(args)};
    HANDLE radio=nullptr;
    auto find=BluetoothFindFirstRadio(&args,&radio);
    if(!find)return GetLastError();
    GUID hid{0x1124,0,0x1000,{0x80,0,0,0x80,0x5f,0x9b,0x34,0xfb}};
    DWORD result=ERROR_NOT_FOUND;
    do {
        BLUETOOTH_DEVICE_INFO info{};info.dwSize=sizeof(info);
        info.Address.ullLong=target;
        if(BluetoothGetDeviceInfo(radio,&info)==ERROR_SUCCESS&&info.fAuthenticated) {
            GUID services[32]{};DWORD count=32;
            result=BluetoothEnumerateInstalledServices(radio,&info,&count,services);
            if(result==ERROR_SUCCESS) {
                bool installed=false;
                for(DWORD i=0;i<count;i++)installed|=IsEqualGUID(services[i],hid)!=FALSE;
                result=installed?BluetoothSetServiceState(radio,&info,&hid,BLUETOOTH_SERVICE_DISABLE):ERROR_SUCCESS;
            } else if(result==ERROR_NOT_FOUND||result==ERROR_SERVICE_DOES_NOT_EXIST) {
                result=ERROR_SUCCESS;
            }
            CloseHandle(radio);radio=nullptr;break;
        }
        CloseHandle(radio);radio=nullptr;
    } while(BluetoothFindNextRadio(find,&radio));
    BluetoothFindRadioClose(find);
    return result;
}
}
