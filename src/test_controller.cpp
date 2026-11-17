#include "hvi.hpp"
#include <filesystem>
#include <memory>
#include <sstream>
#include <fstream>
struct Session {
 PROCESS_INFORMATION process{};std::unique_ptr<hvi::NamedPipeTransport> pipe;uint32_t sequence=0;std::filesystem::path log;
};
int main(int argc,char**argv){
 unsigned count=argc>1?static_cast<unsigned>(std::stoul(argv[1])):2;if(count<2||count>16)return 2;
 wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);auto dir=std::filesystem::path(executable).parent_path();
 std::map<uint32_t,Session> sessions;int result=0;
 try{
  for(unsigned id=1;id<=count;id++){
   auto&s=sessions[id];s.log=dir/("controller-session-"+std::to_string(id)+".log");
   std::wstring command=L"\""+(dir/L"api_probe.exe").wstring()+L"\" "+std::to_wstring(id)+L" \""+s.log.wstring()+L"\"";
   STARTUPINFOW startup{};startup.cb=sizeof(startup);
   if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,dir.c_str(),&startup,&s.process))throw std::runtime_error("launch failed");
   // WaitNamedPipe can report not-found before the server has created its pipe.
   for(int retry=0;retry<100&&!s.pipe;retry++){try{s.pipe=std::make_unique<hvi::NamedPipeTransport>(id);}catch(...){Sleep(50);}}
   if(!s.pipe)throw std::runtime_error("probe connection failed");
  }
  std::cout<<"HVI Test Controller: target ID | down VK | up VK | button INDEX 0/1 | move DX DY | absolute X Y | wheel V H | state | reset | quit\n";
  uint32_t selected=1;std::string line;
  while(std::getline(std::cin,line)){
   std::istringstream in(line);std::string cmd;in>>cmd;if(cmd.empty())continue;if(cmd=="quit")break;
   if(cmd=="target"){uint32_t id;if(in>>id&&sessions.count(id))selected=id;else std::cout<<"Unknown target\n";continue;}
   auto&s=sessions.at(selected);hvi::Event e;e.session=selected;e.sequence=++s.sequence;bool valid=true;
   if(cmd=="down"||cmd=="up"){e.kind=hvi::Kind::Key;e.b=cmd=="down";valid=bool(in>>e.a);}
   else if(cmd=="button"){e.kind=hvi::Kind::Button;valid=bool(in>>e.a>>e.b);}
   else if(cmd=="move"||cmd=="absolute"||cmd=="wheel"){e.kind=cmd=="move"?hvi::Kind::Relative:cmd=="wheel"?hvi::Kind::Wheel:hvi::Kind::Absolute;valid=bool(in>>e.a>>e.b);}
   else if(cmd=="state")e.kind=hvi::Kind::Snapshot;
   else if(cmd=="reset")e.kind=hvi::Kind::Reset;
   else valid=false;
   if(!valid){std::cout<<"Invalid command\n";continue;}
   if(!s.pipe->deliver(e))throw std::runtime_error("probe rejected/disconnected");
   std::ifstream log(s.log);std::string last,current;while(std::getline(log,current))last=current;std::cout<<last<<"\n";
  }
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";result=1;}
 // Bounded child cleanup even if transport/initialization failed.
 for(auto&entry:sessions){auto&s=entry.second;s.pipe.reset();if(s.process.hProcess){if(WaitForSingleObject(s.process.hProcess,2000)==WAIT_TIMEOUT)TerminateProcess(s.process.hProcess,1);WaitForSingleObject(s.process.hProcess,2000);CloseHandle(s.process.hProcess);CloseHandle(s.process.hThread);}}
 return result;
}