#pragma once
#include "Protocol.h"
#include <windows.h>
#include <cstring>
#include <cstddef>
constexpr uint32_t IUSB_MAGIC=0x42535549;
constexpr wchar_t IUSB_MAPPING[]=L"Global\\IslandMagicMouse.UsbReports.v1";
struct IUSB_SHARED {alignas(8) volatile LONG64 epoch;uint32_t magic,size;uint64_t target;uint64_t heartbeat;uint32_t producerPid,reserved;IMT_STATUS status;IMT_RECORD records[IMT_RING_SIZE];};
class MouseUsbReader {
 HANDLE mapping=nullptr;const IUSB_SHARED* data=nullptr;
 public:
 ~MouseUsbReader(){if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);}
 bool Open(uint64_t expectedTarget){mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,IUSB_MAPPING);if(!mapping)return false;data=static_cast<const IUSB_SHARED*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(IUSB_SHARED)));return data&&data->magic==IUSB_MAGIC&&data->size==sizeof(IUSB_SHARED)&&data->target==expectedTarget;}
 bool Status(IMT_STATUS& status){if(!data)return false;for(int i=0;i<8;i++){auto before=data->epoch;MemoryBarrier();if(before&1)continue;auto heartbeat=data->heartbeat;std::memcpy(&status,&data->status,sizeof(status));MemoryBarrier();if(before==data->epoch){return GetTickCount64()>=heartbeat&&GetTickCount64()-heartbeat<2000&&status.version==IMT_VERSION;}}return false;}
 // Producer-owned proof of a full touch report on the current ACL generation.
 bool TouchReady(){if(!data)return false;for(int i=0;i<8;i++){auto before=data->epoch;MemoryBarrier();if(before&1)continue;auto heartbeat=data->heartbeat;bool ready=data->reserved==1&&data->status.devices!=0;MemoryBarrier();if(before==data->epoch){auto now=GetTickCount64();return ready&&now>=heartbeat&&now-heartbeat<2000;}}return false;}
 bool Read(const IMT_READ& query,IMT_BATCH& batch,DWORD& size){if(!data)return false;for(int tries=0;tries<8;tries++){auto before=data->epoch;MemoryBarrier();if(before&1)continue;uint64_t latest=data->status.frames;batch={};batch.version=IMT_VERSION;uint64_t first=latest>=IMT_RING_SIZE?latest-IMT_RING_SIZE+1:1;uint64_t next=query.afterSequence+1;if(next<first){batch.missed=first-next;next=first;}while(next<=latest&&batch.count<IMT_BATCH_SIZE){auto& record=data->records[(next-1)%IMT_RING_SIZE];if(record.sequence!=next)break;std::memcpy(&batch.records[batch.count++],&record,sizeof(record));next++;}MemoryBarrier();if(before==data->epoch){size=DWORD(offsetof(IMT_BATCH,records)+batch.count*sizeof(IMT_RECORD));return true;}}return false;}
};
