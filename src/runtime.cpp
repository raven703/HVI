#define HVI_RUNTIME_BUILD
#include "runtime_api.hpp"
#include "directinput_keyboard.hpp"
#include "timed_pipe.hpp"
#include "pipe_identity.hpp"
#include <thread>
#include <atomic>
#include <string>
namespace {
std::mutex lifecycle;std::shared_ptr<hvi::MouseFeed> feed=std::make_shared<hvi::MouseFeed>();
std::thread worker;std::atomic<bool> running{false},virtualMode{true};HANDLE stopEvent=nullptr;
HviSnapshot diagnostics;std::deque<HviDiagnosticEvent> recent;
std::deque<HviJournalRecord> journal;uint64_t journalOrder=0;
void append(const hvi::Event& e){journal.push_back({++journalOrder,GetTickCount64(),e});if(journal.size()>16384)journal.pop_front();}
void reset(){std::lock_guard<std::mutex> l(feed->mutex);if(diagnostics.connected){hvi::Event e;e.kind=hvi::Kind::Reset;append(e);}feed->state={};++feed->generation;diagnostics.connected=0;diagnostics.clientPid=0;recent.clear();}
void serve(uint32_t session,HANDLE pipe){
 while(running){
  BOOL connected=hvi::timedConnect(pipe,stopEvent,1000);uint32_t last=0;bool verified=false;
  while(running&&connected){
   hvi::Event e;if(!hvi::timedIo(pipe,&e,sizeof(e),false,500,stopEvent)){std::lock_guard<std::mutex> l(feed->mutex);diagnostics.lastError=GetLastError();break;}
   DWORD pid=0;
   if(!verified){
    if(!hvi::verifyPipeClient(pipe,pid)){std::lock_guard<std::mutex> l(feed->mutex);++diagnostics.rejected;diagnostics.lastError=ERROR_ACCESS_DENIED;break;}
    verified=true;std::lock_guard<std::mutex> l(feed->mutex);diagnostics.connected=1;diagnostics.clientPid=pid;++diagnostics.reconnects;
   }
   {
    std::lock_guard<std::mutex> l(feed->mutex);auto before=feed->state.order;
    if(e.magic!=0x31495648||e.version!=1||e.session!=session||!e.sequence||e.sequence<=last||!feed->state.apply(e)){++diagnostics.rejected;diagnostics.lastError=ERROR_INVALID_DATA;break;}
    ++diagnostics.accepted;diagnostics.lastError=0;
    if(e.kind!=hvi::Kind::Snapshot)append(e);
    for(auto& t:feed->state.events.read(256,true))if(t.order>before){recent.push_back({t.order,t.timestamp,t.sequence,static_cast<DWORD>(t.kind),t.control,t.value});if(recent.size()>20)recent.pop_front();}
   }
   last=e.sequence;if(!hvi::timedIo(pipe,&last,sizeof(last),true,500,stopEvent))break;
  }
  reset();DisconnectNamedPipe(pipe);
 }
 CloseHandle(pipe);reset();hvi::detail::reap();
}
}
extern "C" __declspec(dllexport) BOOL WINAPI HviInitialize(UINT session){
 std::lock_guard<std::mutex> l(lifecycle);if(!session||worker.joinable()){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 hvi::PipeSecurity security;if(!security.valid())return FALSE;
 auto name=L"\\\\.\\pipe\\HVI.runtime."+std::to_wstring(session);
 HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,&security.attributes);
 if(pipe==INVALID_HANDLE_VALUE)return FALSE;
 stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent){CloseHandle(pipe);return FALSE;}
 {std::lock_guard<std::mutex> f(feed->mutex);feed->state={};++feed->generation;diagnostics={};diagnostics.session=session;diagnostics.pid=GetCurrentProcessId();recent.clear();}
 running=true;virtualMode=true;try{worker=std::thread(serve,session,pipe);}catch(...){running=false;CloseHandle(pipe);CloseHandle(stopEvent);stopEvent=nullptr;SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}return TRUE;
}
extern "C" __declspec(dllexport) void WINAPI HviShutdown(){std::lock_guard<std::mutex> l(lifecycle);running=false;if(stopEvent)SetEvent(stopEvent);if(worker.joinable())worker.join();if(stopEvent){CloseHandle(stopEvent);stopEvent=nullptr;}reset();hvi::detail::reap();}
extern "C" __declspec(dllexport) void WINAPI HviSetVirtual(BOOL enabled){std::lock_guard<std::mutex> l(feed->mutex);virtualMode=enabled!=FALSE;feed->state={};++feed->generation;hvi::Event e;e.kind=hvi::Kind::Reset;append(e);}
extern "C" __declspec(dllexport) BOOL WINAPI HviGetKeyboardState(PBYTE out){if(!out){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}std::lock_guard<std::mutex> l(feed->mutex);if(!virtualMode)return GetKeyboardState(out);for(int k=0;k<256;k++)out[k]=feed->state.keyboard.byte(k);return TRUE;}
extern "C" __declspec(dllexport) SHORT WINAPI HviGetAsyncKeyState(int key){if(key<0||key>=256){SetLastError(ERROR_INVALID_PARAMETER);return 0;}std::lock_guard<std::mutex> l(feed->mutex);if(!virtualMode)return GetAsyncKeyState(key);return static_cast<SHORT>(feed->state.keyboard.async(key));}
extern "C" __declspec(dllexport) HRESULT WINAPI HviCreateMouse(BOOL wide,void**out){if(!out)return E_POINTER;*out=nullptr;if(!virtualMode)return DIERR_UNSUPPORTED;try{if(wide)*out=static_cast<IDirectInputDevice8W*>(new hvi::DirectInputMouse<true>(feed));else *out=static_cast<IDirectInputDevice8A*>(new hvi::DirectInputMouse<false>(feed));return S_OK;}catch(...){return E_OUTOFMEMORY;}}
extern "C" __declspec(dllexport) HRESULT WINAPI HviCreateKeyboard(BOOL wide,void**out){if(!out)return E_POINTER;*out=nullptr;if(!virtualMode)return DIERR_UNSUPPORTED;try{if(wide)*out=static_cast<IDirectInputDevice8W*>(new hvi::DirectInputKeyboard<true>(feed));else *out=static_cast<IDirectInputDevice8A*>(new hvi::DirectInputKeyboard<false>(feed));return S_OK;}catch(...){return E_OUTOFMEMORY;}}
extern "C" __declspec(dllexport) BOOL WINAPI HviGetSnapshot(HviSnapshot* out){if(!out||out->size!=sizeof(*out)||out->version!=1){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}std::lock_guard<std::mutex> l(feed->mutex);*out=diagnostics;out->generation=feed->generation;out->liveObjects=feed->liveObjects;out->timeouts=hvi::ioCounters.timeouts;out->ioErrors=hvi::ioCounters.errors;out->pendingCancellations=hvi::ioCounters.cancellationsPending;auto& m=feed->state.mouse;out->x=m.x;out->y=m.y;out->dx=m.dx;out->dy=m.dy;out->wheel=m.wheel;for(int i=0;i<5;++i)out->buttons[i]=m.buttons[i]?128:0;return TRUE;}
extern "C" __declspec(dllexport) BOOL WINAPI HviReadEvents(HviDiagnosticEvent* out,DWORD* count){if(!count||(!out&&*count)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}std::lock_guard<std::mutex> l(feed->mutex);DWORD n=static_cast<DWORD>(std::min<size_t>(*count,recent.size()));for(DWORD i=0;i<n;++i)out[i]=recent[recent.size()-n+i];*count=n;return TRUE;}
// Explicit shutdown and COM Release are mandatory before FreeLibrary.
extern "C" __declspec(dllexport) BOOL WINAPI HviReadJournal(HviJournalRead* r,HviJournalRecord* out,DWORD capacity){
 if(!r||r->size!=sizeof(*r)||r->version!=1||(!out&&capacity)||capacity>16384){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 std::lock_guard<std::mutex> l(feed->mutex);r->count=0;r->lost=0;r->resync=0;r->latest=journalOrder;r->generation=feed->generation;
 if(r->cursor>journalOrder||(!journal.empty()&&r->cursor<journal.front().order-1)){
  r->resync=1;r->lost=r->cursor>journalOrder?1:journal.front().order-1-r->cursor;r->cursor=journalOrder;
 }else if(capacity&&!journal.empty()){size_t offset=static_cast<size_t>(r->cursor+1-journal.front().order);for(size_t i=offset;i<journal.size()&&r->count<capacity;++i){out[r->count++]=journal[i];r->cursor=journal[i].order;}}
 for(int k=0;k<256;++k)r->keyboard[k]=feed->state.keyboard.byte(k);auto&m=feed->state.mouse;
 for(int i=0;i<5;++i)r->buttons[i]=m.buttons[i]?128:0;
 r->x=m.x;r->y=m.y;r->dx=m.dx;r->dy=m.dy;r->wheel=m.wheel;r->horizontalWheel=m.horizontalWheel;return TRUE;
}
// No thread creation, hooks or blocking operations in DllMain.