#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
namespace hvi {
inline void dpiAware(){using Fn=BOOL(WINAPI*)(HANDLE);auto f=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));if(f)f(reinterpret_cast<HANDLE>(-4));else SetProcessDPIAware();}
inline std::wstring executableDir(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);std::wstring s=path;return s.substr(0,s.find_last_of(L"\\/"));}
inline RECT demoRect(int slot){RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int w=work.right-work.left,h=work.bottom-work.top,gap=8;int half=(w-3*gap)/2,top=(h-3*gap)/2;
 if(slot<2)return {work.left+gap+slot*(half+gap),work.top+gap,work.left+gap+slot*(half+gap)+half,work.top+gap+top};
 int width=std::min(w-2*gap,1100);return {work.left+(w-width)/2,work.top+2*gap+top,work.left+(w+width)/2,work.bottom-gap};}
inline HFONT guiFont(int height=16){return CreateFontW(-height,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FF_DONTCARE,L"Segoe UI");}
inline std::wstring stamp(){SYSTEMTIME s;GetLocalTime(&s);wchar_t b[32];swprintf(b,32,L"%02u:%02u:%02u.%03u",s.wHour,s.wMinute,s.wSecond,s.wMilliseconds);return b;}
inline int messageLoop(){MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);}
inline void drawLine(HDC dc,int x,int& y,const std::wstring& s,int height=20){TextOutW(dc,x,y,s.c_str(),static_cast<int>(s.size()));y+=height;}
inline bool spawn(const std::wstring& exe,const std::wstring& args,PROCESS_INFORMATION& p){std::wstring command=L"\""+executableDir()+L"\\"+exe+L"\" "+args;STARTUPINFOW s{};s.cb=sizeof(s);return CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,0,nullptr,executableDir().c_str(),&s,&p)!=FALSE;}
}