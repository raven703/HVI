// Independent SDL2 test object. No HVI headers/API/session/transport.
// Uses the standard poll/render loop described in SDL2's public examples.
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
int main(int argc,char**argv){
 std::string title="SDL Input Test",observe;bool relative=false;
 for(int i=1;i<argc;++i){if(!strcmp(argv[i],"--title")&&i+1<argc)title=argv[++i];else if(!strcmp(argv[i],"--observe")&&i+1<argc)observe=argv[++i];else if(!strcmp(argv[i],"--relative"))relative=true;}
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0)return 1;
 SDL_Window* window=SDL_CreateWindow(title.c_str(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_SHOWN);
 if(!window){SDL_Quit();return 2;}
 SDL_Renderer* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer){SDL_DestroyWindow(window);SDL_Quit();return 3;}
 SDL_SetRelativeMouseMode(relative?SDL_TRUE:SDL_FALSE);
 float x=320,y=240,aimX=320,aimY=240,scale=1;bool alternate=false,colored=false,quit=false;Uint64 frames=0;Uint32 last=SDL_GetTicks(),lastReport=0,events=0;double fps=0;
 auto started=SDL_GetPerformanceCounter();auto frequency=SDL_GetPerformanceFrequency();
 unsigned long long keyDown=0,keyUp=0,mouseDown=0,mouseUp=0;
 while(!quit){
  SDL_Event event;
  while(SDL_PollEvent(&event)){
   ++events;if(event.type==SDL_QUIT)quit=true;
   keyDown+=event.type==SDL_KEYDOWN;keyUp+=event.type==SDL_KEYUP;mouseDown+=event.type==SDL_MOUSEBUTTONDOWN;mouseUp+=event.type==SDL_MOUSEBUTTONUP;
   if(event.type==SDL_KEYDOWN&&!event.key.repeat){if(event.key.keysym.sym==SDLK_ESCAPE)quit=true;if(event.key.keysym.sym==SDLK_r){relative=!relative;SDL_SetRelativeMouseMode(relative?SDL_TRUE:SDL_FALSE);}}
   if(event.type==SDL_MOUSEBUTTONDOWN){if(event.button.button==SDL_BUTTON_LEFT)colored=!colored;if(event.button.button==SDL_BUTTON_RIGHT)alternate=!alternate;}
   if(event.type==SDL_MOUSEWHEEL)scale=std::clamp(scale+event.wheel.y*0.1f,0.2f,3.0f);
  }
  Uint32 now=SDL_GetTicks();float dt=std::min(0.05f,(now-last)/1000.0f);last=now;
  const Uint8* keys=SDL_GetKeyboardState(nullptr);float speed=(keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT])?240.0f:100.0f;if(alternate)speed=-speed;
  x=std::clamp(x+(keys[SDL_SCANCODE_D]-keys[SDL_SCANCODE_A])*speed*dt,20.0f,620.0f);y=std::clamp(y+(keys[SDL_SCANCODE_S]-keys[SDL_SCANCODE_W])*speed*dt,20.0f,460.0f);
  int mx=0,my=0;Uint32 buttons;if(relative){buttons=SDL_GetRelativeMouseState(&mx,&my);aimX=std::clamp(aimX+mx,0.0f,640.0f);aimY=std::clamp(aimY+my,0.0f,480.0f);}else {buttons=SDL_GetMouseState(&mx,&my);aimX=static_cast<float>(mx);aimY=static_cast<float>(my);}
  SDL_SetRenderDrawColor(renderer,22,26,36,255);SDL_RenderClear(renderer);SDL_SetRenderDrawColor(renderer,colored?240:70,colored?110:200,100,255);int size=static_cast<int>(30*scale);SDL_Rect square{static_cast<int>(x)-size/2,static_cast<int>(y)-size/2,size,size};SDL_RenderFillRect(renderer,&square);
  SDL_SetRenderDrawColor(renderer,250,220,80,255);SDL_RenderDrawLine(renderer,static_cast<int>(aimX)-10,static_cast<int>(aimY),static_cast<int>(aimX)+10,static_cast<int>(aimY));SDL_RenderDrawLine(renderer,static_cast<int>(aimX),static_cast<int>(aimY)-10,static_cast<int>(aimX),static_cast<int>(aimY)+10);SDL_RenderPresent(renderer);++frames;
  if(now-lastReport>=100){
   fps=frames*static_cast<double>(frequency)/std::max<Uint64>(1,SDL_GetPerformanceCounter()-started);bool focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
   char text[512];snprintf(text,sizeof(text),"%s | %.0f FPS | square %.1f,%.1f | aim %.0f,%.0f | WASD=%d%d%d%d Shift=%d | focus=%d relative=%d scale=%.1f | R=relative F12=compat toggle",title.c_str(),fps,x,y,aimX,aimY,keys[SDL_SCANCODE_W],keys[SDL_SCANCODE_A],keys[SDL_SCANCODE_S],keys[SDL_SCANCODE_D],keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT],focused,relative,scale);SDL_SetWindowTitle(window,text);
   auto hint=[](const char* name){const char* value=SDL_GetHint(name);return value?value:"0";};
   char diagnostics[512];snprintf(diagnostics,sizeof(diagnostics)," | KD/KU=%llu/%llu MD/MU=%llu/%llu lost=%s seq=%s q=%s err=%s",keyDown,keyUp,mouseDown,mouseUp,hint("HVI_DIAG_LOST"),hint("HVI_DIAG_SEQUENCE"),hint("HVI_DIAG_QUEUE"),hint("HVI_DIAG_ERROR"));std::string full=std::string(text)+diagnostics;SDL_SetWindowTitle(window,full.c_str());
   if(!observe.empty()){
    // Read-only results: one-way output file, never read for commands. Atomic
    // replace is deliberately avoided here to use only SDL plus standard C++.
    SDL_RWops* out=SDL_RWFromFile(observe.c_str(),"wb");if(out){char data[1024];int n=snprintf(data,sizeof(data),"{\"frames\":%llu,\"x\":%.3f,\"y\":%.3f,\"aimX\":%.3f,\"aimY\":%.3f,\"scale\":%.3f,\"color\":%d,\"alternate\":%d,\"focus\":%d,\"relative\":%d,\"w\":%d,\"s\":%d,\"d\":%d,\"buttons\":%u,\"events\":%u}\n",static_cast<unsigned long long>(frames),x,y,aimX,aimY,scale,colored,alternate,focused,relative,keys[SDL_SCANCODE_W],keys[SDL_SCANCODE_S],keys[SDL_SCANCODE_D],buttons,events);SDL_RWwrite(out,data,1,n);SDL_RWclose(out);}
   }lastReport=now;
  }
  SDL_Delay(8);
 }
 SDL_SetRelativeMouseMode(SDL_FALSE);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}