#include "gui_common.hpp"
#include "runtime_transport.hpp"
#include <windowsx.h>
#include <thread>
#include <condition_variable>
#include <deque>
#include <sstream>
namespace {
enum {Target=100,Hold=101,Release=102,Down=103,Up=104,Physical=105,ResetMouse=106,WheelUp=107,WheelDown=108,Disconnect=109,KeyBase=200,ButtonBase=300};
HWND window,targetBox,holdBox,keyBox,physical,diagnostic,mouseZone;HFONT font;
std::thread io;std::mutex mutex;std::condition_variable cv;bool stopping=false;unsigned target=1;
struct Command{unsigned targets;hvi::Kind kind;int a,b;bool disconnect=false;};std::deque<Command> commands;
std::wstring status=L"Connecting...";std::deque<std::wstring> logs;uint64_t queued=0,dropped=0;bool held[256]{};unsigned heldTarget[256]{};POINT lastPoint{};bool tracking=false;
struct Geometry{HWND hwnd;int x,y,w,h;};std::vector<Geometry> controls;POINT lastDelta{};
int keyValues[]{'W','A','S','D',VK_LSHIFT,VK_LCONTROL,VK_LMENU,VK_SPACE,VK_RETURN,VK_ESCAPE,VK_RSHIFT,VK_RCONTROL,VK_RMENU};
void enqueue(hvi::Kind kind,int a=0,int b=0,unsigned destinations=99){std::lock_guard<std::mutex> lock(mutex);if(commands.size()>=4096){++dropped;commands.clear();commands.push_back({3,hvi::Kind::Reset,0,0});}commands.push_back({destinations==99?target:destinations,kind,a,b});++queued;cv.notify_one();}
void worker(){
 hvi::RuntimeTransport a(1),b(2);auto heartbeat=GetTickCount64();
 for(;;){std::deque<Command> batch;{std::unique_lock<std::mutex> l(mutex);cv.wait_for(l,std::chrono::milliseconds(75),[]{return stopping||!commands.empty();});if(stopping)break;batch.swap(commands);}
  for(auto&c:batch){if(c.a==-1000){for(int n=0;n<1000;++n){for(auto*t:{&a,&b}){t->send(hvi::Kind::Key,'W',1);t->send(hvi::Kind::Key,'W',0);t->send(hvi::Kind::Button,0,1);t->send(hvi::Kind::Button,0,0);t->send(hvi::Kind::Relative,1,-1);}if(n%100==0){a.send(hvi::Kind::Wheel,120,120);b.send(hvi::Kind::Wheel,-120,-120);}}a.send(hvi::Kind::Key,'W',1);a.disconnect();b.send(hvi::Kind::Snapshot);Sleep(30);a.send(hvi::Kind::Snapshot);continue;}for(int i=0;i<2;++i)if(c.targets&(1u<<i)){auto& t=i?b:a;if(c.disconnect)t.disconnect();else t.send(c.kind,c.a,c.b);}}
  if(GetTickCount64()-heartbeat>=150){a.send(hvi::Kind::Snapshot);b.send(hvi::Kind::Snapshot);heartbeat=GetTickCount64();}
  std::wostringstream s;s<<L"TEST A: "<<(a.connected()?L"CONNECTED":L"OFFLINE")<<L" sent="<<a.sent<<L" ACK="<<a.acks<<L" errors="<<a.errors<<L" last="<<a.lastError<<L" latency="<<a.latencyMs<<L"ms\r\nTEST B: "<<(b.connected()?L"CONNECTED":L"OFFLINE")<<L" sent="<<b.sent<<L" ACK="<<b.acks<<L" errors="<<b.errors<<L" last="<<b.lastError<<L" latency="<<b.latencyMs<<L"ms";
  {std::lock_guard<std::mutex> l(mutex);status=s.str();for(auto&c:batch){logs.push_back(hvi::stamp()+L" target="+std::to_wstring(c.targets)+L" kind="+std::to_wstring(static_cast<DWORD>(c.kind))+L" a="+std::to_wstring(c.a)+L" b="+std::to_wstring(c.b));if(logs.size()>20)logs.pop_front();}}
 }
 a.send(hvi::Kind::Reset);b.send(hvi::Kind::Reset);
}
void physicalKey(int key,bool down){if(key<0||key>255)return;if(down){if(!held[key]){held[key]=true;heldTarget[key]=target;enqueue(hvi::Kind::Key,key,1,target);}}else if(held[key]){enqueue(hvi::Kind::Key,key,0,heldTarget[key]);held[key]=false;}}
WNDPROC oldEdit=nullptr;
LRESULT CALLBACK inputProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN||m==WM_KEYUP||m==WM_SYSKEYUP){int key=static_cast<int>(w);if(key==VK_SHIFT)key=MapVirtualKeyW((l>>16)&255,MAPVK_VSC_TO_VK_EX);if(key==VK_CONTROL)key=(l&(1<<24))?VK_RCONTROL:VK_LCONTROL;if(key==VK_MENU)key=(l&(1<<24))?VK_RMENU:VK_LMENU;physicalKey(key,m==WM_KEYDOWN||m==WM_SYSKEYDOWN);return 0;}
 if(m==WM_KILLFOCUS){for(int k=0;k<256;++k)physicalKey(k,false);}
 if(m==WM_CHAR||m==WM_SYSCHAR)return 0;
 return CallWindowProcW(oldEdit,h,m,w,l);
}
LRESULT CALLBACK zoneProc(HWND h,UINT m,WPARAM w,LPARAM l){
 POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
 if(m==WM_MOUSEMOVE){if(tracking){int dx=p.x-lastPoint.x,dy=p.y-lastPoint.y;lastDelta={dx,dy};if(dx||dy)enqueue(hvi::Kind::Relative,dx,dy);}lastPoint=p;tracking=true;TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
 if(m==WM_MOUSELEAVE){tracking=false;return 0;}
 if(m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN||m==WM_MBUTTONDOWN){SetCapture(h);enqueue(hvi::Kind::Button,m==WM_LBUTTONDOWN?0:m==WM_RBUTTONDOWN?1:2,1);return 0;}
 if(m==WM_LBUTTONUP||m==WM_RBUTTONUP||m==WM_MBUTTONUP){enqueue(hvi::Kind::Button,m==WM_LBUTTONUP?0:m==WM_RBUTTONUP?1:2,0);ReleaseCapture();return 0;}
 if(m==WM_MOUSEWHEEL){enqueue(hvi::Kind::Wheel,GET_WHEEL_DELTA_WPARAM(w));return 0;}
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1));SelectObject(dc,font);DrawTextW(dc,L"MOUSE PAD: move / click / wheel\nVirtual relative movement only\nPhysical Windows input is NOT blocked",-1,&r,DT_CENTER|DT_WORDBREAK);EndPaint(h,&ps);return 0;}
 return DefWindowProcW(h,m,w,l);
}
HWND add(const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);controls.push_back({c,x,y,w,h});return c;}
void layout(){if(controls.empty())return;RECT r;GetClientRect(window,&r);double sx=std::max(0.1,r.right/900.0),sy=std::max(0.1,r.bottom/360.0);HFONT next=hvi::guiFont(std::max(9,static_cast<int>(15*std::min(sx,sy))));for(auto& g:controls){int height=g.h;if(g.hwnd==mouseZone)height=130;if(g.hwnd==diagnostic)height=340;MoveWindow(g.hwnd,static_cast<int>(g.x*sx),static_cast<int>(g.y*sy),std::max(10,static_cast<int>(g.w*sx)),std::max(10,static_cast<int>(height*sy)),TRUE);SendMessageW(g.hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);}HFONT old=font;font=next;if(old)DeleteObject(old);}
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_CREATE:{window=h;font=hvi::guiFont(15);targetBox=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST,Target,10,10,150,150);for(auto s:{L"TEST A",L"TEST B",L"BOTH",L"NONE"})SendMessageW(targetBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(targetBox,CB_SETCURSEL,0,0);
 holdBox=add(L"BUTTON",L"Hold / toggle",BS_AUTOCHECKBOX,Hold,170,10,140,25);add(L"BUTTON",L"Release All",0,Release,320,10,110,25);
 const wchar_t* names[]{L"W",L"A",L"S",L"D",L"LShift",L"LCtrl",L"LAlt",L"Space",L"Enter",L"Esc",L"RShift",L"RCtrl",L"RAlt"};for(int i=0;i<13;++i)add(L"BUTTON",names[i],0,KeyBase+i,10+(i%7)*62,45+(i/7)*32,58,28);
 keyBox=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST,110,10,112,140,180);for(auto s:names)SendMessageW(keyBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(keyBox,CB_SETCURSEL,0,0);add(L"BUTTON",L"KeyDown",0,Down,160,112,85,28);add(L"BUTTON",L"KeyUp",0,Up,250,112,75,28);add(L"BUTTON",L"Disconnect",0,Disconnect,330,112,100,28);
 physical=add(L"EDIT",L"Click here: physical keyboard",WS_BORDER|ES_READONLY,Physical,10,148,420,27);oldEdit=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(physical,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(inputProc)));
 add(L"BUTTON",L"LMB",0,ButtonBase,10,183,60,28);add(L"BUTTON",L"RMB",0,ButtonBase+1,75,183,60,28);add(L"BUTTON",L"MMB",0,ButtonBase+2,140,183,60,28);add(L"BUTTON",L"Wheel +",0,WheelUp,205,183,70,28);add(L"BUTTON",L"Wheel -",0,WheelDown,280,183,70,28);add(L"BUTTON",L"Reset Mouse",0,ResetMouse,355,183,95,28);
 mouseZone=add(L"HviMousePad",L"",WS_BORDER,0,10,220,430,100);diagnostic=add(L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL,0,470,10,400,300);SetTimer(h,1,100,nullptr);io=std::thread(worker);layout();return 0;}
 case WM_KEYDOWN:if(w==VK_F6){enqueue(hvi::Kind::Snapshot,-1000,0,3);return 0;}break;
 case WM_SIZE:layout();return 0;
 case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout();return 0;}
 case WM_COMMAND:{int id=LOWORD(w);if(id==500){enqueue(hvi::Kind::Snapshot,-1000,0,3);return 0;}if(id==Target&&HIWORD(w)==CBN_SELCHANGE){int choice=static_cast<int>(SendMessageW(targetBox,CB_GETCURSEL,0,0));target=choice==0?1:choice==1?2:choice==2?3:0;}
 else if(id>=KeyBase&&id<KeyBase+13){int key=keyValues[id-KeyBase];if(SendMessageW(holdBox,BM_GETCHECK,0,0)==BST_CHECKED){physicalKey(key,!held[key]);}else {enqueue(hvi::Kind::Key,key,1);enqueue(hvi::Kind::Key,key,0);}}
 else if(id==Down||id==Up){int i=static_cast<int>(SendMessageW(keyBox,CB_GETCURSEL,0,0));if(i>=0&&i<13)physicalKey(keyValues[i],id==Down);}
 else if(id==Release){enqueue(hvi::Kind::Reset,0,0,3);std::fill(std::begin(held),std::end(held),false);}
 else if(id==ResetMouse)enqueue(hvi::Kind::ResetMouse);
 else if(id==WheelUp||id==WheelDown)enqueue(hvi::Kind::Wheel,id==WheelUp?120:-120);
 else if(id>=ButtonBase&&id<ButtonBase+3){int button=id-ButtonBase;static bool down[3]{};down[button]=!down[button];enqueue(hvi::Kind::Button,button,down[button]);}
 else if(id==Disconnect){std::lock_guard<std::mutex> lock(mutex);commands.push_back({target,hvi::Kind::Snapshot,0,0,true});cv.notify_one();}return 0;}
 case WM_TIMER:{std::wstring s;{std::lock_guard<std::mutex> lock(mutex);s=status+L"\r\nCommands queued="+std::to_wstring(queued)+L" queue resets="+std::to_wstring(dropped)+L"\r\nForeground HWND="+std::to_wstring(reinterpret_cast<UINT_PTR>(GetForegroundWindow()))+L"\r\nMouse pad X/Y="+std::to_wstring(lastPoint.x)+L"/"+std::to_wstring(lastPoint.y)+L" delta="+std::to_wstring(lastDelta.x)+L"/"+std::to_wstring(lastDelta.y)+L"\r\nRelease All resets BOTH for safety.\r\n";for(auto& line:logs)s+=line+L"\r\n";}SetWindowTextW(diagnostic,s.c_str());return 0;}
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);{std::lock_guard<std::mutex> lock(mutex);stopping=true;}cv.notify_one();if(io.joinable())io.join();DeleteObject(font);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,m,w,l);}
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int){hvi::dpiAware();WNDCLASSW pad{};pad.lpfnWndProc=zoneProc;pad.hInstance=instance;pad.lpszClassName=L"HviMousePad";pad.hCursor=LoadCursor(nullptr,IDC_CROSS);RegisterClassW(&pad);WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"HviController";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);RECT r=hvi::demoRect(2);HWND h=CreateWindowExW(0,wc.lpszClassName,L"HVI Control Panel",WS_OVERLAPPEDWINDOW,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!h)return 1;ShowWindow(h,SW_SHOW);return hvi::messageLoop();}