#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <string>
#include <map>
#include <stdexcept>
#include <iostream>
#include "input_state.hpp"
namespace hvi {
struct Diagnostics { uint64_t delivered=0,rejected=0; };
struct InputAdapter { virtual ~InputAdapter()=default;virtual bool deliver(const Event&)=0; };
class SessionManager {
 std::map<uint32_t,InputAdapter*> sessions;
public:
 bool add(uint32_t id,InputAdapter& a){return id&&sessions.emplace(id,&a).second;}
 void remove(uint32_t id){sessions.erase(id);}
 InputAdapter* find(uint32_t id){auto i=sessions.find(id);return i==sessions.end()?nullptr:i->second;}
};
struct InputRouter {
 SessionManager& sessions;Diagnostics stats;
 bool route(const Event&e){auto a=sessions.find(e.session);if(!a||!a->deliver(e)){++stats.rejected;return false;}++stats.delivered;return true;}
};
inline std::wstring pipeName(uint32_t id){return L"\\\\.\\pipe\\HVI.experimental."+std::to_wstring(id);}
class NamedPipeTransport:public InputAdapter {
 HANDLE handle=INVALID_HANDLE_VALUE;
 uint32_t session;
public:
 explicit NamedPipeTransport(uint32_t id):session(id){
  auto name=pipeName(id);
  if(!WaitNamedPipeW(name.c_str(),5000))throw std::runtime_error("pipe unavailable");
  handle=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);
  if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("pipe connection failed");
 }
 ~NamedPipeTransport(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
 NamedPipeTransport(const NamedPipeTransport&)=delete;
 NamedPipeTransport& operator=(const NamedPipeTransport&)=delete;
 bool deliver(const Event&e)override{
  if(e.session!=session)return false;
  DWORD n=0;uint32_t ack=0;
  return WriteFile(handle,&e,sizeof(e),&n,nullptr)&&n==sizeof(e)&&ReadFile(handle,&ack,sizeof(ack),&n,nullptr)&&n==sizeof(ack)&&ack==e.sequence;
 }
};
}