#include "gui_common.hpp"
#include <string>
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
 hvi::dpiAware();SetEnvironmentVariableW(L"SDL_DYNAMIC_API",(hvi::executableDir()+L"\\hvi_compat.dll").c_str());
 PROCESS_INFORMATION a{},b{},c{};bool ok=true;
 for(int id=1;id<=2;++id){SetEnvironmentVariableW(L"HVI_COMPAT_SESSION",std::to_wstring(id).c_str());SetEnvironmentVariableW(L"HVI_COMPAT_TRACE",(hvi::executableDir()+L"\\sdl_compat_"+std::to_wstring(id)+L".json").c_str());ok=hvi::spawn(L"sdl_input_test.exe",L"--title \"SDL Test "+std::wstring(id==1?L"A":L"B")+L"\" --observe \""+hvi::executableDir()+L"\\sdl_result_"+std::to_wstring(id)+L".json\"",id==1?a:b)&&ok;}
 SetEnvironmentVariableW(L"SDL_DYNAMIC_API",nullptr);SetEnvironmentVariableW(L"HVI_COMPAT_SESSION",nullptr);SetEnvironmentVariableW(L"HVI_COMPAT_TRACE",nullptr);
 ok=hvi::spawn(L"hvi_gui_controller.exe",L"",c)&&ok;
 for(int attempt=0;attempt<60;++attempt){int found=0;for(auto*p:{&a,&b})if(p->hProcess){struct Context{DWORD pid;HWND hwnd;};Context ctx{p->dwProcessId,nullptr};EnumWindows([](HWND h,LPARAM l)->BOOL{auto*c=reinterpret_cast<Context*>(l);DWORD id;GetWindowThreadProcessId(h,&id);if(id==c->pid&&IsWindowVisible(h)){c->hwnd=h;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&ctx));if(ctx.hwnd){RECT r=hvi::demoRect(p==&a?0:1);SetWindowPos(ctx.hwnd,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);++found;}}if(found==2)break;Sleep(50);}
 for(auto*p:{&a,&b,&c})if(p->hProcess){CloseHandle(p->hThread);CloseHandle(p->hProcess);}if(!ok)MessageBoxW(nullptr,L"SDL demo launch failed",L"HVI",MB_ICONERROR);return ok?0:1;
}