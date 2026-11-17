#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <set>
#include <stdexcept>

// Read-only Toolhelp snapshots and foreground sampling. No process handle,
// debugger, remote thread, input hook, privilege escalation or memory access.
int main(int argc,char** argv){
 static_assert(sizeof(void*)==8,"Inspector requires Windows x64");
 try{
  if(argc<2||argc>3){std::cerr<<"Usage: input_backend_inspector <pid|self> [duration-ms 100..600000]\n";return 2;}
  auto number=[](const std::string& s){size_t n=0;auto v=std::stoul(s,&n);if(n!=s.size()||v>0xffffffffUL)throw std::invalid_argument("number");return v;};
  DWORD pid=std::string(argv[1])=="self"?GetCurrentProcessId():number(argv[1]);
  DWORD duration=argc==3?number(argv[2]):5000;
  if(!pid||duration<100||duration>600000)throw std::invalid_argument("range");
  std::cout<<"target_pid="<<pid<<" pointer_bits=64\n"
   <<"LIMITATIONS: module presence is not API execution; absence is not proof of non-use.\n"
   <<"DirectInput8 calls=UNKNOWN RawInput calls=UNKNOWN Win32 keyboard/mouse calls=UNKNOWN\n"
   <<"Raw Input registrations cannot be queried for another process with GetRegisteredRawInputDevices.\n"
   <<"Focus sampled every 50ms; intermediate transitions and thread focus are UNKNOWN.\n";
  std::set<std::wstring> seen;HWND previous=nullptr;bool first=true,ok=false;
  auto start=GetTickCount64();ULONGLONG next=0;
  do{
   auto now=GetTickCount64()-start;HWND fg=GetForegroundWindow();DWORD owner=0;GetWindowThreadProcessId(fg,&owner);
   if(first||fg!=previous){std::cout<<"CONFIRMED foreground t_ms="<<now<<" hwnd="<<fg<<" pid="<<owner<<" target="<<(owner==pid)<<"\n";previous=fg;first=false;}
   if(now>=next){
    HANDLE snap=INVALID_HANDLE_VALUE;
    for(int retry=0;retry<5;++retry){snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(snap!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;Sleep(10);}
    if(snap==INVALID_HANDLE_VALUE){std::cout<<"UNKNOWN modules error="<<GetLastError()<<" t_ms="<<now<<"\n";}
    else{MODULEENTRY32W m{};m.dwSize=sizeof(m);if(Module32FirstW(snap,&m)){ok=true;do{if(seen.insert(m.szExePath).second){std::wcout<<L"CONFIRMED module_loaded t_ms="<<now<<L" name="<<m.szModule<<L" path="<<m.szExePath<<L"\n";if(_wcsicmp(m.szModule,L"dinput8.dll")==0)std::cout<<"LIKELY DirectInput8 availability; actual device creation/use UNKNOWN\n";}}while(Module32NextW(snap,&m));if(GetLastError()!=ERROR_NO_MORE_FILES)std::cout<<"UNKNOWN module_iteration error="<<GetLastError()<<"\n";}else std::cout<<"UNKNOWN Module32First error="<<GetLastError()<<"\n";CloseHandle(snap);}
    next=now+1000;
   }
   Sleep(50);
  }while(GetTickCount64()-start<duration);
  return ok?0:1;
 }catch(const std::exception& e){std::cerr<<"Invalid arguments: "<<e.what()<<"\n";return 2;}
}