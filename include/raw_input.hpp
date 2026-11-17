#pragma once
#include <windows.h>
#include "input_state.hpp"
namespace hvi {
// Deliberately NOT HRAWINPUT: these packets cannot be passed to user32.
struct SyntheticRawPacket {uint32_t session;uint64_t sequence;RAWINPUT input;};
inline SyntheticRawPacket encodeRaw(const Event&e){
 SyntheticRawPacket p{};p.session=e.session;p.sequence=e.sequence;p.input.header.dwSize=sizeof(RAWINPUT);
 if(e.kind==Kind::Key){p.input.header.dwType=RIM_TYPEKEYBOARD;auto&k=p.input.data.keyboard;k.VKey=static_cast<USHORT>(e.a);UINT scan=MapVirtualKeyW(e.a,MAPVK_VK_TO_VSC_EX);k.MakeCode=scan&255;k.Flags=(e.b?RI_KEY_MAKE:RI_KEY_BREAK)|((scan&0xff00)?RI_KEY_E0:0);k.Message=e.b?WM_KEYDOWN:WM_KEYUP;}
 else {p.input.header.dwType=RIM_TYPEMOUSE;auto&m=p.input.data.mouse;if(e.kind==Kind::Relative){m.lLastX=e.a;m.lLastY=e.b;}else if(e.kind==Kind::Wheel){m.usButtonFlags=RI_MOUSE_WHEEL;m.usButtonData=static_cast<USHORT>(e.a);}else if(e.kind==Kind::Button&&e.a>=0&&e.a<5)m.usButtonFlags=static_cast<USHORT>(1u<<(e.a*2+(e.b?0:1)));}
 return p;
}
class NativeRawInput {
 HWND target=nullptr;
public:
 bool registerDevices(HWND hwnd){if(!IsWindow(hwnd))return false;RAWINPUTDEVICE r[2]={{1,2,RIDEV_INPUTSINK,hwnd},{1,6,RIDEV_INPUTSINK,hwnd}};if(!RegisterRawInputDevices(r,2,sizeof(r[0])))return false;target=hwnd;return true;}
 void unregisterDevices(){if(target){RAWINPUTDEVICE r[2]={{1,2,RIDEV_REMOVE,nullptr},{1,6,RIDEV_REMOVE,nullptr}};RegisterRawInputDevices(r,2,sizeof(r[0]));target=nullptr;}}
 ~NativeRawInput(){unregisterDevices();}
 static bool read(HRAWINPUT handle,RAWINPUT&out){UINT size=sizeof(out);return GetRawInputData(handle,RID_INPUT,&out,&size,sizeof(RAWINPUTHEADER))!=UINT(-1)&&size<=sizeof(out);}
 static UINT readBuffer(PRAWINPUT out,UINT&size){return GetRawInputBuffer(out,&size,sizeof(RAWINPUTHEADER));}
 static UINT deviceInfo(HANDLE device,RID_DEVICE_INFO&out){out={};out.cbSize=sizeof(out);UINT size=sizeof(out);return GetRawInputDeviceInfoW(device,RIDI_DEVICEINFO,&out,&size);}
};
}