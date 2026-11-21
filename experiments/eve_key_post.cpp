#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>

namespace {
struct Window {HWND hwnd;DWORD pid,tid;};
std::vector<Window> recipients;
std::map<HWND,std::set<UINT>> held;
struct Mouse {LONG x=0,y=0;UINT buttons=0;bool positioned=false;};
std::map<HWND,Mouse> mice;
std::wstring lastAction=L"none";
std::ofstream logfile;
bool testing=false;
std::string utf8(const std::wstring& s){
 if(s.empty())return {};
 int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
 std::string out(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);return out;
}
void log(const std::wstring& text){
 SYSTEMTIME t{};GetSystemTime(&t);wchar_t time[40];
 swprintf(time,40,L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
 std::wcout<<time<<text<<L'\n';
 if(logfile){logfile<<utf8(std::wstring(time)+text)<<'\n';logfile.flush();}
}
std::wstring handle(HWND hwnd){return std::to_wstring(reinterpret_cast<ULONG_PTR>(hwnd));}
bool client(DWORD pid){
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 if(snapshot==INVALID_HANDLE_VALUE)return false;
 PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;
 if(Process32FirstW(snapshot,&entry))do{
  if(entry.th32ProcessID==pid){found=_wcsicmp(entry.szExeFile,L"exefile.exe")==0;break;}
 }while(Process32NextW(snapshot,&entry));
 CloseHandle(snapshot);return found;
}
BOOL CALLBACK enumerate(HWND hwnd,LPARAM param){
 DWORD pid=0;DWORD tid=GetWindowThreadProcessId(hwnd,&pid);wchar_t cls[128]{};GetClassNameW(hwnd,cls,128);
 if(IsWindowVisible(hwnd)&&std::wstring(cls)==L"trinityWindow"&&client(pid))
  reinterpret_cast<std::vector<Window>*>(param)->push_back({hwnd,pid,tid});
 return TRUE;
}
bool valid(const Window& window){
 DWORD pid=0;DWORD tid=GetWindowThreadProcessId(window.hwnd,&pid);
 if(!window.hwnd||!IsWindow(window.hwnd)||pid!=window.pid||tid!=window.tid)return false;
 wchar_t cls[128]{};GetClassNameW(window.hwnd,cls,128);
 return testing?(pid==GetCurrentProcessId()&&std::wstring(cls)==L"HviPostOwnedTest"):
  (std::wstring(cls)==L"trinityWindow"&&client(pid));
}
bool post(const Window& window,UINT key,bool down){
 HWND before=GetForegroundWindow();
 DWORD error=0;BOOL ok=FALSE;
 bool previous=held[window.hwnd].count(key)!=0;
 UINT scan=MapVirtualKeyExW(key,MAPVK_VK_TO_VSC,GetKeyboardLayout(window.tid));
 DWORD bits=1|((scan&255)<<16);
 if(!down)bits|=0xc0000000u;else if(previous)bits|=0x40000000u;
 if(!valid(window))error=ERROR_INVALID_WINDOW_HANDLE;
 else if(!scan)error=ERROR_INVALID_PARAMETER;
 else{SetLastError(0);ok=PostMessageW(window.hwnd,down?WM_KEYDOWN:WM_KEYUP,key,static_cast<LPARAM>(bits));if(!ok)error=GetLastError();}
 if(ok){if(down)held[window.hwnd].insert(key);else held[window.hwnd].erase(key);}
 HWND after=GetForegroundWindow();
 log(L"POST pid="+std::to_wstring(window.pid)+L" hwnd="+handle(window.hwnd)+L" vk="+std::to_wstring(key)+
  (down?L" WM_KEYDOWN":L" WM_KEYUP")+L" lparam="+std::to_wstring(bits)+
  L" queue_result="+(ok?L"QUEUED":L"FAILED")+L" win32_error="+std::to_wstring(error)+
  L" foreground_before="+handle(before)+L" foreground_after="+handle(after)+L" game_processing=UNKNOWN");
 return ok!=FALSE;
}
bool send(const std::vector<Window>& selected,UINT key,const std::wstring& action){
 bool ok=true;
 if(action==L"press"){
  for(const auto& window:selected)if(held[window.hwnd].count(key)){log(L"REFUSED press on held key; use up or release first");return false;}
  for(const auto& window:selected)ok=post(window,key,true)&&ok;
  Sleep(80);
  for(const auto& window:selected)ok=post(window,key,false)&&ok;
 }else for(const auto& window:selected)ok=post(window,key,action==L"down")&&ok;
 return ok;
}
bool fits(LONG n){return n>=-32768&&n<=32767;}
LPARAM coordinates(LONG x,LONG y){return static_cast<LPARAM>(static_cast<DWORD>(static_cast<WORD>(x))|(static_cast<DWORD>(static_cast<WORD>(y))<<16));}
bool inside(const Window& w,LONG x,LONG y){RECT r{};return valid(w)&&GetClientRect(w.hwnd,&r)&&x>=0&&y>=0&&x<r.right&&y<r.bottom&&fits(x)&&fits(y);}
bool position(const std::vector<Window>& selection,LONG x,LONG y){
 for(auto w:selection)if(!inside(w,x,y)){log(L"REFUSED pos: outside client area or invalid HWND");return false;}
 for(auto w:selection){auto& m=mice[w.hwnd];m.x=x;m.y=y;m.positioned=true;log(L"POSITION hwnd="+handle(w.hwnd)+L" client_x="+std::to_wstring(x)+L" client_y="+std::to_wstring(y));}return true;
}
const wchar_t* mouseName(UINT msg){
 switch(msg){case WM_MOUSEMOVE:return L"WM_MOUSEMOVE";case WM_LBUTTONDOWN:return L"WM_LBUTTONDOWN";case WM_LBUTTONUP:return L"WM_LBUTTONUP";case WM_RBUTTONDOWN:return L"WM_RBUTTONDOWN";case WM_RBUTTONUP:return L"WM_RBUTTONUP";default:return L"WM_MOUSEWHEEL";}
}
bool mousePost(const Window& w,UINT msg,int delta=0){
 auto& m=mice[w.hwnd];UINT buttons=m.buttons;
 bool down=msg==WM_LBUTTONDOWN||msg==WM_RBUTTONDOWN,up=msg==WM_LBUTTONUP||msg==WM_RBUTTONUP;
 UINT bit=(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP)?MK_LBUTTON:MK_RBUTTON;
 if(down)buttons|=bit;if(up)buttons&=~bit;
 HWND before=GetForegroundWindow();POINT cursorBefore{},cursorAfter{};BOOL cb=GetCursorPos(&cursorBefore);
 POINT point{m.x,m.y};DWORD error=0;BOOL ok=FALSE;
 if(!valid(w))error=ERROR_INVALID_WINDOW_HANDLE;
 else if(!m.positioned||(!up&&!inside(w,m.x,m.y)))error=ERROR_INVALID_PARAMETER;
 else if(msg==WM_MOUSEWHEEL&&(delta==0||!fits(delta)))error=ERROR_INVALID_PARAMETER;
 else if(msg==WM_MOUSEWHEEL&&!ClientToScreen(w.hwnd,&point)){error=GetLastError();if(!error)error=ERROR_INVALID_PARAMETER;}
 if(!error&&(!fits(point.x)||!fits(point.y)))error=ERROR_INVALID_PARAMETER;
 WPARAM wp=buttons;if(msg==WM_MOUSEWHEEL)wp|=static_cast<WPARAM>(static_cast<WORD>(delta))<<16;
 LPARAM lp=coordinates(point.x,point.y);
 if(!error){SetLastError(0);ok=PostMessageW(w.hwnd,msg,wp,lp);if(!ok)error=GetLastError();}
 if(ok)m.buttons=buttons;
 HWND after=GetForegroundWindow();BOOL ca=GetCursorPos(&cursorAfter);
 log(L"MOUSE_POST pid="+std::to_wstring(w.pid)+L" hwnd="+handle(w.hwnd)+L" "+mouseName(msg)+
  L" client_x="+std::to_wstring(m.x)+L" client_y="+std::to_wstring(m.y)+L" message_x="+std::to_wstring(point.x)+L" message_y="+std::to_wstring(point.y)+
  L" wparam="+std::to_wstring(wp)+L" lparam="+std::to_wstring(lp)+L" wheel_delta="+std::to_wstring(delta)+
  L" queue_result="+(ok?L"QUEUED":L"FAILED")+L" win32_error="+std::to_wstring(error)+
  L" foreground_before="+handle(before)+L" foreground_after="+handle(after)+L" target_foreground="+std::to_wstring(before==w.hwnd)+
  L" cursor_before="+std::to_wstring(cursorBefore.x)+L","+std::to_wstring(cursorBefore.y)+L" cursor_after="+std::to_wstring(cursorAfter.x)+L","+std::to_wstring(cursorAfter.y)+
  L" cursor_sample_ok="+std::to_wstring(cb&&ca)+L" game_processing=UNKNOWN");return ok!=FALSE;
}
bool mouseSend(const std::vector<Window>& selection,const std::wstring& action,int delta=0){
 bool click=action==L"lclick"||action==L"rclick";UINT bit=action==L"lclick"?MK_LBUTTON:MK_RBUTTON;
 for(auto w:selection)if(!mice[w.hwnd].positioned||(click&&(mice[w.hwnd].buttons&bit))){log(L"REFUSED mouse: set pos first; release held button before click");return false;}
 if(click){
  bool ok=true;std::vector<Window> posted;
  for(auto w:selection){if(mousePost(w,bit==MK_LBUTTON?WM_LBUTTONDOWN:WM_RBUTTONDOWN))posted.push_back(w);else ok=false;}
  Sleep(80);for(auto w:posted)ok=mousePost(w,bit==MK_LBUTTON?WM_LBUTTONUP:WM_RBUTTONUP)&&ok;return ok;
 }
 UINT msg=action==L"move"?WM_MOUSEMOVE:action==L"ldown"?WM_LBUTTONDOWN:action==L"lup"?WM_LBUTTONUP:action==L"rdown"?WM_RBUTTONDOWN:action==L"rup"?WM_RBUTTONUP:WM_MOUSEWHEEL;
 bool ok=true;for(auto w:selection)ok=mousePost(w,msg,delta)&&ok;return ok;
}
bool release(){
 bool ok=true;
 for(const auto& window:recipients){auto keys=held[window.hwnd];for(UINT key:keys)ok=post(window,key,false)&&ok;}
 for(auto w:recipients){if(mice[w.hwnd].buttons&MK_LBUTTON)ok=mousePost(w,WM_LBUTTONUP)&&ok;if(mice[w.hwnd].buttons&MK_RBUTTON)ok=mousePost(w,WM_RBUTTONUP)&&ok;}
 log(L"Release All: KEYUP/button UP attempted for all tracked holds, regardless of current selection; processing UNKNOWN");return ok;
}
void list(){
 for(size_t i=0;i<recipients.size();++i){const auto& w=recipients[i];
  log(L"WINDOW "+std::to_wstring(i+1)+(i==0?L" A":i==1?L" B":L"")+L" pid="+std::to_wstring(w.pid)+L" hwnd="+handle(w.hwnd)+
   L" tid="+std::to_wstring(w.tid)+L" minimized="+std::to_wstring(IsIconic(w.hwnd)!=FALSE)+L" identity=name/class candidate");
  RECT r{};GetClientRect(w.hwnd,&r);const auto& m=mice[w.hwnd];
  log(L"MOUSE_STATE hwnd="+handle(w.hwnd)+L" client_width="+std::to_wstring(r.right)+L" client_height="+std::to_wstring(r.bottom)+L" positioned="+std::to_wstring(m.positioned)+L" x="+std::to_wstring(m.x)+L" y="+std::to_wstring(m.y)+L" buttons="+std::to_wstring(m.buttons));
 }
 if(recipients.empty())log(L"No visible top-level trinityWindow/exefile.exe candidates found.");
}
void discover(){
 std::vector<Window> found;if(!EnumWindows(enumerate,reinterpret_cast<LPARAM>(&found)))throw std::runtime_error("EnumWindows failed");
 std::sort(found.begin(),found.end(),[](const Window&a,const Window&b){return a.pid<b.pid||(a.pid==b.pid&&reinterpret_cast<ULONG_PTR>(a.hwnd)<reinterpret_cast<ULONG_PTR>(b.hwnd));});
 recipients=std::move(found);held.clear();mice.clear();list();
}
struct Received {HWND hwnd;UINT message,key;DWORD bits;};
std::vector<Received> received;
LRESULT CALLBACK testProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_KEYDOWN||m==WM_KEYUP||m==WM_MOUSEMOVE||m==WM_LBUTTONDOWN||m==WM_LBUTTONUP||m==WM_RBUTTONDOWN||m==WM_RBUTTONUP||m==WM_MOUSEWHEEL){received.push_back({h,m,static_cast<UINT>(w),static_cast<DWORD>(l)});return 0;}
 return DefWindowProcW(h,m,w,l);
}
void pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE))DispatchMessageW(&msg);}
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int selfTest(){
 testing=true;WNDCLASSW cls{};cls.lpfnWndProc=testProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"HviPostOwnedTest";
 require(RegisterClassW(&cls)!=0,"test class");HWND original=GetForegroundWindow();
 for(int i=0;i<2;++i){HWND h=CreateWindowW(cls.lpszClassName,L"Owned hidden receiver",WS_POPUP,-100+i*800,100,640,480,nullptr,nullptr,cls.hInstance,nullptr);require(h!=nullptr,"test window");recipients.push_back({h,GetCurrentProcessId(),GetCurrentThreadId()});}
 auto a=recipients[0],b=recipients[1];
 for(const auto& selection:std::vector<std::vector<Window>>{{a},{b},{a,b}}){
  received.clear();require(send(selection,VK_ESCAPE,L"press"),"post press");pump();require(received.size()==selection.size()*2,"A/B/BOTH receipt count");
  for(const auto& window:selection){std::vector<Received> events;for(auto e:received)if(e.hwnd==window.hwnd)events.push_back(e);require(events.size()==2&&events[0].message==WM_KEYDOWN&&events[1].message==WM_KEYUP,"ordered down/up per recipient");}
  for(const auto& event:received){require(event.key==VK_ESCAPE,"key identity");require(std::any_of(selection.begin(),selection.end(),[&](const Window&w){return w.hwnd==event.hwnd;}),"isolation");require((event.bits&65535)==1,"repeat count");require(((event.bits>>16)&255)!=0,"scan code");require((event.bits>>30)==(event.message==WM_KEYUP?3u:0u),"transition flags");}
 }
 received.clear();require(send({a},'A',L"down"),"down");require(send({b},'W',L"down"),"down B");require(send({a},'A',L"down"),"repeat down");pump();require((received.back().bits&0x40000000u)!=0,"repeat previous state");
 received.clear();require(release(),"release all");pump();require(received.size()==2&&received[0].message==WM_KEYUP&&received[1].message==WM_KEYUP,"release receipt");require(held[a.hwnd].empty()&&held[b.hwnd].empty(),"tracked release");
 received.clear();require(send({a},'A',L"down")&&send({a},'A',L"up"),"explicit down/up");pump();require(received.size()==2&&received[0].key=='A'&&received[1].key=='A'&&received[1].message==WM_KEYUP,"explicit A receipt");
 received.clear();require(send({a,b},'S',L"press"),"S press");pump();require(received.size()==4&&received[0].key=='S',"S receipt");
 POINT cursor{};require(GetCursorPos(&cursor)!=FALSE,"cursor sample");
 require(!mouseSend({a},L"move"),"unpositioned refused");
 require(position({a},20,30)&&position({b},90,100),"independent positions");
 require(!position({a,b},640,0)&&mice[a.hwnd].x==20&&mice[b.hwnd].x==90,"bounds and atomic position");
 for(const auto& selection:std::vector<std::vector<Window>>{{a},{b},{a,b}}){
  received.clear();require(mouseSend(selection,L"move"),"move");pump();require(received.size()==selection.size(),"move isolation");
  for(auto e:received){auto m=mice[e.hwnd];require(e.message==WM_MOUSEMOVE&&e.key==0&&e.bits==static_cast<DWORD>(coordinates(m.x,m.y)),"move coordinates");}
  for(auto action:{L"lclick",L"rclick"}){
   received.clear();require(mouseSend(selection,action),"click");pump();require(received.size()==selection.size()*2,"click count");
   bool left=std::wstring(action)==L"lclick";
   for(auto w:selection){std::vector<Received> events;for(auto e:received)if(e.hwnd==w.hwnd)events.push_back(e);
    require(events.size()==2&&events[0].message==(left?WM_LBUTTONDOWN:WM_RBUTTONDOWN)&&events[1].message==(left?WM_LBUTTONUP:WM_RBUTTONUP),"click order/isolation");
    require(events[0].key==(left?MK_LBUTTON:MK_RBUTTON)&&events[1].key==0&&events[0].bits==static_cast<DWORD>(coordinates(mice[w.hwnd].x,mice[w.hwnd].y))&&events[0].bits==events[1].bits,"click flags/coordinates");}
  }
  for(int delta:{120,-120}){received.clear();require(mouseSend(selection,L"wheel",delta),"wheel");pump();require(received.size()==selection.size(),"wheel isolation");
   for(auto e:received){POINT p{mice[e.hwnd].x,mice[e.hwnd].y};require(ClientToScreen(e.hwnd,&p)!=FALSE,"screen conversion");require(e.message==WM_MOUSEWHEEL&&static_cast<short>(HIWORD(e.key))==delta&&LOWORD(e.key)==0&&e.bits==static_cast<DWORD>(coordinates(p.x,p.y)),"wheel signed delta/screen coords");}}
 }
 received.clear();require(mouseSend({a},L"ldown")&&mouseSend({b},L"rdown")&&mouseSend({a},L"rdown"),"independent buttons");pump();
 require(mice[a.hwnd].buttons==(MK_LBUTTON|MK_RBUTTON)&&mice[b.hwnd].buttons==MK_RBUTTON,"button isolation");
 require(!mouseSend({a},L"lclick"),"held click refused");
 received.clear();require(mouseSend({a,b},L"move"),"drag move");pump();require(received[0].key==(MK_LBUTTON|MK_RBUTTON)&&received[1].key==MK_RBUTTON,"drag masks");
 received.clear();require(release(),"mouse release all");pump();require(received.size()==3&&mice[a.hwnd].buttons==0&&mice[b.hwnd].buttons==0,"release buttons");
 require(!mousePost(a,WM_MOUSEWHEEL,32768),"wheel overflow refused");require(coordinates(-12,-34)==static_cast<LPARAM>(0xffdefff4u),"signed coordinates");
 POINT end{};require(GetCursorPos(&end)!=FALSE,"end cursor sample");
 log(end.x==cursor.x&&end.y==cursor.y?L"CURSOR_OBSERVATION unchanged over owned mouse test":L"CURSOR_OBSERVATION changed during test interval; external movement is not blocked; inspect per-post samples. Tool never requests cursor movement.");
 require(GetForegroundWindow()==original,"foreground changed during owned test");
 DestroyWindow(b.hwnd);require(!post(b,VK_ESCAPE,true)&&!mousePost(b,WM_MOUSEMOVE),"stale hwnd should fail");DestroyWindow(a.hwnd);
 log(L"PASS owned receiver A/B/BOTH keyboard + mouse move/left/right/wheel, independent coordinates/buttons, release, signed coordinates, stale HWND, foreground unchanged; cursor observations logged separately. EVE NOT TESTED.");return 0;
}
void help(){
 std::wcout<<L"Commands: list | select A|B|BOTH|<number> | key ESC|A|W|S | press | down | up | release | refresh | confirm yes|no|unknown | quit\n"
  L"Mouse: select hwnd:<decimal or 0xHEX> (discovered windows only) | pos X Y | move | lclick | ldown | lup | rclick | rdown | rup | wheel <signed delta>\n"
  L"pos only stores client-area pixel coordinates per HWND, sends nothing. move/click/wheel use each recipient's own position. Wheel +/-120 = one notch.\n"
  L"list shows dimensions and virtual state. release also releases tracked mouse buttons. No real cursor movement.\n"
  L"confirm reports YOUR observation of last action only; record immediately.\n"
  L"No target activation, WM_CHAR, SendInput, hooks, memory access or DLL injection.\n"
  L"Queued != processed. UIPI/access denied is reported, not bypassed. Release is best effort, not guaranteed after crash.\n";
}
bool number(const std::wstring& text,LONG& value){
 if(text.empty())return false;size_t start=(text[0]==L'-'||text[0]==L'+')?1:0;
 if(start==text.size()||text.find_first_not_of(L"0123456789",start)!=std::wstring::npos)return false;
 try{auto n=std::stoll(text);if(n< -2147483647LL-1||n>2147483647LL)return false;value=static_cast<LONG>(n);return true;}catch(...){return false;}
}
}
int wmain(int argc,wchar_t** argv){
 try{
  if(argc==2&&std::wstring(argv[1])==L"--self-test")return selfTest();
  if(argc==2&&std::wstring(argv[1])==L"--help"){help();return 0;}
  if(argc>2||(argc==2&&std::wstring(argv[1])!=L"--list")){help();return 2;}
  wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
  auto path=std::filesystem::path(exe).parent_path()/L"eve_key_post.log";
  logfile.open(path,std::ios::app|std::ios::binary);if(!logfile)throw std::runtime_error("Cannot open log next to executable");
  log(L"SESSION experimental PostMessage keyboard/mouse test; log="+path.wstring()+L" game_processing=UNKNOWN");discover();
  if(argc==2)return 0;
  help();std::vector<Window> selected;UINT key=VK_ESCAPE;
  std::wstring line;
  while(std::wcout<<L"> "&&std::getline(std::wcin,line)){
   std::wistringstream input(line);std::wstring command,arg,extra,tail;input>>command>>arg>>extra>>tail;
   if(command==L"pos"){
    LONG x=0,y=0;if(!tail.empty()||!number(arg,x)||!number(extra,y)){log(L"Usage: pos X Y (client-area integers)");continue;}
    if(selected.empty())log(L"Select a recipient first");else position(selected,x,y);continue;
   }
   if(!extra.empty()){log(L"Invalid command");continue;}
   if(command==L"move"||command==L"lclick"||command==L"ldown"||command==L"lup"||command==L"rclick"||command==L"rdown"||command==L"rup"||command==L"wheel"){
    LONG delta=0;if(command==L"wheel"?(!number(arg,delta)||!delta||!fits(delta)):!arg.empty()){log(L"Invalid mouse arguments; wheel delta must be nonzero signed 16-bit");continue;}
    if(selected.empty())log(L"Select a recipient first");else{lastAction=command;log(L"MOUSE_ACTION "+line);mouseSend(selected,command,delta);}continue;
   }
   if(command==L"quit"&&arg.empty())break;
   if(command==L"select"){
    selected.clear();
     if(arg.rfind(L"hwnd:",0)==0){
      auto text=arg.substr(5);bool hex=text.rfind(L"0x",0)==0||text.rfind(L"0X",0)==0;
      auto digits=hex?text.substr(2):text;
      if(!digits.empty()&&digits.find_first_not_of(hex?L"0123456789abcdefABCDEF":L"0123456789")==std::wstring::npos){try{auto n=std::stoull(text,nullptr,hex?16:10);for(auto w:recipients)if(n==reinterpret_cast<ULONG_PTR>(w.hwnd))selected={w};}catch(...){}}
     }else if(arg==L"BOTH"){if(recipients.size()>=2)selected={recipients[0],recipients[1]};}
    else{size_t index=recipients.size();if(arg==L"A")index=0;else if(arg==L"B")index=1;
     else if(!arg.empty()&&arg.find_first_not_of(L"0123456789")==std::wstring::npos){try{auto n=std::stoull(arg);if(n>0&&n<=recipients.size())index=static_cast<size_t>(n-1);}catch(...){}}
     if(index<recipients.size())selected={recipients[index]};
    }
    log(selected.empty()?L"No recipient selected":L"Selection changed");for(auto w:selected)log(L"SELECTED pid="+std::to_wstring(w.pid)+L" hwnd="+handle(w.hwnd));
   }else if(command==L"key"){
    if(arg==L"ESC")key=VK_ESCAPE;else if(arg==L"A"||arg==L"W"||arg==L"S")key=arg[0];else{log(L"Allowed keys ESC A W S");continue;}
    log(L"KEY vk="+std::to_wstring(key));
   }else if(command==L"confirm"&&(arg==L"yes"||arg==L"no"||arg==L"unknown")){
     log(L"MANUAL_OBSERVATION response="+arg+L" last_action="+lastAction+L" selected_vk="+std::to_wstring(key)+L" (operator report, not verified processing)");
     for(auto w:selected){const auto& m=mice[w.hwnd];log(L"MANUAL_SELECTED pid="+std::to_wstring(w.pid)+L" hwnd="+handle(w.hwnd)+L" client_x="+std::to_wstring(m.x)+L" client_y="+std::to_wstring(m.y));}
   }else if(!arg.empty()){log(L"Invalid command arguments");}
   else if(command==L"list")list();
   else if(command==L"refresh"){if(release()){selected.clear();discover();}else log(L"Refresh refused: release failed; tracked keys retained");}
    else if(command==L"release"){lastAction=L"release";release();}
   else if(command==L"press"||command==L"down"||command==L"up"){
     if(selected.empty())log(L"Select a recipient first");else{lastAction=L"key "+std::to_wstring(key)+L" "+command;send(selected,key,command);}
   }else help();
  }
  return release()?0:1;
 }catch(const std::exception& e){release();std::cerr<<e.what()<<'\n';return 1;}
}