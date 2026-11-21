#include "input_session.hpp"
#include "runtime_api.hpp"
#include "directinput_keyboard.hpp"
#include <iostream>

int main(int argc,char** argv){
 UINT id=argc>1?static_cast<UINT>(std::stoul(argv[1])):121;
 int errors=0;auto check=[&](bool ok,const char* what){if(!ok){++errors;std::cerr<<what<<" error="<<GetLastError()<<'\n';}};
 check(HviInitialize(id)!=FALSE,"runtime init");
 IDirectInputDevice8A *keyboard=nullptr,*mouse=nullptr;
 check(SUCCEEDED(HviCreateKeyboard(FALSE,reinterpret_cast<void**>(&keyboard))),"keyboard factory");
 check(SUCCEEDED(HviCreateMouse(FALSE,reinterpret_cast<void**>(&mouse))),"mouse factory");
 if(!keyboard||!mouse){if(keyboard)keyboard->Release();if(mouse)mouse->Release();HviShutdown();return 1;}
 check(SUCCEEDED(keyboard->SetDataFormat(&c_dfDIKeyboard))&&SUCCEEDED(mouse->SetDataFormat(&c_dfDIMouse2)),"formats");
 DIPROPDWORD prop{};prop.diph={sizeof(prop),sizeof(DIPROPHEADER),0,DIPH_DEVICE};prop.dwData=64;
 check(SUCCEEDED(keyboard->SetProperty(DIPROP_BUFFERSIZE,&prop.diph))&&SUCCEEDED(mouse->SetProperty(DIPROP_BUFFERSIZE,&prop.diph)),"buffers");
 check(SUCCEEDED(keyboard->Acquire())&&SUCCEEDED(mouse->Acquire()),"acquire");
 hvi::InputSession wrong(id,GetCurrentProcessId()+1);check(!wrong.Heartbeat()&&wrong.lastError==ERROR_ACCESS_DENIED,"PID pin rejects wrong process");wrong.disconnect();Sleep(40);
 hvi::InputSession input(id,GetCurrentProcessId());
 check(!input.KeyDown(256)&&!input.MouseDown(5)&&!input.Submit(static_cast<hvi::Kind>(999)),"invalid inputs local reject");
 int key=id%2?'W':'S',other=id%2?'S':'W';
 auto foreground=GetForegroundWindow();POINT start{},end{};GetCursorPos(&start);
 check(input.MouseMove(id*3,-int(id))&&input.MouseMoveRelative(7,-9),"virtual motion");
 check(input.KeyDown(key)&&input.KeyDown(VK_RCONTROL)&&input.MouseDown(id%5)&&input.Wheel(120,-240),"virtual holds/wheel");
 BYTE keys[256]{};check(HviGetKeyboardState(keys)&&keys[key]==128&&!keys[other]&&keys[VK_CONTROL]&&keys[VK_RCONTROL],"Win32 cooperative keyboard");
 check((HviGetKeyState(key)&0x8000)!=0&&(HviGetAsyncKeyState(key)&0x8001)==0x8001&&(HviGetAsyncKeyState(key)&1)==0,"key state/async edge");
 HviMouseState state;check(HviGetMouseState(&state)&&state.x==id*3+7&&state.y==-int(id)-9&&state.buttons[id%5]&&state.wheel==120&&state.horizontalWheel==-240,"application mouse state");
 DIMOUSESTATE2 ms{};BYTE di[256]{};
 check(SUCCEEDED(mouse->GetDeviceState(sizeof(ms),&ms))&&ms.lX==LONG(id*3+7)&&ms.lY==-LONG(id)-9&&ms.lZ==120&&ms.rgbButtons[id%5],"DI immediate mouse");
 check(SUCCEEDED(keyboard->GetDeviceState(256,di))&&di[hvi::vkToDik(key)]&&!di[hvi::vkToDik(other)],"DI immediate keyboard");
 for(int i=0;i<4;++i){Sleep(140);check(input.Heartbeat(),"heartbeat");}
 check((HviGetKeyState(key)&0x8000)!=0,"heartbeat retains hold");
 check(input.KeyUp(VK_RCONTROL)&&input.MouseUp(id%5)&&input.MouseDown(4),"explicit up and independent hold");
 check(input.ReleaseAll(),"release all command");
 HviMouseState released;check(HviGetMouseState(&released)&&released.x==state.x&&released.y==state.y&&released.dx==state.dx&&released.wheel==state.wheel,"release preserves position and counters");
 HviGetKeyboardState(keys);bool clear=true;for(auto k:keys)clear&=(k&128)==0;for(auto b:released.buttons)clear&=b==0;check(clear,"all holds cleared");
 check(SUCCEEDED(mouse->GetDeviceState(sizeof(ms),&ms))&&ms.lX==0&&ms.lY==0&&ms.lZ==0,"release no inverse deltas");
 DIDEVICEOBJECTDATA events[64]{};DWORD count=64;check(SUCCEEDED(keyboard->GetDeviceData(sizeof(events[0]),events,&count,0))&&count>=4,"DI keyboard transitions");
 count=64;check(SUCCEEDED(mouse->GetDeviceData(sizeof(events[0]),events,&count,0))&&count>=8,"DI mouse transitions");
 HviJournalRead read;HviJournalRecord records[64];check(HviReadJournal(&read,records,64)!=FALSE,"journal");bool release=false;
 for(DWORD i=0;i<read.count;++i)release|=records[i].command.kind==hvi::Kind::ReleaseAll;check(release&&!read.resync,"release journaled for all adapters");
 check(!HviGetMouseState(nullptr),"mouse null");HviMouseState bad;bad.version=99;check(!HviGetMouseState(&bad),"mouse version");
 check(input.KeyDown(key)&&input.MouseDown(0),"disconnect setup");input.disconnect();Sleep(100);
 HviGetMouseState(&state);check(!(HviGetKeyState(key)&0x8000)&&!state.buttons[0],"disconnect clears holds");
 check(input.Heartbeat()&&input.MouseMove(31,47),"reconnect");
 HviSetVirtual(FALSE);check(!HviGetMouseState(&state)&&GetLastError()==ERROR_NOT_SUPPORTED,"native mouse mode explicit unsupported");HviSetVirtual(TRUE);
 check(input.ReleaseAll(),"final release");
 auto after=GetForegroundWindow();
 std::cout<<"foreground observed "<<reinterpret_cast<UINT_PTR>(foreground)<<" -> "<<reinterpret_cast<UINT_PTR>(after)<<" (external changes not blocked; API import guard checks prohibited calls)\n";GetCursorPos(&end);
 std::cout<<"cursor observed "<<start.x<<','<<start.y<<" -> "<<end.x<<','<<end.y<<" (external motion not blocked)\n";
 input.disconnect();keyboard->Unacquire();mouse->Unacquire();keyboard->Release();mouse->Release();HviShutdown();
 std::cout<<"input session "<<id<<" errors="<<errors<<'\n';return errors?1:0;
}