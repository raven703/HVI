#include "hvi.hpp"
#include <fstream>
int main(int argc,char**argv){
 if(argc!=3)return 2;
 uint32_t id=static_cast<uint32_t>(std::stoul(argv[1]));if(!id)return 2;
 std::ofstream log(argv[2]);if(!log)return 3;
 HANDLE pipe=CreateNamedPipeW(hvi::pipeName(id).c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,nullptr);
 if(pipe==INVALID_HANDLE_VALUE)return 4;
 hvi::State state;bool stop=false;
 while(!stop){
  if(!ConnectNamedPipe(pipe,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED)break;
  uint32_t last=0;
  for(;;){
   hvi::Event e;DWORD n=0;
   if(!ReadFile(pipe,&e,sizeof(e),&n,nullptr)||n!=sizeof(e))break;
   if(e.magic!=0x31495648||e.version!=1||e.session!=id||e.sequence<=last||!state.apply(e))break;
   last=e.sequence;
   log<<"event "<<id<<" "<<last<<" "<<static_cast<uint32_t>(e.kind)<<" "<<e.a<<" "<<e.b<<" x="<<state.mouse.x<<" y="<<state.mouse.y<<" A="<<state.keyboard.keys[65]<<" B="<<state.keyboard.keys[66]<<std::endl;
   if(!WriteFile(pipe,&last,sizeof(last),&n,nullptr)||n!=sizeof(last))break;
   if(e.kind==hvi::Kind::Stop){stop=true;break;}
  }
  state={};log<<"disconnect reset A=0 B=0 x=0 y=0"<<std::endl;
  DisconnectNamedPipe(pipe);
 }
 CloseHandle(pipe);return 0;
}