#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <bluetoothapis.h>
#include <cstdio>
#include <mutex>
#include <memory>
#include "../../src/MouseConnectionPipe.h"
#include "../../src/MouseConnectionState.h"
#include "../../src/MouseUsbShared.h"
using namespace winrt;
using namespace Windows::Devices::Enumeration;
static void Phase(const char* phase);
class TargetAuthentication {
 uint64_t target;
 HBLUETOOTH_AUTHENTICATION_REGISTRATION registration=nullptr;
 static BOOL CALLBACK Confirm(void* context,PBLUETOOTH_AUTHENTICATION_CALLBACK_PARAMS request){
  auto& self=*static_cast<TargetAuthentication*>(context);
  if(request->deviceInfo.Address.ullLong!=self.target)return FALSE;
  BLUETOOTH_AUTHENTICATE_RESPONSE response{};response.bthAddressRemote=request->deviceInfo.Address;response.authMethod=request->authenticationMethod;
  DWORD result=ERROR_NOT_SUPPORTED;
  bool supported=true;
  if(request->authenticationMethod==BLUETOOTH_AUTHENTICATION_METHOD_NUMERIC_COMPARISON)response.numericCompInfo.NumericValue=request->Numeric_Value;
  else if(request->authenticationMethod==BLUETOOTH_AUTHENTICATION_METHOD_LEGACY){memcpy(response.pinInfo.pin,"0000",4);response.pinInfo.pinLength=4;}
  else if(request->authenticationMethod==BLUETOOTH_AUTHENTICATION_METHOD_PASSKEY_NOTIFICATION)response.passkeyInfo.passkey=request->Passkey;
  else supported=false;
  if(supported)result=BluetoothSendAuthenticationResponseEx(nullptr,&response);
  wchar_t local[MAX_PATH]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);
  std::wstring log=std::wstring(local)+L"\\GlideMouseSwitch\\authentication-events.txt";FILE* file=nullptr;
  if(_wfopen_s(&file,log.c_str(),L"ab")==0){fprintf(file,"tick=%llu method=%u result=%lu\n",GetTickCount64(),unsigned(request->authenticationMethod),result);fclose(file);}
  return result==ERROR_SUCCESS;
 }
