#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <cstdio>

namespace {
// Read-only snapshots only: no OpenProcess, memory reads, hooks or HVI runtime.
struct Snapshot {
 HANDLE value=INVALID_HANDLE_VALUE;
 Snapshot(DWORD flags,DWORD pid){
  for(int retry=0;retry<5;++retry){
   value=CreateToolhelp32Snapshot(flags,pid);
   if(value!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;
   if(retry<4)Sleep(10);
  }
 }
 ~Snapshot(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 Snapshot(const Snapshot&)=delete;
 Snapshot& operator=(const Snapshot&)=delete;
};
std::string utf8(const std::wstring& text){
 if(text.empty())return {};
 int size=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
 if(!size)throw std::runtime_error("UTF-8 conversion failed");
 std::string out(size,'\0');
 if(!WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),size,nullptr,nullptr))throw std::runtime_error("UTF-8 conversion failed");
 return out;
}
std::string quote(const std::string& text){
 std::string out="\"";const char* hex="0123456789abcdef";
 for(unsigned char ch:text){
  if(ch=='"'||ch=='\\'){out+='\\';out+=static_cast<char>(ch);}
  else if(ch<32){out+="\\u00";out+=hex[ch>>4];out+=hex[ch&15];}
  else out+=static_cast<char>(ch);
 }
 return out+'"';
}
DWORD number(const std::wstring& text){
 if(text.empty()||text.find_first_not_of(L"0123456789")!=std::wstring::npos)throw std::invalid_argument("expected unsigned decimal number");
 auto value=std::stoull(text);
 if(value>MAXDWORD)throw std::invalid_argument("number exceeds DWORD");
 return static_cast<DWORD>(value);
}
const char* role(const std::wstring& name){
 if(_wcsicmp(name.c_str(),L"exefile.exe")==0||_wcsicmp(name.c_str(),L"eve.exe")==0)return "client_candidate";
 if(_wcsicmp(name.c_str(),L"eve-online.exe")==0)return "launcher_candidate";
 if(_wcsicmp(name.c_str(),L"eve_crashmon.exe")==0)return "crash_monitor_candidate";
 return "other";
}
struct Process {DWORD pid;std::wstring name;};
struct Target {
 DWORD pid;std::wstring name;std::set<std::wstring> seen;
 bool modulesComplete=false;unsigned snapshots=0;
 std::vector<std::string> modules,focus,errors;
};
std::string array(const std::vector<std::string>& values){
 std::string out="[";
 for(const auto& value:values){if(out.size()>1)out+=',';out+=value;}
 return out+']';
}
std::string timestamp(){
 SYSTEMTIME t{};GetSystemTime(&t);char out[32];
 std::snprintf(out,sizeof(out),"%04u-%02u-%02uT%02u:%02u:%02uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
 return out;
}
void error(Target& target,const char* operation,DWORD code,ULONGLONG time){
 target.errors.push_back("{\"t_ms\":"+std::to_string(time)+",\"operation\":"+quote(operation)+",\"win32_error\":"+std::to_string(code)+'}');
 std::cout<<"UNKNOWN pid="<<target.pid<<" "<<operation<<" error="<<code<<" t_ms="<<time<<'\n';
}
void modules(Target& target,ULONGLONG time){
 Snapshot snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,target.pid);
 if(snapshot.value==INVALID_HANDLE_VALUE){error(target,"CreateToolhelp32Snapshot",GetLastError(),time);return;}
 MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
 if(!Module32FirstW(snapshot.value,&entry)){error(target,"Module32FirstW",GetLastError(),time);return;}
 do{
  if(target.seen.insert(entry.szExePath).second){
   auto name=utf8(entry.szModule),path=utf8(entry.szExePath);
   target.modules.push_back("{\"first_seen_ms\":"+std::to_string(time)+",\"name\":"+quote(name)+",\"path\":"+quote(path)+'}');
   std::cout<<"CONFIRMED module_loaded pid="<<target.pid<<" t_ms="<<time<<" name="<<name<<" path="<<path<<'\n';
  }
 }while(Module32NextW(snapshot.value,&entry));
 DWORD last=GetLastError();
 if(last!=ERROR_NO_MORE_FILES)error(target,"Module32NextW",last,time);
 else{target.modulesComplete=true;++target.snapshots;}
}
void usage(){
 std::cout<<"Usage: input_backend_inspector <pid|self|--eve|--list-eve> [duration-ms 100..600000] [--json path]\n"
  <<"--list-eve: name-based candidates only; --eve: observe discovered client candidates, not launchers.\n"
  <<"Default duration 5000ms; fixed startup targets. --help shows this text.\n";
}
}

