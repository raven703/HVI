#pragma once
#include <windows.h>
namespace hvi {
// All handles passed here must be opened FILE_FLAG_OVERLAPPED.
inline bool timedIo(HANDLE pipe,void*data,DWORD size,bool write,DWORD timeout=1000){
 OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ov.hEvent)return false;
 DWORD bytes=0;BOOL ok=write?WriteFile(pipe,data,size,&bytes,&ov):ReadFile(pipe,data,size,&bytes,&ov);
 if(!ok&&GetLastError()==ERROR_IO_PENDING){
  if(WaitForSingleObject(ov.hEvent,timeout)==WAIT_OBJECT_0)ok=GetOverlappedResult(pipe,&ov,&bytes,FALSE);
  else {CancelIoEx(pipe,&ov);GetOverlappedResult(pipe,&ov,&bytes,TRUE);ok=FALSE;}
 }
 CloseHandle(ov.hEvent);return ok&&bytes==size;
}
}