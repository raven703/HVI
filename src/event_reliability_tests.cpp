#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "runtime_transport.hpp"
#include <iostream>
int main(){
 SetEnvironmentVariableA("SDL_DYNAMIC_API","hvi_compat.dll");SetEnvironmentVariableA("HVI_COMPAT_SESSION","93");SDL_SetMainReady();int errors=0;
 auto check=[&](bool b,const char*s){if(!b){++errors;std::cerr<<s<<"\n";}};
 for(int cycle=0;cycle<2;++cycle){if(SDL_Init(SDL_INIT_VIDEO)!=0){std::cerr<<SDL_GetError()<<"\n";return 1;}auto*w=SDL_CreateWindow("Event reliability",0,0,800,600,SDL_WINDOW_HIDDEN);hvi::RuntimeTransport t(93);if(!t.send(hvi::Kind::Snapshot)){std::cerr<<"connect "<<t.lastError<<"\n";return 1;}SDL_Event e;while(SDL_PollEvent(&e)){}
 for(int i=0;i<1000;++i){check(t.send(hvi::Kind::Key,'W',1)&&t.send(hvi::Kind::Key,'W',1)&&t.send(hvi::Kind::Key,'W',0)&&t.send(hvi::Kind::Button,0,1)&&t.send(hvi::Kind::Button,0,0),"burst transport");}
 SDL_PumpEvents();check(SDL_PollEvent(nullptr)==1&&SDL_PollEvent(nullptr)==1,"null poll retains event");
 int kd=0,ku=0,md=0,mu=0;SDL_Event peek[8];check(SDL_PeepEvents(peek,8,SDL_PEEKEVENT,SDL_KEYDOWN,SDL_KEYUP)==8,"peek filter");
 while(SDL_PollEvent(&e)){kd+=e.type==SDL_KEYDOWN;ku+=e.type==SDL_KEYUP;md+=e.type==SDL_MOUSEBUTTONDOWN;mu+=e.type==SDL_MOUSEBUTTONUP;}
 check(kd==1000&&ku==1000&&md==1000&&mu==1000,"1000 ordered key/click pairs");check(SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W]==0,"released state");
 if(!t.send(hvi::Kind::Snapshot)){Sleep(20);check(t.send(hvi::Kind::Snapshot),"reconnect after drain idle");}while(SDL_PollEvent(&e)){}
 check(t.send(hvi::Kind::Wheel,120,-120),"wheel send");SDL_PumpEvents();check(SDL_PeepEvents(&e,1,SDL_GETEVENT,SDL_MOUSEWHEEL,SDL_MOUSEWHEEL)==1&&e.wheel.x==-1&&e.wheel.y==1,"horizontal vertical wheel");
 t.send(hvi::Kind::Absolute,750,550);check(SDL_WaitEvent(&e)&&e.type==SDL_MOUSEMOTION&&e.motion.x==750&&e.motion.y==550,"absolute resized coordinates");
 // Force upstream loss without pumping; recover to held snapshot, never replay tail.
 for(int i=0;i<17000;++i)t.send(hvi::Kind::Key,'A',i%2==0);t.send(hvi::Kind::Key,'W',1);SDL_PumpEvents();check(SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W],"overflow snapshot recovery");
 t.disconnect();Sleep(50);SDL_PumpEvents();check(!SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W],"disconnect release");
 SDL_DestroyWindow(w);SDL_Quit();}
 std::cout<<"event reliability errors="<<errors<<"\n";return errors?1:0;
}