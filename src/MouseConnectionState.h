#pragma once
#include <windows.h>
#include <bluetoothapis.h>
namespace MouseConnectionState {
inline DWORD Query(unsigned long long target,BLUETOOTH_DEVICE_INFO& output){
 BLUETOOTH_FIND_RADIO_PARAMS parameters{sizeof(parameters)};HANDLE radio=nullptr;auto find=BluetoothFindFirstRadio(&parameters,&radio);if(!find)return GetLastError();DWORD error=ERROR_NOT_FOUND;bool found=false;
 do{BLUETOOTH_DEVICE_INFO info{};info.dwSize=sizeof(info);info.Address.ullLong=target;DWORD current=BluetoothGetDeviceInfo(radio,&info);if(current==ERROR_SUCCESS){if(!found||info.fConnected){output=info;found=true;error=ERROR_SUCCESS;}if(info.fConnected){CloseHandle(radio);radio=nullptr;break;}}else if(!found)error=current;CloseHandle(radio);radio=nullptr;}while(BluetoothFindNextRadio(find,&radio));BluetoothFindRadioClose(find);return error;
}
}
