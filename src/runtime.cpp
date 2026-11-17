#include "directinput_mouse.hpp"
#include "timed_pipe.hpp"
#include <thread>
#include <atomic>
#include <string>
namespace {
std::mutex lifecycle;std::shared_ptr<hvi::MouseFeed> feed=std::make_shared<hvi::MouseFeed>();
std::thread worker;std::atomic<bool> running{false},virtualMode{true};
void serve(uint32_t session){
 std::wstring name=L"\\\\.\\pipe\\HVI.runtime."+std::to_wstring(session);
 while(running){
  HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,nullptr);
  if(pipe==INVALID_HANDLE_VALUE){running=false;break;}
  OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ov.hEvent){CloseHandle(pipe);running=false;break;}
  BOOL connected=ConnectNamedPipe(pipe,&ov);DWORD err=GetLastError();
  if(!connected&&err==ERROR_PIPE_CONNECTED)connected=TRUE;
  else if(!connected&&err==ERROR_IO_PENDING){
   while(running&&WaitForSingleObject(ov.hEvent,50)==WAIT_TIMEOUT){}
   if(running){DWORD n;connected=GetOverlappedResult(pipe,&ov,&n,FALSE);}else {CancelIoEx(pipe,&ov);DWORD n;GetOverlappedResult(pipe,&ov,&n,TRUE);}
  }
  CloseHandle(ov.hEvent);uint32_t last=0;
  while(running&&connected){
   hvi::Event e;if(!hvi::timedIo(pipe,&e,sizeof(e),false,500))break;
   if(e.magic!=0x31495648||e.version!=1||e.session!=session||e.sequence<=last||!feed->apply(e))break;
   last=e.sequence;if(!hvi::timedIo(pipe,&last,sizeof(last),true,500))break;
  }
  feed->reset();DisconnectNamedPipe(pipe);CloseHandle(pipe);
 }
 feed->reset();
}
}
extern "C" __declspec(dllexport) BOOL WINAPI HviInitialize(UINT session){std::lock_guard<std::mutex> l(lifecycle);if(!session||worker.joinable())return FALSE;running=true;try{worker=std::thread(serve,session);}catch(...){running=false;return FALSE;}return TRUE;}
extern "C" __declspec(dllexport) void WINAPI HviShutdown(){std::lock_guard<std::mutex> l(lifecycle);running=false;if(worker.joinable())worker.join();feed->reset();}
extern "C" __declspec(dllexport) void WINAPI HviSetVirtual(BOOL enabled){std::lock_guard<std::mutex> l(feed->mutex);virtualMode=enabled!=FALSE;feed->state={};++feed->generation;}
extern "C" __declspec(dllexport) BOOL WINAPI HviGetKeyboardState(PBYTE out){if(!out)return FALSE;if(!virtualMode)return GetKeyboardState(out);std::lock_guard<std::mutex> l(feed->mutex);for(int k=0;k<256;k++)out[k]=feed->state.keyboard.byte(k);return TRUE;}
extern "C" __declspec(dllexport) SHORT WINAPI HviGetAsyncKeyState(int key){if(!virtualMode)return GetAsyncKeyState(key);std::lock_guard<std::mutex> l(feed->mutex);return static_cast<SHORT>(feed->state.keyboard.async(key));}
extern "C" __declspec(dllexport) HRESULT WINAPI HviCreateMouse(BOOL wide,void**out){if(!out)return E_POINTER;*out=nullptr;if(!virtualMode)return DIERR_UNSUPPORTED;try{if(wide)*out=static_cast<IDirectInputDevice8W*>(new hvi::DirectInputMouse<true>(feed));else *out=static_cast<IDirectInputDevice8A*>(new hvi::DirectInputMouse<false>(feed));return S_OK;}catch(...){return E_OUTOFMEMORY;}}
// Explicit shutdown and COM Release are mandatory before FreeLibrary.
// No thread creation, hooks or blocking operations in DllMain.