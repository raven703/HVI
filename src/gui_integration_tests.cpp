#define DIRECTINPUT_VERSION 0x0800
#include "gui_common.hpp"
#include "gui_test_protocol.hpp"
#include <iostream>
namespace {
GuiObservation reply;bool received=false;
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_COPYDATA){auto*c=reinterpret_cast<COPYDATASTRUCT*>(l);if(c&&c->dwData==HVI_GUI_QUERY&&c->cbData==sizeof(reply)){memcpy(&reply,c->lpData,sizeof(reply));received=true;return TRUE;}}return DefWindowProcW(h,m,w,l);}
GuiObservation query(HWND receiver,HWND client){received=false;COPYDATASTRUCT c{HVI_GUI_QUERY,0,nullptr};DWORD_PTR result=0;SendMessageTimeoutW(client,WM_COPYDATA,reinterpret_cast<WPARAM>(receiver),reinterpret_cast<LPARAM>(&c),SMTO_ABORTIFHUNG,1000,&result);if(!received)throw std::runtime_error("GUI API observation unavailable");return reply;}
void click(HWND control,int id){SendMessageW(control,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(control,id)));Sleep(180);}
void target(HWND c,int index){auto box=GetDlgItem(c,100);SendMessageW(box,CB_SETCURSEL,index,0);SendMessageW(c,WM_COMMAND,MAKEWPARAM(100,CBN_SELCHANGE),reinterpret_cast<LPARAM>(box));}
void key(HWND c,int index,bool down){SendMessageW(GetDlgItem(c,110),CB_SETCURSEL,index,0);click(c,down?103:104);}
}
int main(){
 hvi::dpiAware();if(FindWindowW(L"HviTestClient",nullptr)||FindWindowW(L"HviController",nullptr)){std::cerr<<"Close existing HVI demo before GUI test\n";return 1;}
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"HviGuiTestReceiver";RegisterClassW(&wc);HWND receiver=CreateWindowW(wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
 PROCESS_INFORMATION a{},b{},c{};int errors=0;auto check=[&](bool ok,const char*s){std::cout<<(ok?"PASS ":"FAIL ")<<s<<"\n";if(!ok)++errors;};
 try{
  if(!hvi::spawn(L"hvi_test_client.exe",L"1",a)||!hvi::spawn(L"hvi_test_client.exe",L"2",b)||!hvi::spawn(L"hvi_gui_controller.exe",L"",c))throw std::runtime_error("launch failed");
  HWND aw=nullptr,bw=nullptr,cw=nullptr;for(int i=0;i<100;++i){aw=FindWindowW(L"HviTestClient",L"HVI Test A");bw=FindWindowW(L"HviTestClient",L"HVI Test B");cw=FindWindowW(L"HviController",nullptr);if(aw&&bw&&cw)break;Sleep(50);}if(!aw||!bw||!cw)throw std::runtime_error("three windows not found");Sleep(400);
  auto qa=[&]{return query(receiver,aw);};auto qb=[&]{return query(receiver,bw);};
  SetForegroundWindow(cw);target(cw,0);key(cw,0,true);auto x=qa(),y=qb();check(x.win32['W']&&x.directInput[DIK_W]&&!y.win32['W']&&!y.directInput[DIK_W],"A W only via Win32 and DI API");
  target(cw,1);key(cw,2,true);x=qa();y=qb();check(y.win32['S']&&y.directInput[DIK_S]&&!x.win32['S']&&x.win32['W'],"B S while A holds W");key(cw,3,true);y=qb();check(y.directInput[DIK_D]&&qa().directInput[DIK_W],"independent simultaneous hold");
  target(cw,0);HWND pad=FindWindowExW(cw,nullptr,L"HviMousePad",nullptr);POINT origin{30,35};ClientToScreen(pad,&origin);POINT saved;GetCursorPos(&saved);SetCursorPos(origin.x,origin.y);Sleep(100);click(cw,106);SetCursorPos(origin.x+20,origin.y+30);Sleep(250);x=qa();y=qb();check(x.runtime.x==20&&x.runtime.y==30&&y.runtime.x==0,"A mouse isolation");
  target(cw,1);SetCursorPos(origin.x+15,origin.y+25);click(cw,301);y=qb();check(y.runtime.x==-5&&y.runtime.y==-5&&y.mouse.rgbButtons[1]&&!qa().mouse.rgbButtons[1],"B mouse/button isolation");SetCursorPos(saved.x,saved.y);
  target(cw,2);key(cw,7,true);check(qa().directInput[DIK_SPACE]&&qb().directInput[DIK_SPACE],"BOTH independent ACK sequences");
  target(cw,3);key(cw,8,true);check(!qa().directInput[DIK_RETURN]&&!qb().directInput[DIK_RETURN],"NONE no delivery");
  Sleep(700);check(qa().win32['W']&&qb().win32['S'],"heartbeat preserves background hold");click(cw,102);x=qa();y=qb();bool clear=true;for(auto k:x.directInput)clear&=k==0;for(auto k:y.directInput)clear&=k==0;for(auto k:y.mouse.rgbButtons)clear&=k==0;check(clear,"Release All API state clear");
  target(cw,0);HWND edit=GetDlgItem(cw,105);SendMessageW(edit,WM_KEYDOWN,'A',0);Sleep(200);check(qa().directInput[DIK_A]&&!qb().directInput[DIK_A],"physical entry field routes only A");SendMessageW(edit,WM_KEYUP,'A',0);Sleep(150);
  key(cw,0,true);click(cw,109);Sleep(250);check(!qa().directInput[DIK_W],"disconnect reset");key(cw,3,true);check(qa().directInput[DIK_D]&&!qb().directInput[DIK_D],"reconnect independent APIs");
  check(qa().keyboardBuffered>0&&qa().mouseBuffered>0&&qa().keyboardResult==DI_OK&&qa().mouseResult==DI_OK,"GUI buffered and immediate API polling");
  PostMessageW(aw,WM_CLOSE,0,0);check(WaitForSingleObject(a.hProcess,3000)==WAIT_OBJECT_0,"A exits without hung process");target(cw,1);key(cw,1,true);check(qb().directInput[DIK_A],"B survives A close");
 }catch(const std::exception&e){++errors;std::cerr<<e.what()<<"\n";}
 for(auto*p:{&a,&b,&c})if(p->hProcess){DWORD pid=p->dwProcessId;EnumWindows([](HWND h,LPARAM l)->BOOL{DWORD id=0;GetWindowThreadProcessId(h,&id);if(id==static_cast<DWORD>(l))PostMessageW(h,WM_CLOSE,0,0);return TRUE;},pid);if(WaitForSingleObject(p->hProcess,4000)!=WAIT_OBJECT_0){++errors;TerminateProcess(p->hProcess,99);}CloseHandle(p->hThread);CloseHandle(p->hProcess);}
 DestroyWindow(receiver);std::cout<<"GUI integration errors="<<errors<<"\n";return errors?1:0;
}