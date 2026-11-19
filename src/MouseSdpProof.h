#pragma once
#include <winsock2.h>
#include <ws2bth.h>
#include <objbase.h>
#include <array>
#include <atomic>
struct MouseSdpProof {
 GUID uuid{};std::array<uint8_t,16> wire{};
 std::atomic<int> stage{0},startupError{0},addressError{0},beginError{0},nextError{0};
 bool Make(){if(FAILED(CoCreateGuid(&uuid)))return false;wire={uint8_t(uuid.Data1>>24),uint8_t(uuid.Data1>>16),uint8_t(uuid.Data1>>8),uint8_t(uuid.Data1),uint8_t(uuid.Data2>>8),uint8_t(uuid.Data2),uint8_t(uuid.Data3>>8),uint8_t(uuid.Data3),uuid.Data4[0],uuid.Data4[1],uuid.Data4[2],uuid.Data4[3],uuid.Data4[4],uuid.Data4[5],uuid.Data4[6],uuid.Data4[7]};return true;}
 void Query(uint64_t target){
  stage=1;WSADATA wsa{};int error=WSAStartup(MAKEWORD(2,2),&wsa);if(error){startupError=error;return;}
  SOCKADDR_BTH remote{};remote.addressFamily=AF_BTH;remote.btAddr=target;WCHAR context[128]{};DWORD length=128;stage=2;
  if(WSAAddressToStringW(reinterpret_cast<SOCKADDR*>(&remote),sizeof(remote),nullptr,context,&length)){addressError=WSAGetLastError();WSACleanup();return;}
  WSAQUERYSETW set{};set.dwSize=sizeof(set);set.dwNameSpace=NS_BTH;set.lpszContext=context;BTH_QUERY_SERVICE query{};query.type=SDP_SERVICE_SEARCH_REQUEST;query.uuids[0].uuidType=SDP_ST_UUID128;query.uuids[0].u.uuid128=uuid;BLOB blob{sizeof(query),reinterpret_cast<BYTE*>(&query)};set.lpBlob=&blob;HANDLE lookup=nullptr;stage=3;
  if(WSALookupServiceBeginW(&set,LUP_FLUSHCACHE|LUP_RETURN_BLOB,&lookup)==0){
   alignas(WSAQUERYSETW) BYTE bytes[8192]{};auto* result=reinterpret_cast<WSAQUERYSETW*>(bytes);result->dwSize=sizeof(*result);DWORD size=sizeof(bytes);stage=4;
   if(WSALookupServiceNextW(lookup,LUP_RETURN_BLOB,&size,result))nextError=WSAGetLastError();
   WSALookupServiceEnd(lookup);SecureZeroMemory(bytes,sizeof(bytes));stage=5;
  }else beginError=WSAGetLastError();
  WSACleanup();
 }
};
