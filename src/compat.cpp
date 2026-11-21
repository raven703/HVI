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
uint64_t cursor=0,lostEvents=0;DWORD lastSequence=0,lastError=0;bool queueOverflow=false;
constexpr size_t queueLimit=16384;
void queue(SDL_Event e){if(pending.size()==queueLimit){++lostEvents;queueOverflow=true;return;}pending.push_back(e);++eventCount;}
Uint32 windowId(){return window?native.SDL_GetWindowID(window):0;}
SDL_Scancode scan(int k){switch(k){case VK_LSHIFT:return SDL_SCANCODE_LSHIFT;case VK_RSHIFT:return SDL_SCANCODE_RSHIFT;case VK_LCONTROL:return SDL_SCANCODE_LCTRL;case VK_RCONTROL:return SDL_SCANCODE_RCTRL;case VK_LMENU:return SDL_SCANCODE_LALT;case VK_RMENU:return SDL_SCANCODE_RALT;case VK_RETURN:return SDL_SCANCODE_RETURN;case VK_ESCAPE:return SDL_SCANCODE_ESCAPE;case VK_SHIFT:case VK_CONTROL:case VK_MENU:return SDL_SCANCODE_UNKNOWN;}return native.SDL_GetScancodeFromKey(k>='A'&&k<='Z'?k+32:k);}
void keyChange(int k,bool down){auto sc=scan(k);if(sc<=0||sc>=SDL_NUM_SCANCODES||bool(keys[sc])==down)return;keys[sc]=down;SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.timestamp=native.SDL_GetTicks();e.key.windowID=windowId();e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.scancode=sc;e.key.keysym.sym=native.SDL_GetKeyFromScancode(sc);e.key.keysym.mod=(keys[SDL_SCANCODE_LSHIFT]?KMOD_LSHIFT:0)|(keys[SDL_SCANCODE_RSHIFT]?KMOD_RSHIFT:0)|(keys[SDL_SCANCODE_LCTRL]?KMOD_LCTRL:0)|(keys[SDL_SCANCODE_RCTRL]?KMOD_RCTRL:0)|(keys[SDL_SCANCODE_LALT]?KMOD_LALT:0)|(keys[SDL_SCANCODE_RALT]?KMOD_RALT:0);queue(e);}
const Uint8 buttonMapping[]{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT,SDL_BUTTON_MIDDLE,SDL_BUTTON_X1,SDL_BUTTON_X2};
void buttonChange(int i,bool down){if(i<0||i>=5)return;Uint32 mask=SDL_BUTTON(buttonMapping[i]);if(bool(buttons&mask)==down)return;if(down)buttons|=mask;else buttons&=~mask;SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=windowId();e.button.timestamp=native.SDL_GetTicks();e.button.button=buttonMapping[i];e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=mouseX;e.button.y=mouseY;e.button.clicks=1;queue(e);}
void resetMouse(){for(int i=0;i<5;++i)buttonChange(i,false);mouseX=mouseY=deltaX=deltaY=0;}
void resetAll(){for(int k=0;k<256;++k)keyChange(k,false);resetMouse();}
void motion(int x,int y,bool absolute){int width=1,height=1;if(window)native.SDL_GetWindowSize(window,&width,&height);int dx=absolute?x-mouseX:x,dy=absolute?y-mouseY:y;mouseX=std::clamp(absolute?x:mouseX+x,0,std::max(0,width-1));mouseY=std::clamp(absolute?y:mouseY+y,0,std::max(0,height-1));deltaX+=dx;deltaY+=dy;SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=windowId();e.motion.timestamp=native.SDL_GetTicks();e.motion.state=buttons;e.motion.x=mouseX;e.motion.y=mouseY;e.motion.xrel=dx;e.motion.yrel=dy;queue(e);}
void command(const hvi::Event& c){switch(c.kind){case hvi::Kind::Key:{int k=c.a;if(k==VK_SHIFT)k=VK_LSHIFT;if(k==VK_CONTROL)k=VK_LCONTROL;if(k==VK_MENU)k=VK_LMENU;keyChange(k,c.b!=0);break;}case hvi::Kind::Button:buttonChange(c.a,c.b!=0);break;case hvi::Kind::Relative:motion(c.a,c.b,false);break;case hvi::Kind::Absolute:motion(c.a,c.b,true);break;case hvi::Kind::ReleaseAll:for(int k=0;k<256;++k)keyChange(k,false);for(int i=0;i<5;++i)buttonChange(i,false);break;case hvi::Kind::Reset:case hvi::Kind::Stop:resetAll();break;case hvi::Kind::ResetMouse:resetMouse();break;case hvi::Kind::Wheel:{SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=windowId();e.wheel.timestamp=native.SDL_GetTicks();e.wheel.x=c.b/120;e.wheel.y=c.a/120;e.wheel.preciseX=c.b/120.0f;e.wheel.preciseY=c.a/120.0f;queue(e);break;}default:break;}}
void recover(const HviJournalRead& r){pending.clear();queueOverflow=false;resetAll();for(int k=0;k<256;++k)if(r.keyboard[k]&128)keyChange(k,true);for(int i=0;i<5;++i)buttonChange(i,r.buttons[i]!=0);motion(static_cast<int>(r.x),static_cast<int>(r.y),true);deltaX=deltaY=0;lastError=ERROR_BUFFER_OVERFLOW;}
void pump(){
 if(!initialized)return;
 DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
 bool hot=foregroundPid==GetCurrentProcessId()&&(GetAsyncKeyState(VK_F12)&0x8000)!=0;if(hot&&!f12){enabled=!enabled;HviSetVirtual(enabled);keys.fill(0);buttons=0;deltaX=deltaY=0;pending.clear();previous={};if(!enabled)native.SDL_SetRelativeMouseMode(relative?SDL_TRUE:SDL_FALSE);else native.SDL_SetRelativeMouseMode(SDL_FALSE);}f12=hot;
 if(!enabled)return;
 HviJournalRecord records[512];HviJournalRead read;read.cursor=cursor;
 do{if(!HviReadJournal(&read,records,512)){lastError=GetLastError();break;}cursor=read.cursor;if(read.resync){lostEvents+=read.lost;recover(read);break;}for(DWORD i=0;i<read.count;++i){lastSequence=records[i].command.sequence;command(records[i].command);}if(queueOverflow){lostEvents+=pending.size();read.cursor=read.latest;cursor=read.latest;recover(read);break;}}while(read.count==512);
 native.SDL_SetHint("HVI_DIAG_LOST",std::to_string(lostEvents).c_str());native.SDL_SetHint("HVI_DIAG_ORDER",std::to_string(cursor).c_str());native.SDL_SetHint("HVI_DIAG_SEQUENCE",std::to_string(lastSequence).c_str());native.SDL_SetHint("HVI_DIAG_QUEUE",std::to_string(pending.size()).c_str());native.SDL_SetHint("HVI_DIAG_ERROR",std::to_string(lastError).c_str());
}
bool inputEvent(Uint32 t){return (t>=SDL_KEYDOWN&&t<=SDL_TEXTINPUT)||(t>=SDL_MOUSEMOTION&&t<=SDL_MOUSEWHEEL);}
void SDLCALL pumpEvents(){native.SDL_PumpEvents();pump();if(!initialized||!enabled)return;SDL_Event e;while(native.SDL_PeepEvents(&e,1,SDL_GETEVENT,SDL_FIRSTEVENT,SDL_LASTEVENT)>0)if(!inputEvent(e.type))queue(e);}
int SDLCALL peep(SDL_Event* out,int n,SDL_eventaction action,Uint32 first,Uint32 last){
 if(!initialized||!enabled)return native.SDL_PeepEvents(out,n,action,first,last);
 if(n<0||first>last)return native.SDL_SetError("Invalid event query");
 if(action==SDL_ADDEVENT){if(!out&&n)return native.SDL_SetError("Null event array");int added=0;for(;added<n&&pending.size()<queueLimit;++added)queue(out[added]);return added;}
 if(action!=SDL_GETEVENT&&action!=SDL_PEEKEVENT)return native.SDL_SetError("Invalid event action");
 int found=0;for(auto it=pending.begin();it!=pending.end();){if(it->type>=first&&it->type<=last){if(!out){++found;++it;continue;}if(found==n)break;out[found++]=*it;if(action==SDL_GETEVENT){it=pending.erase(it);continue;}}++it;}return found;
}
int SDLCALL poll(SDL_Event* out){
 ++polls;pumpEvents();if(!initialized||!enabled)return native.SDL_PollEvent(out);
 if(!pending.empty()){if(out){*out=pending.front();pending.pop_front();}return 1;}
 if(polls%30==0)compatTraceWrite(tracePath.c_str(),traceOnly?"TRACE_NATIVE":"B_SDL_DYNAPI",polls,eventCount);return 0;
}
int SDLCALL waitTimeout(SDL_Event* out,int timeout){if(!initialized||!enabled)return native.SDL_WaitEventTimeout(out,timeout);Uint64 start=native.SDL_GetTicks64();for(;;){if(poll(out))return 1;if(timeout>=0&&native.SDL_GetTicks64()-start>=static_cast<Uint64>(timeout))return 0;native.SDL_Delay(1);}}
int SDLCALL waitEvent(SDL_Event* out){return waitTimeout(out,-1);}
const Uint8* SDLCALL keyboard(int* count){if(initialized&&enabled){pump();if(count)*count=SDL_NUM_SCANCODES;return keys.data();}return native.SDL_GetKeyboardState(count);}
Uint32 SDLCALL mouse(int*x,int*y){if(initialized&&enabled){pump();if(x)*x=mouseX;if(y)*y=mouseY;return buttons;}return native.SDL_GetMouseState(x,y);}
Uint32 SDLCALL relativeMouse(int*x,int*y){if(initialized&&enabled){pump();if(x)*x=deltaX;if(y)*y=deltaY;deltaX=deltaY=0;return buttons;}return native.SDL_GetRelativeMouseState(x,y);}
int SDLCALL setRelative(SDL_bool value){relative=value!=SDL_FALSE;if(initialized&&enabled)return 0;return native.SDL_SetRelativeMouseMode(value);}
SDL_bool SDLCALL getRelative(){return initialized&&enabled?(relative?SDL_TRUE:SDL_FALSE):native.SDL_GetRelativeMouseMode();}
SDL_Window* SDLCALL createWindow(const char*t,int x,int y,int w,int h,Uint32 f){window=native.SDL_CreateWindow(t,x,y,w,h,f);return window;}
int startRuntime(){
 if(!traceOnly&&!initialized){const char* id=getenv("HVI_COMPAT_SESSION");UINT session=id?static_cast<UINT>(strtoul(id,nullptr,10)):0;if(!session||!HviInitialize(session)){native.SDL_Quit();return native.SDL_SetError("HVI compatibility session initialization failed");}HviJournalRead r;HviReadJournal(&r,nullptr,0);cursor=r.latest;pending.clear();keys.fill(0);buttons=0;deltaX=deltaY=0;enabled=true;initialized=true;}
 return 0;
}
int SDLCALL init(Uint32 flags){int rc=native.SDL_Init(flags);return rc<0?rc:startRuntime();}
int SDLCALL videoInit(const char* driver){int rc=native.SDL_VideoInit(driver);return rc<0?rc:startRuntime();}
void SDLCALL quit(){if(initialized){HviShutdown();initialized=false;}pending.clear();keys.fill(0);buttons=0;window=nullptr;compatTraceWrite(tracePath.c_str(),traceOnly?"TRACE_NATIVE":"B_SDL_DYNAPI",polls,eventCount);native.SDL_Quit();}
}
extern "C" __declspec(dllexport) Sint32 SDLCALL SDL_DYNAPI_entry(Uint32 version,void* table,Uint32 size){
 // SDL has not initialized its public dispatch table. Fetch the unmodified
 // implementation table directly; forwarding through public SDL would recurse.
 auto entry=reinterpret_cast<Sint32(SDLCALL*)(Uint32,void*,Uint32)>(GetProcAddress(GetModuleHandleW(L"SDL2.dll"),"SDL_DYNAPI_entry"));
 if(!entry||version!=1||size>sizeof(SdlTable)||entry(version,&native,sizeof(native))<0)return -1;
 const char* trace=getenv("HVI_COMPAT_TRACE");tracePath=trace?trace:"";traceOnly=getenv("HVI_TRACE_ONLY")!=nullptr;compatTraceInstall(GetModuleHandleW(L"SDL2.dll"));
 SdlTable altered=native;altered.SDL_Init=init;altered.SDL_VideoInit=videoInit;altered.SDL_Quit=quit;altered.SDL_PollEvent=poll;altered.SDL_CreateWindow=createWindow;
 if(!traceOnly){altered.SDL_PumpEvents=pumpEvents;altered.SDL_PeepEvents=peep;altered.SDL_WaitEvent=waitEvent;altered.SDL_WaitEventTimeout=waitTimeout;altered.SDL_GetKeyboardState=keyboard;altered.SDL_GetMouseState=mouse;altered.SDL_GetRelativeMouseState=relativeMouse;altered.SDL_SetRelativeMouseMode=setRelative;altered.SDL_GetRelativeMouseMode=getRelative;}
 std::memcpy(table,&altered,size);return 0;
}