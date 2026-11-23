#pragma once
#include <windows.h>
#include <bluetoothapis.h>
#include <functional>
#include "MouseConnectionPipe.h"

namespace MouseBondRenewalClient {
struct State {DWORD generation=0,stage=0,flags=0,error=0;};
inline DWORD Request(uint64_t target,BYTE command,State& state){
 HANDLE pipe=ConnectionPipe::OpenCommandPipe(L"\\\\.\\pipe\\GlideMouse.BondRenewal.v1");if(pipe==INVALID_HANDLE_VALUE)return GetLastError();
 BYTE request[16]{'G','M','R','1',command,0,0,0};memcpy(request+8,&target,8);BYTE response[32]{};DWORD got=0,result=ERROR_TIMEOUT;
 OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 bool ok=ConnectionPipe::Complete(pipe,ov,WriteFile(pipe,request,sizeof(request),nullptr,&ov),got,nullptr,2000);ResetEvent(ov.hEvent);
 if(ok&&got==sizeof(request)&&ConnectionPipe::Complete(pipe,ov,ReadFile(pipe,response,sizeof(response),nullptr,&ov),got,nullptr,3000)&&got==sizeof(response)&&memcmp(response,"GMR1",4)==0){memcpy(&result,response+4,4);memcpy(&state.generation,response+8,4);memcpy(&state.stage,response+12,4);memcpy(&state.flags,response+16,4);memcpy(&state.error,response+20,4);BYTE ack=0xA5;ResetEvent(ov.hEvent);ConnectionPipe::Complete(pipe,ov,WriteFile(pipe,&ack,1,nullptr,&ov),got,nullptr,500);}
 CloseHandle(ov.hEvent);CloseHandle(pipe);return result;
}
// Called only for the broker's configured mouse after an explicit local claim.
// It refreshes authentication without removing its Windows HID/PnP node.
inline DWORD Run(uint64_t target,HANDLE stop,const std::function<DWORD()>& disconnect,const std::function<DWORD()>& connect){
 // A live ACL would reuse its authenticated session without requesting a key.
 // Close only this link, keeping its pairing, HID node and filter untouched.
 DWORD closed=disconnect();
 if(closed!=ERROR_SUCCESS&&closed!=ERROR_NOT_CONNECTED)return closed;
 for(int attempt=0;attempt<40;++attempt){
  BLUETOOTH_DEVICE_INFO current{};current.dwSize=sizeof(current);
  if(MouseConnectionState::Query(target,current)!=ERROR_SUCCESS||!current.fConnected)break;
  if(attempt==39)return ERROR_BUSY;
  if(WaitForSingleObject(stop,100)!=WAIT_TIMEOUT)return ERROR_CANCELLED;
 }
 // Authentication confirmation belongs to the explicit per-user claim process.
 State state;DWORD result=Request(target,1,state);

 if(result==ERROR_SUCCESS){connect();ULONGLONG start=GetTickCount64();result=ERROR_TIMEOUT;while(WaitForSingleObject(stop,100)==WAIT_TIMEOUT&&GetTickCount64()-start<22000){DWORD query=Request(target,2,state);if(query!=ERROR_SUCCESS){result=query;break;}if(state.stage==3&&state.flags==7){result=ERROR_SUCCESS;break;}if(state.stage==4){result=state.error?state.error:ERROR_NOT_AUTHENTICATED;break;}}if(WaitForSingleObject(stop,0)!=WAIT_TIMEOUT)result=ERROR_CANCELLED;}
 return result;
}
}
