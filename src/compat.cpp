#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>
#include "runtime_api.hpp"
#include "compat_trace.hpp"
#include <deque>
#include <array>
#include <cstring>
#include <cstdlib>
#include <string>
// The pinned official SDL-generated header defines the exact versioned jump ABI.
struct SdlTable {
#define SDL_DYNAPI_PROC(rc,fn,params,args,ret) rc (SDLCALL *fn) params;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
};
namespace {
SdlTable native{};bool initialized=false,enabled=true,traceOnly=false,relative=false,f12=false;SDL_Window* window=nullptr;
std::array<Uint8,SDL_NUM_SCANCODES> keys{};std::deque<SDL_Event> pending;Uint32 buttons=0;int mouseX=320,mouseY=240,deltaX=0,deltaY=0;HviSnapshot previous;
unsigned long long polls=0,eventCount=0;std::string tracePath;
void queue(SDL_Event e){pending.push_back(e);++eventCount;}
Uint32 windowId(){return window?native.SDL_GetWindowID(window):0;}
void pump(){
 if(!initialized)return;
 DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
 bool hot=foregroundPid==GetCurrentProcessId()&&(GetAsyncKeyState(VK_F12)&0x8000)!=0;if(hot&&!f12){enabled=!enabled;HviSetVirtual(enabled);keys.fill(0);buttons=0;deltaX=deltaY=0;pending.clear();previous={};if(!enabled)native.SDL_SetRelativeMouseMode(relative?SDL_TRUE:SDL_FALSE);else native.SDL_SetRelativeMouseMode(SDL_FALSE);}f12=hot;
 if(!enabled)return;
 BYTE vk[256]{};HviGetKeyboardState(vk);std::array<Uint8,SDL_NUM_SCANCODES> next{};
 for(int k=0;k<256;++k)if(vk[k]&128){SDL_Scancode sc=native.SDL_GetScancodeFromKey(k>='A'&&k<='Z'?k+32:k);switch(k){case VK_LSHIFT:sc=SDL_SCANCODE_LSHIFT;break;case VK_RSHIFT:sc=SDL_SCANCODE_RSHIFT;break;case VK_LCONTROL:sc=SDL_SCANCODE_LCTRL;break;case VK_RCONTROL:sc=SDL_SCANCODE_RCTRL;break;case VK_LMENU:sc=SDL_SCANCODE_LALT;break;case VK_RMENU:sc=SDL_SCANCODE_RALT;break;case VK_RETURN:sc=SDL_SCANCODE_RETURN;break;case VK_ESCAPE:sc=SDL_SCANCODE_ESCAPE;break;case VK_SHIFT:case VK_CONTROL:case VK_MENU:sc=SDL_SCANCODE_UNKNOWN;break;}if(sc>0&&sc<SDL_NUM_SCANCODES)next[sc]=1;}
 for(int sc=1;sc<SDL_NUM_SCANCODES;++sc)if(keys[sc]!=next[sc]){SDL_Event e{};e.type=next[sc]?SDL_KEYDOWN:SDL_KEYUP;e.key.timestamp=native.SDL_GetTicks();e.key.windowID=windowId();e.key.state=next[sc]?SDL_PRESSED:SDL_RELEASED;e.key.keysym.scancode=static_cast<SDL_Scancode>(sc);e.key.keysym.sym=native.SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(sc));queue(e);}keys=next;
 HviSnapshot snapshot;HviGetSnapshot(&snapshot);
 if(snapshot.generation!=previous.generation){previous={};deltaX=deltaY=0;}
 int dx=static_cast<int>(std::clamp<int64_t>(snapshot.dx-previous.dx,-1000000,1000000)),dy=static_cast<int>(std::clamp<int64_t>(snapshot.dy-previous.dy,-1000000,1000000));
 if(dx||dy){deltaX+=dx;deltaY+=dy;mouseX=std::clamp(mouseX+dx,0,639);mouseY=std::clamp(mouseY+dy,0,479);SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=windowId();e.motion.timestamp=native.SDL_GetTicks();e.motion.x=mouseX;e.motion.y=mouseY;e.motion.xrel=dx;e.motion.yrel=dy;queue(e);}
 const Uint8 mapping[]{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT,SDL_BUTTON_MIDDLE,SDL_BUTTON_X1,SDL_BUTTON_X2};
 for(int i=0;i<5;++i){Uint32 mask=SDL_BUTTON(mapping[i]);bool down=snapshot.buttons[i]!=0;if(down!=bool(buttons&mask)){SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=windowId();e.button.timestamp=native.SDL_GetTicks();e.button.button=mapping[i];e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=mouseX;e.button.y=mouseY;e.button.clicks=1;queue(e);if(down)buttons|=mask;else buttons&=~mask;}}
 int wheel=static_cast<int>((snapshot.wheel-previous.wheel)/120);if(wheel){SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=windowId();e.wheel.timestamp=native.SDL_GetTicks();e.wheel.y=wheel;e.wheel.preciseY=static_cast<float>(wheel);queue(e);}previous=snapshot;
}
bool inputEvent(Uint32 t){return (t>=SDL_KEYDOWN&&t<=SDL_TEXTINPUT)||(t>=SDL_MOUSEMOTION&&t<=SDL_MOUSEWHEEL);}
int SDLCALL poll(SDL_Event* out){
 ++polls;native.SDL_PumpEvents();pump();
 // Preserve lifecycle/window events, suppress physical input only inside this process.
 SDL_Event e;while(native.SDL_PollEvent(&e)){if(!initialized||!enabled||!inputEvent(e.type)){if(out)*out=e;return 1;}}
 if(initialized&&enabled&&!pending.empty()){if(out){*out=pending.front();pending.pop_front();}return 1;}
 if(polls%30==0)compatTraceWrite(tracePath.c_str(),traceOnly?"TRACE_NATIVE":"B_SDL_DYNAPI",polls,eventCount);return 0;
}
const Uint8* SDLCALL keyboard(int* count){if(initialized&&enabled){pump();if(count)*count=SDL_NUM_SCANCODES;return keys.data();}return native.SDL_GetKeyboardState(count);}
Uint32 SDLCALL mouse(int*x,int*y){if(initialized&&enabled){pump();if(x)*x=mouseX;if(y)*y=mouseY;return buttons;}return native.SDL_GetMouseState(x,y);}
Uint32 SDLCALL relativeMouse(int*x,int*y){if(initialized&&enabled){pump();if(x)*x=deltaX;if(y)*y=deltaY;deltaX=deltaY=0;return buttons;}return native.SDL_GetRelativeMouseState(x,y);}
int SDLCALL setRelative(SDL_bool value){relative=value!=SDL_FALSE;if(initialized&&enabled)return 0;return native.SDL_SetRelativeMouseMode(value);}
SDL_bool SDLCALL getRelative(){return initialized&&enabled?(relative?SDL_TRUE:SDL_FALSE):native.SDL_GetRelativeMouseMode();}
SDL_Window* SDLCALL createWindow(const char*t,int x,int y,int w,int h,Uint32 f){window=native.SDL_CreateWindow(t,x,y,w,h,f);return window;}
int SDLCALL init(Uint32 flags){int rc=native.SDL_Init(flags);if(rc<0)return rc;
 if(!traceOnly){const char* id=getenv("HVI_COMPAT_SESSION");UINT session=id?static_cast<UINT>(strtoul(id,nullptr,10)):0;if(!session||!HviInitialize(session)){native.SDL_Quit();return native.SDL_SetError("HVI compatibility session initialization failed");}initialized=true;}
 return rc;
}
void SDLCALL quit(){if(initialized){HviShutdown();initialized=false;}compatTraceWrite(tracePath.c_str(),traceOnly?"TRACE_NATIVE":"B_SDL_DYNAPI",polls,eventCount);native.SDL_Quit();}
}
extern "C" __declspec(dllexport) Sint32 SDLCALL SDL_DYNAPI_entry(Uint32 version,void* table,Uint32 size){
 // SDL has not initialized its public dispatch table. Fetch the unmodified
 // implementation table directly; forwarding through public SDL would recurse.
 auto entry=reinterpret_cast<Sint32(SDLCALL*)(Uint32,void*,Uint32)>(GetProcAddress(GetModuleHandleW(L"SDL2.dll"),"SDL_DYNAPI_entry"));
 if(!entry||version!=1||size>sizeof(SdlTable)||entry(version,&native,sizeof(native))<0)return -1;
 const char* trace=getenv("HVI_COMPAT_TRACE");tracePath=trace?trace:"";traceOnly=getenv("HVI_TRACE_ONLY")!=nullptr;compatTraceInstall(GetModuleHandleW(L"SDL2.dll"));
 SdlTable altered=native;altered.SDL_Init=init;altered.SDL_Quit=quit;altered.SDL_PollEvent=poll;altered.SDL_CreateWindow=createWindow;
 if(!traceOnly){altered.SDL_GetKeyboardState=keyboard;altered.SDL_GetMouseState=mouse;altered.SDL_GetRelativeMouseState=relativeMouse;altered.SDL_SetRelativeMouseMode=setRelative;altered.SDL_GetRelativeMouseMode=getRelative;}
 std::memcpy(table,&altered,size);return 0;
}