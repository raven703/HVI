#pragma once
#include "runtime_transport.hpp"

namespace hvi {
// Slave-facing, thread-confined command endpoint. One instance per target Runtime.
// No shared cursor/held state: the target process State is the source of truth.
// ACK means applied to Runtime, not consumed by an application API.
class InputSession final:public RuntimeTransport {
 bool invalid(){lastError=ERROR_INVALID_PARAMETER;SetLastError(lastError);++errors;return false;}
public:
 explicit InputSession(UINT session,DWORD expectedProcess=0):RuntimeTransport(session,expectedProcess){}
 bool KeyDown(int vk){return Submit(Kind::Key,vk,1);}
 bool KeyUp(int vk){return Submit(Kind::Key,vk,0);}
 // Absolute application-local coordinates; not desktop pixels or normalized HID.
 bool MouseMove(int x,int y){return Submit(Kind::Absolute,x,y);}
 bool MouseMoveRelative(int dx,int dy){return Submit(Kind::Relative,dx,dy);}
 // Buttons: 0 left, 1 right, 2 middle, 3 X1, 4 X2.
 bool MouseDown(int button){return Submit(Kind::Button,button,1);}
 bool MouseUp(int button){return Submit(Kind::Button,button,0);}
 bool Wheel(int vertical,int horizontal=0){return Submit(Kind::Wheel,vertical,horizontal);}
 bool ReleaseAll(){return Submit(Kind::ReleaseAll);}
 // Caller must tick at <500ms while connected, including while holding keys.
 // No hidden worker/thread or automatic retry of ambiguously acknowledged input.
 bool Heartbeat(){return Submit(Kind::Snapshot);}
 bool Submit(Kind kind,int a=0,int b=0){
  switch(kind){
   case Kind::Key:if(a<0||a>255||(b!=0&&b!=1))return invalid();break;
   case Kind::Button:if(a<0||a>4||(b!=0&&b!=1))return invalid();break;
   case Kind::Focus:if((a!=0&&a!=1)||b)return invalid();break;
   case Kind::Absolute:case Kind::Relative:case Kind::Wheel:break;
   case Kind::Reset:case Kind::ResetMouse:case Kind::ReleaseAll:case Kind::Snapshot:case Kind::Stop:
    if(a||b)return invalid();break;
   default:return invalid();
  }
  return RuntimeTransport::send(kind,a,b);
 }
private:
 using RuntimeTransport::send;
};
}