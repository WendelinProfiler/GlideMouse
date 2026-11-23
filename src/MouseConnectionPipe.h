#pragma once
#include <windows.h>
#include <sddl.h>
#include <string>
#include <vector>
namespace ConnectionPipe {
inline std::wstring UserSid(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {};DWORD bytes=0;GetTokenInformation(token,TokenUser,nullptr,0,&bytes);std::vector<BYTE> data(bytes);std::wstring sid;if(GetTokenInformation(token,TokenUser,data.data(),bytes,&bytes)){LPWSTR text=nullptr;if(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&text)){sid=text;LocalFree(text);}}CloseHandle(token);return sid;}
inline std::wstring Name(const std::wstring& sid){return L"\\\\.\\pipe\\GlideMouse.Connection."+sid;}
inline bool Complete(HANDLE file,OVERLAPPED& ov,BOOL started,DWORD& bytes,HANDLE stop,DWORD timeout){DWORD error=started?ERROR_SUCCESS:GetLastError();if(!started&&error!=ERROR_IO_PENDING)return false;HANDLE handles[2]{ov.hEvent,stop};DWORD count=stop?2:1;DWORD waited=WaitForMultipleObjects(count,handles,FALSE,timeout);if(waited!=WAIT_OBJECT_0){CancelIoEx(file,&ov);GetOverlappedResult(file,&ov,&bytes,TRUE);return false;}return GetOverlappedResult(file,&ov,&bytes,FALSE)!=FALSE;}
inline HANDLE OpenCommandPipe(const std::wstring& name){ULONGLONG start=GetTickCount64();DWORD error=ERROR_FILE_NOT_FOUND;while(GetTickCount64()-start<2000){if(WaitNamedPipeW(name.c_str(),50)){HANDLE pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(pipe!=INVALID_HANDLE_VALUE)return pipe;error=GetLastError();}else error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PIPE_BUSY&&error!=ERROR_SEM_TIMEOUT){SetLastError(error);return INVALID_HANDLE_VALUE;}Sleep(15);}SetLastError(error);return INVALID_HANDLE_VALUE;}
inline DWORD Command(DWORD command){auto sid=UserSid();if(sid.empty())return ERROR_ACCESS_DENIED;auto name=Name(sid);HANDLE pipe=OpenCommandPipe(name);if(pipe==INVALID_HANDLE_VALUE)return GetLastError();DWORD mode=PIPE_READMODE_MESSAGE;SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr);OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);DWORD bytes=0,result=ERROR_TIMEOUT;bool ok=Complete(pipe,ov,WriteFile(pipe,&command,sizeof(command),nullptr,&ov),bytes,nullptr,2000);ResetEvent(ov.hEvent);if(ok&&bytes==sizeof(command)){if(!Complete(pipe,ov,ReadFile(pipe,&result,sizeof(result),nullptr,&ov),bytes,nullptr,command==9?45000:20000)||bytes!=sizeof(result))result=ERROR_TIMEOUT;else{BYTE ack=0xA5;ResetEvent(ov.hEvent);Complete(pipe,ov,WriteFile(pipe,&ack,1,nullptr,&ov),bytes,nullptr,500);}}CloseHandle(ov.hEvent);CloseHandle(pipe);return result;}
inline DWORD Request(bool connect){return Command(connect?2:1);}

}
