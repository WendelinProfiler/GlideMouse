#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#include "../src/MagicMouseTray.h"
static int Save(Gdiplus::Bitmap& image,const wchar_t* path){UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> bytes(size);auto encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());Gdiplus::GetImageEncoders(count,size,encoders);for(UINT i=0;i<count;i++)if(wcscmp(encoders[i].MimeType,L"image/png")==0)return image.Save(path,&encoders[i].Clsid,nullptr)==Gdiplus::Ok?0:1;return 1;}
int wmain(int argc,wchar_t** argv){if(argc!=3)return 64;Gdiplus::GdiplusStartupInput init;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&init,nullptr)!=Gdiplus::Ok)return 1;int result=0;{
 wchar_t folder[MAX_PATH],file[MAX_PATH];GetTempPathW(MAX_PATH,folder);GetTempFileNameW(folder,L"glm",0,file);WritePrivateProfileStringW(L"Mouse",L"InputMode",L"active",file);WritePrivateProfileStringW(L"Mouse",L"InputHeartbeat",std::to_wstring(GetTickCount64()).c_str(),file);
 GlidePopup::profile=file;GlidePopup::name=L"Black Magic Mouse";GlidePopup::connected=wcscmp(argv[2],L"connected")==0;GlidePopup::fresh=true;GlidePopup::battery=68;GlidePopup::dragging=0;GlidePopup::controls={};
 Gdiplus::Bitmap image(780,1096,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&image);graphics.ScaleTransform(2,2);GlidePopup::Draw(graphics);result=Save(image,argv[1]);DeleteFileW(file);
 }GlidePopup::product.reset();Gdiplus::GdiplusShutdown(token);return result;}
