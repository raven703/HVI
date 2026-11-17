#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "runtime_api.hpp"
#include "gui_common.hpp"
#include "gui_test_protocol.hpp"
#include <deque>
#include <sstream>
namespace {
UINT session=1;HWND window;HFONT font;IDirectInputDevice8W *keyboard=nullptr,*mouse=nullptr;
GuiObservation observed;std::deque<std::wstring> logLines;bool compare=true;int scroll=0,contentHeight=800;
DWORD rawType=0,rawVk=0,rawFlags=0;LONG rawX=0,rawY=0;BYTE previous[256]{};
LONG lastDx=0,lastDy=0;
void log(const std::wstring& s){logLines.push_back(hvi::stamp()+L" "+s);if(logLines.size()>20)logLines.pop_front();}
void poll(){
 observed.session=session;observed.pid=GetCurrentProcessId();observed.foreground=GetForegroundWindow()==window;++observed.polls;
 HviGetKeyboardState(observed.win32);GetKeyboardState(observed.native);
 observed.keyboardResult=keyboard->GetDeviceState(256,observed.directInput);observed.mouseResult=mouse->GetDeviceState(sizeof(observed.mouse),&observed.mouse);HviGetSnapshot(&observed.runtime);
 if(observed.mouse.lX||observed.mouse.lY){lastDx=observed.mouse.lX;lastDy=observed.mouse.lY;}
 for(int k=0;k<256;++k)if((previous[k]&128)!=(observed.win32[k]&128)){log(L"HVI Win32 VK="+std::to_wstring(k)+(observed.win32[k]&128?L" DOWN":L" UP"));previous[k]=observed.win32[k];}
 for(int source=0;source<2;++source){DIDEVICEOBJECTDATA events[256]{};DWORD n=256;auto* device=source?mouse:keyboard;HRESULT hr=device->GetDeviceData(sizeof(events[0]),events,&n,0);if(hr==DI_BUFFEROVERFLOW)log(L"HVI DirectInput BUFFER OVERFLOW");if(SUCCEEDED(hr)){if(source)observed.mouseBuffered+=n;else observed.keyboardBuffered+=n;for(DWORD i=0;i<n;++i)log(std::wstring(source?L"HVI DI Mouse":L"HVI DI Keyboard")+L" offset="+std::to_wstring(events[i].dwOfs)+L" value="+std::to_wstring(static_cast<LONG>(events[i].dwData)));}}
 InvalidateRect(window,nullptr,FALSE);
}
void paint(HDC dc){
 RECT r;GetClientRect(window,&r);FillRect(dc,&r,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);int y=10-scroll;int line=20;
 auto draw=[&](const std::wstring&s){hvi::drawLine(dc,12,y,s,line);};
 draw(L"HVI TEST "+std::wstring(session==1?L"A":L"B")+L" | PID "+std::to_wstring(GetCurrentProcessId())+L" | Session "+std::to_wstring(session)+L" | "+(observed.foreground?L"FOREGROUND":L"BACKGROUND"));
 draw(std::wstring(L"Runtime: ")+(observed.runtime.connected?L"CONNECTED":L"WAITING / DISCONNECTED")+L" | controller PID "+std::to_wstring(observed.runtime.clientPid));
 draw(L"Commands="+std::to_wstring(observed.runtime.accepted)+L" rejected="+std::to_wstring(observed.runtime.rejected)+L" timeout="+std::to_wstring(observed.runtime.timeouts)+L" error="+std::to_wstring(observed.runtime.lastError));
 draw(L"HVI virtual Win32: Shift="+std::to_wstring(bool(observed.win32[VK_SHIFT]&128))+L" Ctrl="+std::to_wstring(bool(observed.win32[VK_CONTROL]&128))+L" Alt="+std::to_wstring(bool(observed.win32[VK_MENU]&128)));
 std::wstring pressed=L"HVI VK held: ";for(int k=0;k<256;++k)if(observed.win32[k]&128)pressed+=std::to_wstring(k)+L" ";draw(pressed);
 const wchar_t* labels[]{L"W",L"A",L"S",L"D",L"Shift",L"Ctrl",L"Alt",L"Space",L"Enter",L"Esc"};int keys[]{'W','A','S','D',VK_SHIFT,VK_CONTROL,VK_MENU,VK_SPACE,VK_RETURN,VK_ESCAPE};
 int cell=std::max(24,std::min<int>(65,(r.right-24)/10));for(int i=0;i<10;++i){RECT key{12+i*cell,y,10+(i+1)*cell,y+30};HBRUSH b=CreateSolidBrush(observed.win32[keys[i]]&128?RGB(80,210,120):RGB(230,234,240));FillRect(dc,&key,b);DeleteObject(b);DrawTextW(dc,labels[i],-1,&key,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}y+=38;
 std::wstring di=L"HVI DI Keyboard held (DIK): ";for(int k=0;k<256;++k)if(observed.directInput[k]&128)di+=std::to_wstring(k)+L" ";draw(di);
 draw(L"HVI Mouse virtual X/Y="+std::to_wstring(observed.runtime.x)+L" / "+std::to_wstring(observed.runtime.y));
 draw(L"HVI DI Mouse delta="+std::to_wstring(observed.mouse.lX)+L", "+std::to_wstring(observed.mouse.lY)+L" wheel delta="+std::to_wstring(observed.mouse.lZ)+L" total="+std::to_wstring(observed.runtime.wheel));
 draw(L"Last nonzero DI Mouse movement="+std::to_wstring(lastDx)+L", "+std::to_wstring(lastDy));
 std::wstring buttons=L"HVI DI Mouse buttons: ";for(int i=0;i<5;++i)buttons+=std::to_wstring(i)+L"="+std::to_wstring(bool(observed.mouse.rgbButtons[i]&128))+L" ";draw(buttons);
 draw(L"API HRESULT keyboard="+std::to_wstring(observed.keyboardResult)+L" mouse="+std::to_wstring(observed.mouseResult)+L" | buffered K/M="+std::to_wstring(observed.keyboardBuffered)+L"/"+std::to_wstring(observed.mouseBuffered));
 if(compare){std::wstring native=L"NATIVE Windows GetKeyboardState VK: ";for(int k=0;k<256;++k)if(observed.native[k]&128)native+=std::to_wstring(k)+L" ";draw(native);
 draw(L"NATIVE GetAsyncKeyState W/S="+std::to_wstring(bool(GetAsyncKeyState('W')&0x8000))+L"/"+std::to_wstring(bool(GetAsyncKeyState('S')&0x8000)));
 draw(L"NATIVE WM_INPUT count="+std::to_wstring(observed.rawCount)+L" type="+std::to_wstring(rawType)+L" VK="+std::to_wstring(rawVk)+L" flags="+std::to_wstring(rawFlags)+L" dx/dy="+std::to_wstring(rawX)+L"/"+std::to_wstring(rawY));}
 draw(L"Synthetic HVI events are NOT Raw Input. F2: native comparison. Wheel: scroll.");draw(L"LAST 20 API / NATIVE EVENTS:");for(auto&s:logLines)draw(s);contentHeight=y+scroll+10;
 SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,contentHeight,static_cast<UINT>(r.bottom),scroll,0};SetScrollInfo(window,SB_VERT,&si,TRUE);
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:window=hwnd;font=hvi::guiFont();SetTimer(hwnd,1,33,nullptr);return 0;
 case WM_TIMER:poll();return 0;
 case WM_KEYDOWN:if(wp==VK_F2){compare=!compare;InvalidateRect(hwnd,nullptr,FALSE);}return 0;
 case WM_MOUSEWHEEL:scroll=std::max(0,scroll-GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*60);InvalidateRect(hwnd,nullptr,FALSE);return 0;
 case WM_VSCROLL:{int code=LOWORD(wp);if(code==SB_LINEUP)scroll-=20;else if(code==SB_LINEDOWN)scroll+=20;else if(code==SB_PAGEUP)scroll-=200;else if(code==SB_PAGEDOWN)scroll+=200;else if(code==SB_THUMBTRACK){SCROLLINFO si{sizeof(si),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&si);scroll=si.nTrackPos;}scroll=std::clamp(scroll,0,std::max(0,contentHeight-100));InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 case WM_INPUT:{UINT n=0;if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,nullptr,&n,sizeof(RAWINPUTHEADER))==0&&n>=sizeof(RAWINPUTHEADER)&&n<=65536){std::vector<BYTE> data(n);if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,data.data(),&n,sizeof(RAWINPUTHEADER))==n){auto*raw=reinterpret_cast<RAWINPUT*>(data.data());++observed.rawCount;rawType=raw->header.dwType;if(rawType==RIM_TYPEKEYBOARD&&n>=sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD)){rawVk=raw->data.keyboard.VKey;rawFlags=raw->data.keyboard.Flags;log(L"NATIVE Raw keyboard VK="+std::to_wstring(rawVk));}if(rawType==RIM_TYPEMOUSE&&n>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE)){rawX=raw->data.mouse.lLastX;rawY=raw->data.mouse.lLastY;}}}return DefWindowProcW(hwnd,msg,wp,lp);}
 case WM_COPYDATA:{auto* c=reinterpret_cast<COPYDATASTRUCT*>(lp);if(c&&c->dwData==HVI_GUI_QUERY&&c->cbData==0&&IsWindow(reinterpret_cast<HWND>(wp))){poll();COPYDATASTRUCT reply{HVI_GUI_QUERY,sizeof(observed),&observed};DWORD_PTR result=0;return SendMessageTimeoutW(reinterpret_cast<HWND>(wp),WM_COPYDATA,reinterpret_cast<WPARAM>(hwnd),reinterpret_cast<LPARAM>(&reply),SMTO_ABORTIFHUNG,500,&result)!=0;}return FALSE;}
 case WM_SIZE:InvalidateRect(hwnd,nullptr,TRUE);return 0;
 case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
 case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);paint(dc);EndPaint(hwnd,&ps);return 0;}
 case WM_CLOSE:DestroyWindow(hwnd);return 0;
 case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
 }return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR args,int){
 hvi::dpiAware();session=args&&*args?static_cast<UINT>(strtoul(args,nullptr,10)):1;if(session!=1&&session!=2)return 1;
 if(!HviInitialize(session)){MessageBoxW(nullptr,L"Cannot initialize Runtime session. Another TEST may already own it.",L"HVI",MB_ICONERROR);return 2;}
 if(FAILED(HviCreateKeyboard(TRUE,reinterpret_cast<void**>(&keyboard)))||FAILED(HviCreateMouse(TRUE,reinterpret_cast<void**>(&mouse)))){if(keyboard)keyboard->Release();HviShutdown();return 3;}
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"HviTestClient";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
 RECT r=hvi::demoRect(session-1);std::wstring title=session==1?L"HVI Test A":L"HVI Test B";
 HWND hwnd=CreateWindowExW(0,wc.lpszClassName,title.c_str(),WS_OVERLAPPEDWINDOW|WS_VSCROLL,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
 int result=4;
 if(hwnd){
  DIPROPDWORD prop{};prop.diph={sizeof(prop),sizeof(DIPROPHEADER),0,DIPH_DEVICE};prop.dwData=256;
  bool ok=SUCCEEDED(keyboard->SetDataFormat(&c_dfDIKeyboard))&&SUCCEEDED(mouse->SetDataFormat(&c_dfDIMouse2))&&SUCCEEDED(keyboard->SetCooperativeLevel(hwnd,DISCL_BACKGROUND|DISCL_NONEXCLUSIVE))&&SUCCEEDED(mouse->SetCooperativeLevel(hwnd,DISCL_BACKGROUND|DISCL_NONEXCLUSIVE))&&SUCCEEDED(keyboard->SetProperty(DIPROP_BUFFERSIZE,&prop.diph))&&SUCCEEDED(mouse->SetProperty(DIPROP_BUFFERSIZE,&prop.diph))&&SUCCEEDED(keyboard->Acquire())&&SUCCEEDED(mouse->Acquire());
  if(ok){RAWINPUTDEVICE raw[]{{1,6,RIDEV_INPUTSINK,hwnd},{1,2,RIDEV_INPUTSINK,hwnd}};if(!RegisterRawInputDevices(raw,2,sizeof(raw[0])))log(L"Native Raw Input registration unavailable");ShowWindow(hwnd,SW_SHOWNOACTIVATE);result=hvi::messageLoop();}else DestroyWindow(hwnd);
 }
 keyboard->Unacquire();mouse->Unacquire();keyboard->Release();mouse->Release();HviShutdown();if(font)DeleteObject(font);return result;
}