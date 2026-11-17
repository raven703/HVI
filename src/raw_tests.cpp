#include "raw_input.hpp"
#include <iostream>
int main(){hvi::Event e;e.session=3;e.sequence=7;e.kind=hvi::Kind::Key;e.a='A';e.b=1;auto p=hvi::encodeRaw(e);if(p.session!=3||p.input.data.keyboard.Flags&RI_KEY_BREAK)return 1;e.b=0;if(!(hvi::encodeRaw(e).input.data.keyboard.Flags&RI_KEY_BREAK))return 2;e.kind=hvi::Kind::Relative;e.a=-10;e.b=20;if(hvi::encodeRaw(e).input.data.mouse.lLastX!=-10)return 3;
 HWND w=CreateWindowExW(0,L"STATIC",L"HVI native raw receiver",0,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!w)return 4;
 hvi::NativeRawInput native;if(!native.registerDevices(w)){DestroyWindow(w);return 5;}RAWINPUTDEVICE registrations[16]{};UINT count=16;if(GetRegisteredRawInputDevices(registrations,&count,sizeof(RAWINPUTDEVICE))==UINT(-1))return 6;
 RAWINPUT invalid{};if(hvi::NativeRawInput::read(reinterpret_cast<HRAWINPUT>(1),invalid))return 7;
 native.unregisterDevices();DestroyWindow(w);std::cout<<"raw registration/invalid native handle and synthetic codec PASS; no physical WM_INPUT delivery asserted\n";return 0;}