int wmain(int argc,wchar_t** argv){
 static_assert(sizeof(void*)==8,"Inspector requires Windows x64");
 try{
  if(argc==2&&std::wstring(argv[1])==L"--help"){usage();return 0;}
  if(argc<2){usage();return 2;}
  std::wstring selector=argv[1],output;DWORD duration=5000;bool haveDuration=false;
  bool list=selector==L"--list-eve",eve=selector==L"--eve";DWORD pid=0;
  if(!list&&!eve){pid=selector==L"self"?GetCurrentProcessId():number(selector);if(!pid)throw std::invalid_argument("PID must be nonzero");}
  for(int i=2;i<argc;++i){
   std::wstring arg=argv[i];
   if(arg==L"--json"){
    if(!output.empty()||++i>=argc||!argv[i][0])throw std::invalid_argument("--json requires one path");
    output=argv[i];
   }else{
    if(list||haveDuration)throw std::invalid_argument("unexpected argument");
    duration=number(arg);haveDuration=true;
   }
  }
  if(duration<100||duration>600000)throw std::invalid_argument("duration out of range");
  std::ofstream file;
  if(!output.empty()){
   file.open(std::filesystem::path(output),std::ios::binary|std::ios::trunc);
   if(!file){std::cerr<<"Cannot open JSON output\n";return 3;}
  }
  std::string started=timestamp();std::vector<Process> inventory;DWORD inventoryError=0;
  {
   Snapshot snapshot(TH32CS_SNAPPROCESS,0);
   if(snapshot.value==INVALID_HANDLE_VALUE)inventoryError=GetLastError();
   else{
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Process32FirstW(snapshot.value,&entry))inventoryError=GetLastError();
    else{
     do{inventory.push_back({entry.th32ProcessID,entry.szExeFile});}while(Process32NextW(snapshot.value,&entry));
     if(GetLastError()!=ERROR_NO_MORE_FILES)inventoryError=GetLastError();
    }
   }
  }
  std::vector<Target> targets;std::vector<std::string> candidates;
  for(const auto& process:inventory){
   std::string kind=role(process.name);
   if(kind!="other"){
    std::cout<<"CONFIRMED candidate pid="<<process.pid<<" name="<<utf8(process.name)<<" role="<<kind<<" identity=UNKNOWN\n";
    candidates.push_back("{\"pid\":"+std::to_string(process.pid)+",\"name\":"+quote(utf8(process.name))+",\"role\":"+quote(kind)+",\"identity\":\"UNKNOWN\"}");
   }
   if(eve&&kind=="client_candidate")targets.push_back(Target{process.pid,process.name});
  }
  if(!eve&&!list){
   std::wstring name;
   for(const auto& process:inventory)if(process.pid==pid)name=process.name;
   targets.push_back(Target{pid,name});
  }
  if(inventoryError)std::cout<<"UNKNOWN process_inventory error="<<inventoryError<<'\n';
  std::cout<<"LIMITATIONS: module presence is not API execution; absence is not proof of non-use.\n"
   <<"DirectInput8 calls=UNKNOWN RawInput calls=UNKNOWN Win32 keyboard/mouse calls=UNKNOWN\n"
   <<"Raw Input registrations are current-application scoped, not queried for foreign processes.\n"
   <<"Focus sampled every 50ms; intermediate transitions and thread focus are UNKNOWN.\n";
  auto start=GetTickCount64();ULONGLONG next=0;HWND previous=nullptr;DWORD previousOwner=0;bool first=true;
  if(!list&&!targets.empty())do{
   auto now=GetTickCount64()-start;HWND foreground=GetForegroundWindow();DWORD owner=0;
   if(foreground)GetWindowThreadProcessId(foreground,&owner);
   if(first||foreground!=previous||owner!=previousOwner){
    for(auto& target:targets){
     std::string state=owner?(owner==target.pid?"foreground":"background"):"UNKNOWN";
     target.focus.push_back("{\"t_ms\":"+std::to_string(now)+",\"foreground_pid\":"+std::to_string(owner)+",\"foreground_hwnd\":"+quote(std::to_string(reinterpret_cast<ULONG_PTR>(foreground)))+",\"state\":"+quote(state)+'}');
     std::cout<<(owner?"CONFIRMED":"UNKNOWN")<<" focus pid="<<target.pid<<" t_ms="<<now<<" foreground_pid="<<owner<<" state="<<state<<'\n';
    }
    first=false;previous=foreground;previousOwner=owner;
   }
   if(now>=next){for(auto& target:targets)modules(target,now);next=GetTickCount64()-start+1000;}
   auto elapsed=GetTickCount64()-start;
   if(elapsed>=duration)break;
   Sleep(static_cast<DWORD>((duration-elapsed)<50?(duration-elapsed):50));
  }while(GetTickCount64()-start<duration);
  bool complete=list?!inventoryError:!targets.empty();std::vector<std::string> observations;
  for(const auto& target:targets){
   complete=complete&&target.modulesComplete;
   observations.push_back("{\"pid\":"+std::to_string(target.pid)+",\"name\":"+quote(utf8(target.name))+
    ",\"module_enumeration\":"+quote(target.modulesComplete?"CONFIRMED":"UNKNOWN")+
    ",\"complete_snapshots\":"+std::to_string(target.snapshots)+",\"modules\":"+array(target.modules)+
    ",\"focus\":"+array(target.focus)+",\"errors\":"+array(target.errors)+
    ",\"mechanisms\":{\"win32\":\"UNKNOWN\",\"directinput\":\"UNKNOWN\",\"raw_input\":\"UNKNOWN\"}}");
  }
  if(eve&&inventoryError)complete=false;
  int result=complete?0:1;
  if(!output.empty()){
   file<<"{\"schema_version\":1,\"started_utc\":"<<quote(started)<<",\"mode\":"<<quote(list?"list-eve":eve?"eve":"pid")
    <<",\"pointer_bits\":64,\"requested_duration_ms\":"<<(list?0:duration)<<",\"elapsed_ms\":"<<GetTickCount64()-start
    <<",\"focus_interval_ms\":50,\"module_interval_ms\":1000,\"process_inventory_error\":"<<inventoryError
    <<",\"candidates\":"<<array(candidates)<<",\"targets\":"<<array(observations)<<",\"exit_code\":"<<result
    <<",\"limitations\":[\"Names are candidates, not verified EVE identity\",\"Module presence is not API execution; absence is not proof of non-use\",\"Fixed startup targets; PID reuse and process lifetime are UNKNOWN\",\"First-seen modules only; unloads and transient loads may be missed\",\"Sampled desktop foreground is not thread focus or input acceptance\",\"Raw Input registrations are current-application scoped\",\"Paths may contain personal information; redact before sharing\"]}\n";
   file.flush();
   if(!file){std::cerr<<"Cannot write JSON output\n";return 3;}
  }
  if(eve&&targets.empty())std::cout<<"UNKNOWN no client candidates found; use --list-eve or an explicit verified PID\n";
  return result;
 }catch(const std::invalid_argument& e){std::cerr<<"Invalid arguments: "<<e.what()<<'\n';return 2;}
 catch(const std::out_of_range& e){std::cerr<<"Invalid arguments: "<<e.what()<<'\n';return 2;}
 catch(const std::exception& e){std::cerr<<"Inspector failure: "<<e.what()<<'\n';return 3;}
}