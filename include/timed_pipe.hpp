#pragma once
#include <windows.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstring>
namespace hvi {
struct IoCounters {std::atomic<unsigned long long> timeouts{0},errors{0},cancellationsPending{0};};
inline IoCounters ioCounters;
namespace detail {
struct Operation {
 OVERLAPPED ov{};std::vector<unsigned char> bytes;
 explicit Operation(DWORD size):bytes(size){ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);}
 ~Operation(){if(ov.hEvent)CloseHandle(ov.hEvent);}
};
// Retain cancelled operations until completion; kernel IO never references caller memory.
inline std::mutex retiredMutex;
inline auto& retired(){static auto* p=new std::vector<std::unique_ptr<Operation>>;return *p;}
inline void reap(){std::lock_guard<std::mutex> l(retiredMutex);auto& v=retired();for(auto i=v.begin();i!=v.end();)if(WaitForSingleObject((*i)->ov.hEvent,0)==WAIT_OBJECT_0){i=v.erase(i);--ioCounters.cancellationsPending;}else ++i;}
inline bool finish(HANDLE pipe,std::unique_ptr<Operation>& op,BOOL ok,DWORD& bytes,DWORD timeout,HANDLE stop){
 if(!ok&&GetLastError()==ERROR_IO_PENDING){
  HANDLE waits[]{op->ov.hEvent,stop};DWORD w=WaitForMultipleObjects(stop?2:1,waits,FALSE,timeout);
  if(w==WAIT_OBJECT_0)ok=GetOverlappedResult(pipe,&op->ov,&bytes,FALSE);
  else {
   DWORD error=w==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_OPERATION_ABORTED;
   if(w==WAIT_TIMEOUT)++ioCounters.timeouts;
   CancelIoEx(pipe,&op->ov);
   if(WaitForSingleObject(op->ov.hEvent,200)!=WAIT_OBJECT_0){
    std::lock_guard<std::mutex> l(retiredMutex);retired().push_back(std::move(op));++ioCounters.cancellationsPending;
   }
   SetLastError(error);return false;
  }
 }
 if(!ok)++ioCounters.errors;
 return ok!=FALSE;
}
}
// Exact byte-stream transfer, including partial reads, under one deadline.
inline bool timedIo(HANDLE pipe,void* data,DWORD size,bool write,DWORD timeout=1000,HANDLE stop=nullptr){
 if(pipe==INVALID_HANDLE_VALUE||(!data&&size)){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 detail::reap();ULONGLONG deadline=GetTickCount64()+timeout;DWORD done=0;
 while(done<size){
  if((stop&&WaitForSingleObject(stop,0)==WAIT_OBJECT_0)||GetTickCount64()>=deadline){SetLastError(stop&&WaitForSingleObject(stop,0)==WAIT_OBJECT_0?ERROR_OPERATION_ABORTED:ERROR_TIMEOUT);return false;}
  auto op=std::make_unique<detail::Operation>(size-done);if(!op->ov.hEvent)return false;
  if(write)std::memcpy(op->bytes.data(),static_cast<unsigned char*>(data)+done,size-done);
  DWORD n=0;BOOL ok=write?WriteFile(pipe,op->bytes.data(),size-done,&n,&op->ov):ReadFile(pipe,op->bytes.data(),size-done,&n,&op->ov);
  ULONGLONG now=GetTickCount64();DWORD remaining=static_cast<DWORD>(deadline>now?deadline-now:0);
  if(!detail::finish(pipe,op,ok,n,remaining,stop))return false;
  if(!n){SetLastError(ERROR_BROKEN_PIPE);return false;}
  if(!write)std::memcpy(static_cast<unsigned char*>(data)+done,op->bytes.data(),n);done+=n;
 }
 return true;
}
inline bool timedConnect(HANDLE pipe,HANDLE stop,DWORD timeout){
 detail::reap();auto op=std::make_unique<detail::Operation>(0);if(!op->ov.hEvent)return false;
 BOOL ok=ConnectNamedPipe(pipe,&op->ov);if(!ok&&GetLastError()==ERROR_PIPE_CONNECTED)return true;
 DWORD n=0;return detail::finish(pipe,op,ok,n,timeout,stop);
}
}