#pragma once
#include "input_state.hpp"
#include "timed_pipe.hpp"
#include <string>
namespace hvi {
inline std::wstring runtimePipeName(UINT session){return L"\\\\.\\pipe\\HVI.runtime."+std::to_wstring(session);}
// Thread-confined client. GUI owns these only on its IO thread.
class RuntimeTransport {
 HANDLE pipe=INVALID_HANDLE_VALUE;UINT session;uint32_t sequence=0;DWORD expectedPid=0;
public:
 uint64_t sent=0,acks=0,errors=0;DWORD lastError=0;double latencyMs=0;
 explicit RuntimeTransport(UINT id,DWORD pid=0):session(id),expectedPid(pid){}
 ~RuntimeTransport(){disconnect();}
 RuntimeTransport(const RuntimeTransport&)=delete;
 RuntimeTransport& operator=(const RuntimeTransport&)=delete;
 bool connected()const{return pipe!=INVALID_HANDLE_VALUE;}
 void disconnect(){if(connected())CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;sequence=0;}
 bool connect(){
   if(connected())return true;
   if(!session){lastError=ERROR_INVALID_PARAMETER;SetLastError(lastError);return false;}
  pipe=CreateFileW(runtimePipeName(session).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IMPERSONATION,nullptr);
   if(!connected()){lastError=GetLastError();return false;}
   if(expectedPid){ULONG pid=0;if(!GetNamedPipeServerProcessId(pipe,&pid)||pid!=expectedPid){disconnect();lastError=ERROR_ACCESS_DENIED;SetLastError(lastError);return false;}}
   sequence=0;lastError=0;return true;
 }
 bool send(Kind kind,int a=0,int b=0){
  if(!connect()){++errors;return false;}
  if(sequence==UINT32_MAX){disconnect();if(!connect()){++errors;return false;}}
  Event e;e.session=session;e.sequence=++sequence;e.kind=kind;e.a=a;e.b=b;DWORD ack=0;
  LARGE_INTEGER start,end,freq;QueryPerformanceCounter(&start);QueryPerformanceFrequency(&freq);++sent;
  bool ok=timedIo(pipe,&e,sizeof(e),true,250)&&timedIo(pipe,&ack,sizeof(ack),false,250)&&ack==sequence;
  QueryPerformanceCounter(&end);latencyMs=1000.0*(end.QuadPart-start.QuadPart)/freq.QuadPart;
  if(ok){++acks;lastError=0;}else {lastError=GetLastError();if(!lastError)lastError=ERROR_INVALID_DATA;++errors;disconnect();}return ok;
 }
};
}