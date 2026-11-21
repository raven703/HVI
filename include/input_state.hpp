#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <deque>
#include <vector>
#include <mutex>
#include <algorithm>
namespace hvi {
enum class Kind:uint32_t { Key=1,Button,Absolute,Relative,Reset,Snapshot,Stop,Wheel,Focus,ResetMouse,ReleaseAll };
struct Event {uint32_t magic=0x31495648,version=1,session=0,sequence=0;Kind kind=Kind::Reset;int32_t a=0,b=0;};
static_assert(sizeof(Event)==28);
struct Transition {uint64_t order;uint32_t sequence;Kind kind;int32_t control;int64_t value;uint64_t timestamp;};
class EventBuffer {
 std::deque<Transition> events;
 size_t capacity=256;
 bool overflow=false;
 uint64_t dropped=0;
public:
 void push(Transition t){if(events.size()==capacity){dropped=events.front().order;events.pop_front();overflow=true;}events.push_back(t);}
 uint64_t lastDroppedOrder()const{return dropped;}
 std::vector<Transition> read(size_t count,bool peek=false){count=std::min(count,events.size());std::vector<Transition> result(events.begin(),events.begin()+count);if(!peek){events.erase(events.begin(),events.begin()+count);overflow=false;}return result;}
 size_t size()const{return events.size();}
 bool overflowed()const{return overflow;}
 void clear(){events.clear();overflow=false;dropped=0;}
};
struct VirtualKeyboard {
 std::array<bool,256> keys{},pressed{},released{},toggled{},asyncPressed{};
 std::array<uint64_t,256> repeats{};
 bool set(int key,bool down){
  if(key<0||key>=256)return false;
  if(keys[key]==down){if(down)++repeats[key];return false;}
  keys[key]=down;
  if(down){pressed[key]=true;asyncPressed[key]=true;if(key==0x14||key==0x90||key==0x91)toggled[key]=!toggled[key];}
  else released[key]=true;
  return true;
 }
 uint8_t byte(int key)const{if(key<0||key>=256)return 0;return static_cast<uint8_t>((keys[key]?0x80:0)|(toggled[key]?1:0));}
 uint16_t async(int key){if(key<0||key>=256)return 0;uint16_t v=(keys[key]?0x8000:0)|(asyncPressed[key]?1:0);asyncPressed[key]=false;return v;}
 void nextFrame(){pressed.fill(false);released.fill(false);}
};
struct VirtualMouse {std::array<bool,5> buttons{};int64_t x=0,y=0,dx=0,dy=0,wheel=0,horizontalWheel=0;};
struct State {
 VirtualKeyboard keyboard;VirtualMouse mouse;EventBuffer events,keyboardEvents,mouseEvents;
 bool focused=false;uint64_t order=0,mouseEpoch=0;
 void emit(const Event&e,Kind kind,int32_t control,int64_t value){
  Transition t{++order,e.sequence,kind,control,value,GetTickCount64()};events.push(t);
  if(kind==Kind::Key)keyboardEvents.push(t);else mouseEvents.push(t);
 }
 bool apply(const Event&e){
  switch(e.kind){
   case Kind::Key:{
    if(e.a<0||e.a>=256||(e.b!=0&&e.b!=1))return false;
    int key=e.a;if(key==VK_SHIFT)key=VK_LSHIFT;if(key==VK_CONTROL)key=VK_LCONTROL;if(key==VK_MENU)key=VK_LMENU;
    if(keyboard.set(key,e.b!=0))emit(e,Kind::Key,key,e.b?0x80:0);
    for(auto group:std::array<std::array<int,3>,3>{{{{VK_SHIFT,VK_LSHIFT,VK_RSHIFT}},{{VK_CONTROL,VK_LCONTROL,VK_RCONTROL}},{{VK_MENU,VK_LMENU,VK_RMENU}}}}){
     bool down=keyboard.keys[group[1]]||keyboard.keys[group[2]];if(keyboard.keys[group[0]]!=down)keyboard.set(group[0],down);
    }break;
   }
   case Kind::Button:if(e.a<0||e.a>=5||(e.b!=0&&e.b!=1))return false;if(mouse.buttons[e.a]!=(e.b!=0)){mouse.buttons[e.a]=e.b!=0;emit(e,e.kind,e.a,e.b?0x80:0);}break;
   case Kind::Absolute:{int64_t dx=static_cast<int64_t>(e.a)-mouse.x,dy=static_cast<int64_t>(e.b)-mouse.y;mouse.x=e.a;mouse.y=e.b;mouse.dx+=dx;mouse.dy+=dy;if(dx)emit(e,Kind::Relative,0,dx);if(dy)emit(e,Kind::Relative,1,dy);break;}
   case Kind::Relative:mouse.x+=e.a;mouse.y+=e.b;mouse.dx+=e.a;mouse.dy+=e.b;if(e.a)emit(e,e.kind,0,e.a);if(e.b)emit(e,e.kind,1,e.b);break;
   case Kind::Wheel:mouse.wheel+=e.a;mouse.horizontalWheel+=e.b;if(e.a)emit(e,e.kind,0,e.a);if(e.b)emit(e,e.kind,1,e.b);break;
   case Kind::Focus:if(e.a!=0&&e.a!=1)return false;focused=e.a!=0;break;
    case Kind::ResetMouse:
     for(int i=0;i<5;i++)if(mouse.buttons[i]){mouse.buttons[i]=false;emit(e,Kind::Button,i,0);}
     mouse={};++mouseEpoch;break;
    case Kind::ReleaseAll:
     for(int i=0;i<256;i++)if(keyboard.keys[i]){keyboard.set(i,false);emit(e,Kind::Key,i,0);}
     for(int i=0;i<5;i++)if(mouse.buttons[i]){mouse.buttons[i]=false;emit(e,Kind::Button,i,0);}
     keyboard.asyncPressed.fill(false);break;
    case Kind::Reset:case Kind::Stop:
    for(int i=0;i<256;i++)if(keyboard.keys[i]){keyboard.set(i,false);emit(e,Kind::Key,i,0);}
    for(int i=0;i<5;i++)if(mouse.buttons[i]){mouse.buttons[i]=false;emit(e,Kind::Button,i,0);}
     mouse={};++mouseEpoch;keyboard.toggled.fill(false);keyboard.asyncPressed.fill(false);break;
   case Kind::Snapshot:break;
   default:return false;
  }return true;
 }
};
// Serializes writes and observations; State itself is thread-confined.
class InputDevice {
 mutable std::mutex mutex;State state;
public:
 bool apply(const Event&e){std::lock_guard<std::mutex> lock(mutex);return state.apply(e);}
 State snapshot()const{std::lock_guard<std::mutex> lock(mutex);return state;}
 uint16_t async(int key){std::lock_guard<std::mutex> lock(mutex);return state.keyboard.async(key);}
 std::vector<Transition> read(size_t count,bool peek=false){std::lock_guard<std::mutex> lock(mutex);return state.events.read(count,peek);}
 void disconnect(){std::lock_guard<std::mutex> lock(mutex);state={};}
};
}