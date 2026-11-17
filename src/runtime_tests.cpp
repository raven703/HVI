#include "directinput_mouse.hpp"
#include "timed_pipe.hpp"
#include <iostream>
int main(int argc,char**argv){UINT id=argc>1?static_cast<UINT>(std::stoul(argv[1])):71;
 HMODULE dll=LoadLibraryW(L"hvi_runtime.dll");if(!dll)return 1;
 auto init=reinterpret_cast<BOOL(WINAPI*)(UINT)>(GetProcAddress(dll,"HviInitialize"));auto stop=reinterpret_cast<void(WINAPI*)()>(GetProcAddress(dll,"HviShutdown"));auto create=reinterpret_cast<HRESULT(WINAPI*)(BOOL,void**)>(GetProcAddress(dll,"HviCreateMouse"));auto keyboard=reinterpret_cast<BOOL(WINAPI*)(PBYTE)>(GetProcAddress(dll,"HviGetKeyboardState"));if(!init||!stop||!create||!keyboard)return 2;
 if(!init(id))return 3;IDirectInputDevice8A* mouse=nullptr;if(FAILED(create(FALSE,reinterpret_cast<void**>(&mouse))))return 4;
 mouse->SetDataFormat(&c_dfDIMouse2);DIPROPDWORD prop{};prop.diph={sizeof(prop),sizeof(DIPROPHEADER),0,DIPH_DEVICE};prop.dwData=16;mouse->SetProperty(DIPROP_BUFFERSIZE,&prop.diph);mouse->Acquire();
 std::wstring name=L"\\\\.\\pipe\\HVI.runtime."+std::to_wstring(id);HANDLE pipe=INVALID_HANDLE_VALUE;
 for(int i=0;i<100&&pipe==INVALID_HANDLE_VALUE;i++){pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(pipe==INVALID_HANDLE_VALUE)Sleep(20);}
 int errors=0;if(pipe==INVALID_HANDLE_VALUE)++errors;
 else {uint32_t sequence=0;auto send=[&](hvi::Kind kind,int a,int b){hvi::Event e;e.session=id;e.sequence=++sequence;e.kind=kind;e.a=a;e.b=b;uint32_t ack=0;if(!hvi::timedIo(pipe,&e,sizeof(e),true)||!hvi::timedIo(pipe,&ack,4,false)||ack!=sequence)++errors;};
 send(hvi::Kind::Relative,id,-int(id));send(hvi::Kind::Button,4,1);send(hvi::Kind::Wheel,120,0);send(hvi::Kind::Key,id%2?'A':'B',1);
 DIMOUSESTATE2 s{};if(FAILED(mouse->GetDeviceState(sizeof(s),&s))||s.lX!=LONG(id)||s.lY!=-LONG(id)||s.lZ!=120||s.rgbButtons[4]!=128)++errors;
 BYTE keys[256]{};keyboard(keys);if(keys[id%2?'A':'B']!=128||keys[id%2?'B':'A'])++errors;
 DIDEVICEOBJECTDATA events[16]{};DWORD n=16;if(FAILED(mouse->GetDeviceData(sizeof(events[0]),events,&n,0))||n!=4)++errors;
 CloseHandle(pipe);Sleep(650);keyboard(keys);if(keys['A']||keys['B'])++errors;
 pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
 if(pipe==INVALID_HANDLE_VALUE)++errors;else {sequence=0;send(hvi::Kind::Key,'C',1);keyboard(keys);if(!keys['C'])++errors;CloseHandle(pipe);Sleep(650);keyboard(keys);if(keys['C'])++errors;}
 }
 mouse->Unacquire();mouse->Release();stop();if(!FreeLibrary(dll))++errors;
 std::cout<<"runtime session "<<id<<" errors="<<errors<<"\n";return errors?1:0;
}