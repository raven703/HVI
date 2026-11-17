#include "directinput_keyboard.hpp"
#include "runtime_transport.hpp"
#include "runtime_api.hpp"
#include <thread>
#include <iostream>
int main(){
 int errors=0;auto check=[&](bool ok,const char* what){if(!ok){++errors;std::cerr<<what<<" error="<<GetLastError()<<"\n";}};
 auto feed=std::make_shared<hvi::MouseFeed>();auto*d=new hvi::DirectInputMouse<false>(feed);d->SetDataFormat(&c_dfDIMouse2);DIPROPDWORD prop{};prop.diph={sizeof(prop),sizeof(DIPROPHEADER),0,DIPH_DEVICE};prop.dwData=256;d->SetProperty(DIPROP_BUFFERSIZE,&prop.diph);d->Acquire();
 hvi::Event e;e.kind=hvi::Kind::Relative;e.a=1;feed->apply(e);DIMOUSESTATE2 mouse{};d->GetDeviceState(sizeof(mouse),&mouse);DIDEVICEOBJECTDATA data[256]{};DWORD n=256;d->GetDeviceData(sizeof(data[0]),data,&n,0);
 for(int i=0;i<300;++i){e.kind=hvi::Kind::Key;e.a='A';e.b=i%2;feed->apply(e);}
 e.kind=hvi::Kind::Relative;e.a=1;e.b=0;feed->apply(e);n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,0)==DI_OK&&n==1,"keyboard gaps not mouse loss");
 d->GetDeviceState(sizeof(mouse),&mouse);e.kind=hvi::Kind::ResetMouse;feed->apply(e);d->GetDeviceState(sizeof(mouse),&mouse);check(mouse.lX==0&&mouse.lY==0,"reset mouse no inverse delta");e.kind=hvi::Kind::Relative;e.a=1;e.b=0;
 for(int i=0;i<300;++i)feed->apply(e);n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,DIGDD_PEEK)==DI_BUFFEROVERFLOW&&n==256,"actual upstream mouse loss reported");
 // Concurrent reset/read must not resurrect deltas or race generation changes.
 std::thread resetter([&]{for(int i=0;i<1000;++i){feed->reset();feed->apply(e);}});
 for(int i=0;i<1000;++i)d->GetDeviceState(sizeof(mouse),&mouse);resetter.join();feed->reset();d->GetDeviceState(sizeof(mouse),&mouse);check(mouse.lX==0,"reset state");d->Release();
 check(!HviInitialize(0),"invalid session");check(HviInitialize(91),"init");check(!HviInitialize(91),"double init");
 IDirectInputDevice8A* keyboard=nullptr;check(SUCCEEDED(HviCreateKeyboard(FALSE,reinterpret_cast<void**>(&keyboard))),"factory");if(!keyboard){HviShutdown();return 1;}keyboard->SetDataFormat(&c_dfDIKeyboard);keyboard->Acquire();
 hvi::RuntimeTransport client(91);check(client.send(hvi::Kind::Key,'W',1),"authenticated command");BYTE keys[256]{};keyboard->GetDeviceState(256,keys);check(keys[DIK_W]==128,"keyboard API");
 HviSnapshot snap;HviGetSnapshot(&snap);check(snap.connected&&snap.clientPid==GetCurrentProcessId()&&snap.liveObjects==1,"identity diagnostics");
 client.disconnect();Sleep(100);keyboard->GetDeviceState(256,keys);check(!keys[DIK_W],"disconnect reset");check(client.send(hvi::Kind::Key,'S',1),"reconnect sequence reset");keyboard->GetDeviceState(256,keys);check(keys[DIK_S]&&!keys[DIK_W],"reconnect API");
 Sleep(800);keyboard->GetDeviceState(256,keys);check(!keys[DIK_S],"stalled peer reset");HviGetSnapshot(&snap);check(snap.timeouts>0,"timeout diagnostic");client.disconnect();
 check(client.send(hvi::Kind::Key,'D',1),"reconnect after idle");client.disconnect();Sleep(100);
 // Invalid protocol must not be acknowledged or applied.
 HANDLE bad=CreateFileW(hvi::runtimePipeName(91).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
 if(bad!=INVALID_HANDLE_VALUE){e={};e.session=91;e.sequence=1;e.magic=0;e.kind=hvi::Kind::Key;e.a='A';e.b=1;hvi::timedIo(bad,&e,sizeof(e),true);DWORD ack=0;check(!hvi::timedIo(bad,&ack,sizeof(ack),false,200),"invalid protocol rejected");CloseHandle(bad);}else check(false,"bad client open");
 Sleep(100);HviGetSnapshot(&snap);check(snap.rejected>0,"rejection diagnostic");
 // A token that cannot establish identity must be rejected before command apply.
 HANDLE anonymous=CreateFileW(hvi::runtimePipeName(91).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_ANONYMOUS,nullptr);
 if(anonymous!=INVALID_HANDLE_VALUE){e={};e.session=91;e.sequence=1;e.kind=hvi::Kind::Key;e.a='A';e.b=1;hvi::timedIo(anonymous,&e,sizeof(e),true);DWORD ack=0;check(!hvi::timedIo(anonymous,&ack,sizeof(ack),false,200),"anonymous identity rejected");CloseHandle(anonymous);}else check(false,"anonymous open");
 Sleep(100);keyboard->GetDeviceState(256,keys);check(!keys[DIK_A],"anonymous command not applied");
 // The byte-stream protocol must accept a fragmented Event without desync.
 HANDLE fragmented=CreateFileW(hvi::runtimePipeName(91).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IMPERSONATION,nullptr);
 if(fragmented!=INVALID_HANDLE_VALUE){e={};e.session=91;e.sequence=1;e.kind=hvi::Kind::Key;e.a='W';e.b=1;check(hvi::timedIo(fragmented,&e,9,true),"fragment one");Sleep(20);check(hvi::timedIo(fragmented,reinterpret_cast<BYTE*>(&e)+9,sizeof(e)-9,true),"fragment two");DWORD ack=0;check(hvi::timedIo(fragmented,&ack,sizeof(ack),false)&&ack==1,"fragment ACK");keyboard->GetDeviceState(256,keys);check(keys[DIK_W],"fragment API state");
  // Shutdown while a new read is pending must not wait for its idle timeout.
  auto start=GetTickCount64();HviShutdown();check(GetTickCount64()-start<1000,"shutdown bounded pending read");CloseHandle(fragmented);
 }else {check(false,"fragment open");HviShutdown();}
 keyboard->Unacquire();keyboard->Release();HviGetSnapshot(&snap);check(snap.liveObjects==0&&snap.pendingCancellations==0,"clean objects IO");
 check(!HviGetSnapshot(nullptr),"snapshot null");snap.size=1;check(!HviGetSnapshot(&snap),"snapshot size");check(!HviGetKeyboardState(nullptr),"keyboard null");check(HviGetAsyncKeyState(-1)==0,"key range");
 for(int i=0;i<10;++i){check(HviInitialize(91),"repeat init");HviShutdown();}
 std::cout<<"stability errors="<<errors<<"\n";return errors?1:0;
}