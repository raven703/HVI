#pragma once
#include <windows.h>
#include <sddl.h>
#include <vector>
#include <string>
namespace hvi {
inline std::vector<BYTE> tokenInfo(HANDLE token,TOKEN_INFORMATION_CLASS kind){DWORD n=0;GetTokenInformation(token,kind,nullptr,0,&n);std::vector<BYTE> v(n);if(!n||!GetTokenInformation(token,kind,v.data(),n,&n))v.clear();return v;}
class PipeSecurity {
 PSECURITY_DESCRIPTOR descriptor=nullptr;
public:
 SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,FALSE};
 PipeSecurity(){
  HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return;
  auto u=tokenInfo(token,TokenUser);CloseHandle(token);if(u.empty())return;
  LPWSTR sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(u.data())->User.Sid,&sid))return;
  std::wstring s=L"D:P(A;;GA;;;"+std::wstring(sid)+L")";LocalFree(sid);
  if(ConvertStringSecurityDescriptorToSecurityDescriptorW(s.c_str(),SDDL_REVISION_1,&descriptor,nullptr))attributes.lpSecurityDescriptor=descriptor;
 }
 ~PipeSecurity(){if(descriptor)LocalFree(descriptor);}
 bool valid()const{return descriptor!=nullptr;}
};
// Must be called after a successful read, before applying even the first command.
// Trust boundary: same user + authentication/logon + Windows session, not signed EXE.
inline bool verifyPipeClient(HANDLE pipe,DWORD& pid){
 ULONG clientPid=0;if(!GetNamedPipeClientProcessId(pipe,&clientPid))return false;
 if(!ImpersonateNamedPipeClient(pipe))return false;
 HANDLE client=nullptr;BOOL opened=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&client);
 BOOL reverted=RevertToSelf();if(!opened||!reverted){if(client)CloseHandle(client);return false;}
 HANDLE self=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&self)){CloseHandle(client);return false;}
 auto cu=tokenInfo(client,TokenUser),su=tokenInfo(self,TokenUser);
 auto cs=tokenInfo(client,TokenStatistics),ss=tokenInfo(self,TokenStatistics);
 auto ci=tokenInfo(client,TokenSessionId),si=tokenInfo(self,TokenSessionId);
 CloseHandle(client);CloseHandle(self);
 if(cu.empty()||su.empty()||cs.empty()||ss.empty()||ci.empty()||si.empty())return false;
 auto a=reinterpret_cast<TOKEN_STATISTICS*>(cs.data())->AuthenticationId,b=reinterpret_cast<TOKEN_STATISTICS*>(ss.data())->AuthenticationId;
 bool ok=EqualSid(reinterpret_cast<TOKEN_USER*>(cu.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(su.data())->User.Sid)&&a.LowPart==b.LowPart&&a.HighPart==b.HighPart&&*reinterpret_cast<DWORD*>(ci.data())==*reinterpret_cast<DWORD*>(si.data());
 if(ok)pid=clientPid;return ok;
}
}