public:
 explicit TargetAuthentication(uint64_t address):target(address){}
 DWORD Start(){BLUETOOTH_DEVICE_INFO info{sizeof(info)};info.Address.ullLong=target;DWORD result=BluetoothRegisterForAuthenticationEx(&info,&registration,Confirm,this);Phase(result==ERROR_SUCCESS?"Exact mouse authentication callback registered":"Exact mouse authentication callback registration failed");return result;}
 ~TargetAuthentication(){if(registration)BluetoothUnregisterAuthentication(registration);}
};
template<class T> static bool Finished(T const& operation,DWORD milliseconds){ULONGLONG start=GetTickCount64();while(operation.Status()==Windows::Foundation::AsyncStatus::Started){if(GetTickCount64()-start>=milliseconds){operation.Cancel();return false;}Sleep(50);}return operation.Status()==Windows::Foundation::AsyncStatus::Completed;}
static DWORD PairTargetEndpoint(uint64_t address,bool renew=false){
 wchar_t local[MAX_PATH]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);std::wstring cache=std::wstring(local)+L"\\GlideMouseSwitch\\endpoint-id.txt";
 auto matches=[&](DeviceInformation const& device){if(!device)return false;auto properties=device.Properties();if(!properties.HasKey(L"System.Devices.Aep.DeviceAddress"))return false;auto raw=unbox_value_or<hstring>(properties.Lookup(L"System.Devices.Aep.DeviceAddress"),L"");std::wstring hex;for(auto c:raw)if(iswxdigit(c))hex+=c;return hex.size()==12&&wcstoull(hex.c_str(),nullptr,16)==address;};
 DeviceInformation selected{nullptr};FILE* saved=nullptr;if(_wfopen_s(&saved,cache.c_str(),L"rb")==0){wchar_t id[1024]{};fread(id,sizeof(wchar_t),1023,saved);fclose(saved);try{auto direct=DeviceInformation::CreateFromIdAsync(id,{L"System.Devices.Aep.DeviceAddress"},DeviceInformationKind::AssociationEndpoint);if(Finished(direct,3000)){auto item=direct.GetResults();if(matches(item))selected=item;}}catch(hresult_error const&){} }
 if(!selected){auto search=DeviceInformation::FindAllAsync(L"System.Devices.Aep.ProtocolId:=\"{E0CBF06C-CD8B-4647-BB8A-263B43F0F974}\"",{L"System.Devices.Aep.DeviceAddress"},DeviceInformationKind::AssociationEndpoint);if(!Finished(search,60000))return ERROR_TIMEOUT;for(auto const& item:search.GetResults())if(matches(item)){selected=item;if(item.Pairing().CanPair())break;}if(!selected)return ERROR_NOT_FOUND;if(_wfopen_s(&saved,cache.c_str(),L"wb")==0){auto id=selected.Id();fwrite(id.c_str(),sizeof(wchar_t),id.size(),saved);fclose(saved);}}
 char detail[100];sprintf_s(detail,"Target endpoint: paired=%d canPair=%d",selected.Pairing().IsPaired(),selected.Pairing().CanPair());Phase(detail);
 if(renew){
  auto unpair=selected.Pairing().UnpairAsync();
  if(!Finished(unpair,15000))return ERROR_TIMEOUT;
  auto status=unpair.GetResults().Status();sprintf_s(detail,"Windows association unpair result=%u",unsigned(status));Phase(detail);
  if(status!=DeviceUnpairingResultStatus::Unpaired&&status!=DeviceUnpairingResultStatus::AlreadyUnpaired)return ERROR_NOT_AUTHENTICATED;
  auto refresh=DeviceInformation::CreateFromIdAsync(selected.Id(),{L"System.Devices.Aep.DeviceAddress"},DeviceInformationKind::AssociationEndpoint);
  if(!Finished(refresh,5000))return ERROR_TIMEOUT;
  selected=refresh.GetResults();if(!matches(selected))return ERROR_NOT_FOUND;
 }
 auto custom=selected.Pairing().Custom();auto callback=custom.PairingRequested([](auto const&,DevicePairingRequestedEventArgs const& args){Phase("Windows requested confirmation for the configured mouse");if(args.PairingKind()==DevicePairingKinds::ConfirmOnly)args.Accept();else if(args.PairingKind()==DevicePairingKinds::ProvidePin)args.Accept(L"0000");});
 if(renew){
  // PairAsync owns the Windows security context. Arm physical renewal only
  // for that explicit pairing so a cached controller reply cannot reuse a
  // stale key, while SSP confirmation still belongs to this user operation.
  DWORD armed=ConnectionPipe::Command(12);
  if(armed){custom.PairingRequested(callback);return armed;}
  Phase("Physical renewal armed for the explicit Windows pairing");
 }
 auto ceremonies=renew?DevicePairingKinds::ConfirmOnly:(DevicePairingKinds::ConfirmOnly|DevicePairingKinds::ProvidePin);
 Phase(renew?"Use confirmation-only SSP for the configured mouse":"Use initial-pairing ceremonies");
 auto operation=custom.PairAsync(ceremonies,renew?DevicePairingProtectionLevel::Encryption:DevicePairingProtectionLevel::Default);bool completed=Finished(operation,45000);custom.PairingRequested(callback);if(!completed)return ERROR_TIMEOUT;auto status=operation.GetResults().Status();sprintf_s(detail,"Windows pairing result=%u",unsigned(status));Phase(detail);return status==DevicePairingResultStatus::Paired||status==DevicePairingResultStatus::AlreadyPaired?ERROR_SUCCESS:ERROR_NOT_AUTHENTICATED;
}static bool trace=false;
static void Phase(const char* phase){if(!trace)return;wchar_t local[MAX_PATH]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH))return;std::wstring path=std::wstring(local)+L"\\GlideMouseSwitch\\backend-phase.txt";FILE* file=nullptr;if(_wfopen_s(&file,path.c_str(),L"wb")==0){fprintf(file,"%llu %s\n",GetTickCount64(),phase);fclose(file);}path=std::wstring(local)+L"\\GlideMouseSwitch\\backend-events.txt";if(_wfopen_s(&file,path.c_str(),L"ab")==0){fprintf(file,"%llu %s\n",GetTickCount64(),phase);fclose(file);}}
static int Reply(bool ok,const char* message,bool connected=false,bool driver=false){
 printf("{\"Ok\":%s,\"Message\":\"%s\",\"Connected\":%s,\"DriverReady\":%s}\n",ok?"true":"false",message,connected?"true":"false",driver?"true":"false");return ok?0:1;
}
static bool InputReady(uint64_t address,const std::wstring& ini){
 MouseUsbReader reader;IMT_STATUS status{};
 if(!reader.Open(address)||!reader.Status(status)||!status.devices)return false;
 wchar_t mode[64]{},tick[32]{};
 GetPrivateProfileStringW(L"Mouse",L"InputMode",L"",mode,64,ini.c_str());
 GetPrivateProfileStringW(L"Mouse",L"InputHeartbeat",L"0",tick,32,ini.c_str());
 auto heartbeat=wcstoull(tick,nullptr,10),now=GetTickCount64();
 return wcscmp(mode,L"active")==0&&heartbeat&&now>=heartbeat&&now-heartbeat<2500;
}
static DWORD PairNativeEndpoint(uint64_t address){
 struct RadioScope {HANDLE radio=nullptr;HBLUETOOTH_RADIO_FIND find=nullptr;~RadioScope(){if(radio)CloseHandle(radio);if(find)BluetoothFindRadioClose(find);}} selected;
 BLUETOOTH_FIND_RADIO_PARAMS radios{sizeof(radios)};selected.find=BluetoothFindFirstRadio(&radios,&selected.radio);
 if(!selected.find)return GetLastError();
 BLUETOOTH_DEVICE_INFO info{sizeof(info)};info.Address.ullLong=address;
 DWORD cached=BluetoothGetDeviceInfo(selected.radio,&info);
 char metadata[100];sprintf_s(metadata,"Native device lookup result=%lu",cached);Phase(metadata);
 {
  // A remembered device is not proof of current reachability after host
  // handoff. Refresh discovery on explicit pairing even when cached.
  BLUETOOTH_DEVICE_SEARCH_PARAMS search{sizeof(search)};
  search.fReturnAuthenticated=search.fReturnRemembered=search.fReturnUnknown=search.fReturnConnected=TRUE;
  search.fIssueInquiry=TRUE;search.cTimeoutMultiplier=2;
  search.hRadio=selected.radio;
  BLUETOOTH_DEVICE_INFO candidate{sizeof(candidate)};
  auto find=BluetoothFindFirstDevice(&search,&candidate);bool found=false;
  if(find){do{if(candidate.Address.ullLong==address){info=candidate;found=true;break;}candidate={sizeof(candidate)};}while(BluetoothFindNextDevice(find,&candidate));BluetoothFindDeviceClose(find);}
  Phase(found?"Configured mouse present in discovery/cache enumeration":"Configured mouse absent from discovery/cache enumeration");
  // Bonded peripherals need not remain discoverable. An inquiry miss is
  // diagnostic only; still page the already configured address directly.
 }
 DWORD armed=ConnectionPipe::Command(12);if(armed)return armed;
 Phase("Explicit native Windows authentication with the configured mouse callback");
 Phase("Request dedicated Windows bonding for the explicit mouse claim");
 DWORD result=BluetoothAuthenticateDeviceEx(nullptr,selected.radio,&info,nullptr,MITMProtectionNotRequiredBonding);
 char detail[100];sprintf_s(detail,"Native Windows authentication result=%lu",result);Phase(detail);
 return result;
}
static int TakeWithFreshPairing(uint64_t address,const std::wstring& ini){
 DWORD quiet=ConnectionPipe::Command(7);if(quiet)return Reply(false,"Could not reserve the GlideMouse claim");
 Phase("Retire only the configured mouse pairing through Windows");
 BLUETOOTH_ADDRESS target{};target.ullLong=address;
 DWORD removed=BluetoothRemoveDevice(&target);
 if(removed!=ERROR_SUCCESS&&removed!=ERROR_NOT_FOUND)return Reply(false,"Windows could not retire the stale mouse pairing");
 for(int wait=0;wait<30;wait++){BLUETOOTH_DEVICE_INFO info{sizeof(info)};if(MouseConnectionState::Query(address,info)!=ERROR_SUCCESS||!info.fAuthenticated)break;Sleep(100);if(wait==29)return Reply(false,"Windows has not retired the old authentication record");}
 Phase("Pair the configured mouse with an explicit user-session pairing handler");
 bool native=GetPrivateProfileIntW(L"Mouse",L"HostSwitchNativePairing",0,ini.c_str())!=0;
 std::unique_ptr<TargetAuthentication> authentication;
 if(native){authentication=std::make_unique<TargetAuthentication>(address);DWORD registered=authentication->Start();if(registered)return Reply(false,"Could not register the explicit mouse authentication handler");}
 DWORD paired=native?PairNativeEndpoint(address):PairTargetEndpoint(address,true);
 if(paired){char message[150];sprintf_s(message,"Windows mouse pairing failed (error %lu)",paired);return Reply(false,message);}
 for(int wait=0;wait<50;wait++){BLUETOOTH_DEVICE_INFO info{sizeof(info)};if(MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&info.fAuthenticated)break;Sleep(100);if(wait==49)return Reply(false,"Windows has not confirmed the new Bluetooth authentication");}
 Phase("Wait for the scoped physical renewal to settle before reusing the new Windows key");
 DWORD settled=ConnectionPipe::Command(14);if(settled)return Reply(false,"The physical authentication transaction has not settled yet");
 Phase("Keep native mouse input disabled for the GlideMouse transport");
 DWORD profile=ConnectionPipe::Command(13);
 if(profile){char message[150];sprintf_s(message,"Could not select the GlideMouse input path (error %lu)",profile);return Reply(false,message);}
 if(!native){DWORD closed=ConnectionPipe::Command(1);if(closed!=ERROR_SUCCESS&&closed!=ERROR_NOT_CONNECTED)return Reply(false,"Could not settle the newly paired mouse link");}
 Phase("Connect the new Windows bond through GlideMouse");
 DWORD linked=ConnectionPipe::Command(2);
 if(linked!=ERROR_SUCCESS&&linked!=ERROR_IO_PENDING)return Reply(false,"The new mouse pairing could not start its GlideMouse connection");
 if(native){DWORD activated=ConnectionPipe::Command(8);if(activated!=ERROR_SUCCESS&&activated!=ERROR_BUSY)return Reply(false,"The encrypted mouse link could not activate GlideMouse");}
 for(int wait=0;wait<80;wait++){
  BLUETOOTH_DEVICE_INFO info{sizeof(info)};bool connected=MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&info.fConnected;
  if(connected&&ConnectionPipe::Command(5)==ERROR_SUCCESS&&InputReady(address,ini))return Reply(true,"Connected; Windows pairing and the complete GlideMouse input path verified",true,true);
  Sleep(100);
 }
 BLUETOOTH_DEVICE_INFO current{sizeof(current)};bool connected=MouseConnectionState::Query(address,current)==ERROR_SUCCESS&&current.fConnected;
 return Reply(false,"The new pairing is present, but GlideMouse touch input is not ready",connected,false);
}
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return Reply(false,"Expected one mouse action");
 std::wstring action=argv[1];if(action!=L"--status"&&action!=L"--release"&&action!=L"--release-fast"&&action!=L"--take"&&action!=L"--pair-only"&&action!=L"--prepare")return 64;
 trace=action!=L"--status";Phase("Start");
 init_apartment();
 try{
  wchar_t local[MAX_PATH]{},text[32]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH))return Reply(false,"User profile unavailable");
  std::wstring ini=std::wstring(local)+L"\\IslandMagicMouse\\settings.ini";
  GetPrivateProfileStringW(L"Mouse",L"Address",L"0",text,32,ini.c_str());auto address=wcstoull(text,nullptr,16);
  if(!address||address>0xffffffffffffULL)return Reply(false,"Select a Magic Mouse in GlideMouse first");
  bool freshPairing=GetPrivateProfileIntW(L"Mouse",L"HostSwitchFreshPairing",0,ini.c_str())!=0;
  std::unique_ptr<TargetAuthentication> authentication;
  if(action==L"--take"&&!freshPairing){
   authentication=std::make_unique<TargetAuthentication>(address);
   if(authentication->Start()!=ERROR_SUCCESS)return Reply(false,"Could not register the mouse authentication handler in the user session");
  }
  BLUETOOTH_DEVICE_INFO info{sizeof(info)};bool connected=MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&info.fConnected;
  bool filter=connected&&ConnectionPipe::Command(5)==ERROR_SUCCESS;
  bool driver=filter&&InputReady(address,ini);
  if(action==L"--status")return Reply(true,driver?"Connected; GlideMouse driver is active":connected?"Connected; GlideMouse driver is not active":"Not connected",connected,driver);
  if(action==L"--prepare")return Reply(ConnectionPipe::Command(13)==ERROR_SUCCESS,"Configure the scoped direct mouse profile",connected,driver);
  if(action==L"--pair-only"){
   if(ConnectionPipe::Command(7)!=ERROR_SUCCESS)return Reply(false,"Could not enter quiet GlideMouse recovery");
   if(!info.fAuthenticated){Phase("Restore target authentication without rebuilding the device");DWORD paired=PairTargetEndpoint(address);if(paired!=ERROR_SUCCESS){char message[160];sprintf_s(message,"Windows mouse authentication has not completed (error %lu)",paired);return Reply(false,message);}}
   info={sizeof(info)};for(int i=0;i<30;++i){if(MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&info.fAuthenticated)return Reply(true,"Authentication restored; GlideMouse driver binding still needs verification",info.fConnected!=FALSE,false);Sleep(100);}
   return Reply(false,"Windows did not retain the new mouse authentication");
  }
  if(action==L"--take"&&connected&&driver)return Reply(true,"Connected; GlideMouse driver is active",true,true);
  if(action==L"--take"&&connected&&filter)return Reply(false,"GlideMouse is waiting for finger input; pairing has not been changed",true,false);
  if(action==L"--take"&&freshPairing)return TakeWithFreshPairing(address,ini);
  if(action==L"--release"||action==L"--release-fast"){
   if(connected&&action!=L"--release-fast"){
    // A missing finger report is not a missing HID transport. Give the
    // control channel time to finish without requiring the user to scroll.
    MouseUsbReader reader;IMT_STATUS transport{};
    bool open=reader.Open(address);
    for(int wait=0;wait<20;wait++){
     if(open&&reader.Status(transport)&&transport.devices)break;
     if(wait%4==0)ConnectionPipe::Command(8);
     Sleep(250);
    }
   }
   DWORD held=ConnectionPipe::Command(10);
   if(held!=ERROR_SUCCESS){
    char detail[160];sprintf_s(detail,"Mouse-side release result=%lu; keep the scoped reconnect hold and close only this mouse ACL",held);Phase(detail);
    // A connected mouse may not yet have its HID control channel. The broker
    // has already persisted the reconnect hold; lack of that channel must not
    // strand the device or report failure after the ACL has already closed.
    // Command 1 repeats the exact-device hold and disconnects only this mouse.
    DWORD disconnected=ConnectionPipe::Command(1);
    if(disconnected!=ERROR_SUCCESS&&disconnected!=ERROR_NOT_CONNECTED&&disconnected!=ERROR_DEVICE_NOT_CONNECTED)return Reply(false,"GlideMouse could not release the physical mouse connection");
   }
   for(int i=0;i<30;++i){info={sizeof(info)};if(MouseConnectionState::Query(address,info)!=ERROR_SUCCESS||!info.fConnected)return Reply(true,"Mouse released; Windows pairing and GlideMouse driver are preserved");Sleep(100);}
   return Reply(false,"The physical mouse link has not closed yet");
  }
  if(!info.fAuthenticated)return Reply(false,"Initial GlideMouse pairing must be completed first");
  Phase("Renew authentication while preserving the Windows HID device");
  DWORD renewed=ERROR_NOT_CONNECTED;
  for(int attempt=1;attempt<=3;attempt++){
   renewed=ConnectionPipe::Command(9);info={sizeof(info)};
   connected=MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&info.fConnected;
   driver=connected&&ConnectionPipe::Command(5)==ERROR_SUCCESS&&InputReady(address,ini);
   if(renewed==ERROR_IO_PENDING&&!driver){
    // The secured touch channel may already be ready while the user's input
    // processor is consuming its first frame. Do not tear it down to retry.
    for(int wait=0;wait<20&&!driver;wait++){Sleep(250);info={sizeof(info)};connected=MouseConnectionState::Query(address,info)==ERROR_SUCCESS&&info.fConnected;driver=connected&&ConnectionPipe::Command(5)==ERROR_SUCCESS&&InputReady(address,ini);}
   }
   if(driver){char message[160];sprintf_s(message,"Connected; GlideMouse driver and encrypted link verified after %d attempts",attempt);return Reply(true,message,true,true);}
   // A just-retired peer can terminate the first ACL during handoff. Only
   // transient link failures qualify; permission/configuration errors do not.
   if(attempt==3||(renewed!=19&&renewed!=4&&renewed!=ERROR_TIMEOUT&&renewed!=ERROR_SEM_TIMEOUT))break;
   Sleep(800);
  }
  char message[180];sprintf_s(message,"GlideMouse is not fully ready (renewal result %lu). Pairing and driver node have been preserved",renewed);
  return Reply(false,message,connected,driver); }catch(hresult_error const&){return Reply(false,"Windows Bluetooth operation failed; no automatic retry was started");}
}
