#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
namespace MouseUsb {
inline uint16_t U16(const uint8_t* p){return uint16_t(p[0]|uint16_t(p[1])<<8);}inline uint32_t U32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
struct Counts {uint64_t usb=0,otherUsb=0,malformed=0,connections=0,disconnects=0,reports=0,contacts=0,interruptIn=0,bulkIn=0,bulkOut=0,hciEvents=0,hciConnections=0,acl=0,proofBindings=0,sdpRequests=0,pendingSubmits=0;};
// No raw USB, Bluetooth, audio or key material is written to logs. Only this
// target's validated 0x12 report may leave the parser callback.
class Parser {
 uint64_t address;uint16_t usbDevice,handle=0xffff,interruptCid=0;uint16_t requestedCid=0,remoteRequestedCid=0;
 std::vector<uint8_t> stream,bulkIn,bulkOut,eventBytes,l2In,l2Out;bool header=false,failed=false;size_t wantedIn=0,wantedOut=0;
 std::function<void(const uint8_t*,size_t,double)> receive;
 std::array<uint8_t,16> proof{};bool proofSet=false,proofBound=false;
 void ClearLink(bool clearBulk=true){proofBound=false;handle=0xffff;interruptCid=requestedCid=remoteRequestedCid=0;l2In.clear();l2Out.clear();wantedIn=wantedOut=0;if(clearBulk){bulkIn.clear();bulkOut.clear();}}
 void Event(const uint8_t* p,size_t n){
  counts.hciEvents++;if(n>=1&&p[0]==3)counts.hciConnections++;if(n<2||size_t(p[1])+2!=n){counts.malformed++;return;}
  if(p[0]==3&&n==13&&p[2]==0){uint64_t mac=0;for(int i=0;i<6;i++)mac|=uint64_t(p[5+i])<<(8*i);if(mac==address){ClearLink();handle=U16(p+3)&0xfff;counts.connections++;}}
  else if(p[0]==5&&n==6&&p[2]==0&&(U16(p+3)&0xfff)==handle){ClearLink();counts.disconnects++;}
 }
 void Signal(const uint8_t* p,size_t n,bool incoming){
  while(n>=4){size_t length=U16(p+2);if(length>n-4){counts.malformed++;return;}
   if(p[0]==2&&length==4&&!incoming&&U16(p+4)==0x13)requestedCid=U16(p+6);
   if(p[0]==3&&length==8&&incoming&&requestedCid&&U16(p+6)==requestedCid&&U16(p+8)==0)interruptCid=requestedCid;
   if(p[0]==2&&length==4&&incoming&&U16(p+4)==0x13)remoteRequestedCid=U16(p+6);
   if(p[0]==3&&length==8&&!incoming&&remoteRequestedCid&&U16(p+6)==remoteRequestedCid&&U16(p+8)==0)interruptCid=U16(p+4);
   p+=4+length;n-=4+length;
  }
  if(n)counts.malformed++;
 }
 void L2(const uint8_t* p,size_t n,bool incoming,double stamp){
  if(n<4||size_t(U16(p))+4!=n){counts.malformed++;return;}uint16_t cid=U16(p+2);p+=4;n-=4;
  if(cid==1){Signal(p,n,incoming);return;}
  if(!incoming||n<2||p[0]!=0xa1||p[1]!=0x12)return;
  if(!interruptCid&&proofBound&&cid>=0x40)interruptCid=cid;
  if(!interruptCid||cid!=interruptCid)return;
  p++;n--;if(n!=8&&(n<14||n>142||(n-14)%8)){counts.malformed++;return;}
  counts.reports++;if(n>14)counts.contacts+=(n-14)/8;if(receive)receive(p,n,stamp);
 }
 void Acl(const uint8_t* p,size_t n,bool incoming,double stamp){
  counts.acl++;if(n<4||size_t(U16(p+2))+4!=n){counts.malformed++;return;}
  // A unique SDP UUID query is sent via the documented Windows Bluetooth
  // namespace to the configured MAC. Seeing that fresh nonce in the outgoing
  // SDP request proves the ACL handle even when capture begins mid-connection.
  if(proofSet&&!incoming&&((U16(p)>>12&3)==0||(U16(p)>>12&3)==2)&&n>=4+4+5+17){
   const uint8_t* l=p+4;size_t ln=n-4;
   if(U16(l)+4==ln&&U16(l+2)>=0x40&&(l[4]==2||l[4]==6)&&size_t((l[7]<<8)|l[8])+5==ln-4){counts.sdpRequests++;
    for(size_t i=9;i+17<=ln;i++)if(l[i]==0x1c&&std::equal(proof.begin(),proof.end(),l+i+1)){uint16_t proven=U16(p)&0xfff;if(handle!=proven)ClearLink(false);handle=proven;proofBound=true;proofSet=false;counts.proofBindings++;break;}
   }
  }
  if(handle==0xffff||(U16(p)&0xfff)!=handle)return;
  unsigned pb=(U16(p)>>12)&3;auto& buf=incoming?l2In:l2Out;auto& wanted=incoming?wantedIn:wantedOut;p+=4;n-=4;
  if(pb==0||pb==2){buf.clear();wanted=0;if(n<4){counts.malformed++;return;}wanted=size_t(U16(p))+4;if(wanted>4096||wanted<n){counts.malformed++;wanted=0;return;}}
  else if(pb!=1||!wanted){counts.malformed++;return;}
  if(n>wanted-buf.size()){counts.malformed++;buf.clear();wanted=0;return;}
  buf.insert(buf.end(),p,p+n);if(buf.size()==wanted){L2(buf.data(),buf.size(),incoming,stamp);buf.clear();wanted=0;}
 }
 void Bulk(const uint8_t* p,size_t n,bool incoming,double stamp){
  auto& buf=incoming?bulkIn:bulkOut;if(n>65536||buf.size()+n>65536){counts.malformed++;buf.clear();return;}buf.insert(buf.end(),p,p+n);size_t at=0;
  while(buf.size()-at>=4){size_t length=size_t(U16(buf.data()+at+2))+4;if(length>4096){counts.malformed++;buf.clear();return;}if(length>buf.size()-at)break;Acl(buf.data()+at,length,incoming,stamp);at+=length;}
  if(at){std::fill(buf.begin(),buf.begin()+at,uint8_t(0));buf.erase(buf.begin(),buf.begin()+at);}
 }
 void Usb(const uint8_t* p,size_t n,double stamp){
  counts.usb++;if(n<27){counts.malformed++;return;}size_t h=U16(p),length=U32(p+23);if(h<27||h>n){counts.malformed++;return;}if(U16(p+19)!=usbDevice){counts.otherUsb++;return;}if(length>n-h)return;
  bool complete=(p[16]&1)!=0,incoming=(p[21]&0x80)!=0;uint8_t transfer=p[22];
  // Outbound submits can legitimately carry USBD_STATUS_PENDING. Their
  // completion has no payload, so rejecting pending submits hides SDP proof.
  const uint32_t status=U32(p+10);if(complete&&status!=0)return;if(!complete&&status!=0&&status!=0x40000000)return;
  if(!complete&&status==0x40000000)counts.pendingSubmits++;p+=h;
  // Only inbound completed reads and outbound submitted bulk data. No HCI
  // commands/control transfers, isochronous audio or unrelated USB devices.
  if(transfer==3&&complete==incoming){if(incoming)counts.bulkIn++;else counts.bulkOut++;Bulk(p,length,incoming,stamp);}
  else if(transfer==1&&complete&&incoming){counts.interruptIn++;
   if(eventBytes.size()+length>1024){counts.malformed++;eventBytes.clear();return;}eventBytes.insert(eventBytes.end(),p,p+length);size_t at=0;
   while(eventBytes.size()-at>=2){size_t bytes=eventBytes[at+1]+2;if(bytes>eventBytes.size()-at)break;Event(eventBytes.data()+at,bytes);at+=bytes;}
   if(at){std::fill(eventBytes.begin(),eventBytes.begin()+at,uint8_t(0));eventBytes.erase(eventBytes.begin(),eventBytes.begin()+at);}
  }
 }
 public:
 Counts counts;
 Parser(uint64_t target,uint16_t device,std::function<void(const uint8_t*,size_t,double)> callback={}):address(target),usbDevice(device),receive(callback){}
 bool Bound()const{return handle!=0xffff&&interruptCid!=0;}
 void SetProof(const std::array<uint8_t,16>& nonce){proof=nonce;proofSet=true;}
 bool Feed(const uint8_t* p,size_t n){
  if(failed||n>1048576||stream.size()+n>1048576){failed=true;return false;}stream.insert(stream.end(),p,p+n);size_t at=0;
  if(!header){if(stream.size()<24)return true;if(U32(stream.data())!=0xa1b2c3d4||U16(stream.data()+4)!=2||U16(stream.data()+6)!=4||U32(stream.data()+20)!=249){failed=true;return false;}at=24;header=true;}
  while(stream.size()-at>=16){auto* r=stream.data()+at;size_t bytes=U32(r+8);if(bytes>65536||bytes>U32(r+12)||U32(r+4)>=1000000){failed=true;return false;}if(stream.size()-at<16+bytes)break;Usb(r+16,bytes,double(U32(r))+double(U32(r+4))/1e6);at+=16+bytes;}
  if(at){std::fill(stream.begin(),stream.begin()+at,uint8_t(0));stream.erase(stream.begin(),stream.begin()+at);}return true;
 }
};
}
