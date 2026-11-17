// Controlled self-instrumentation only: patches this executable's imports and
// one DirectInput object owned by this process. No injection or remote access.
#define DIRECTINPUT_VERSION 0x0800
#include "hvi.hpp"
#include <dinput.h>
#include <fstream>
#include <cstring>
#include <vector>
static hvi::State state;
static BOOL WINAPI virtualKeyboard(PBYTE out){
 if(!out)return FALSE;for(int i=0;i<256;i++)out[i]=state.keyboard.byte(i);return TRUE;
}
static SHORT WINAPI virtualAsync(int key){return static_cast<SHORT>(state.keyboard.async(key));}
static HRESULT WINAPI virtualData(IDirectInputDevice8A*,DWORD size,LPDIDEVICEOBJECTDATA out,LPDWORD count,DWORD flags){
 if(size!=sizeof(DIDEVICEOBJECTDATA)||!count||(flags&~DIGDD_PEEK))return DIERR_INVALIDPARAM;
 bool overflow=state.keyboardEvents.overflowed();auto events=state.keyboardEvents.read(*count,(flags&DIGDD_PEEK)!=0);*count=static_cast<DWORD>(events.size());
 if(!out)return overflow?DI_BUFFEROVERFLOW:DI_OK;
 for(size_t i=0;i<events.size();i++){
  auto&e=events[i];unsigned scan=MapVirtualKeyA(e.control,MAPVK_VK_TO_VSC_EX);if(scan&0xff00)scan=(scan&0xff)|0x80;
  out[i]={};out[i].dwOfs=scan;out[i].dwData=static_cast<DWORD>(e.value);out[i].dwTimeStamp=static_cast<DWORD>(e.timestamp);out[i].dwSequence=static_cast<DWORD>(e.order);
 }return overflow?DI_BUFFEROVERFLOW:DI_OK;
}
static HRESULT WINAPI virtualDevice(IDirectInputDevice8A*,DWORD size,LPVOID out){
 if(size!=256||!out)return DIERR_INVALIDPARAM;
 auto bytes=static_cast<BYTE*>(out);std::memset(bytes,0,256);
 for(unsigned vk=0;vk<256;vk++)if(state.keyboard.keys[vk]){
  if(vk==VK_SHIFT||vk==VK_CONTROL||vk==VK_MENU)continue;
  unsigned scan=MapVirtualKeyA(vk,MAPVK_VK_TO_VSC_EX);if(scan&0xff00)scan=(scan&0xff)|0x80;if(scan&&scan<256)bytes[scan]=0x80;
 }return DI_OK;
}
struct Patch { ULONG_PTR* slot;ULONG_PTR original; };
static std::vector<Patch> patches;
static bool patchImport(const char* name,ULONG_PTR replacement){
 auto base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
 auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
 if(!rva)return false;
 for(auto desc=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);desc->Name;desc++){
  if(!desc->OriginalFirstThunk)continue;
  auto names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+desc->OriginalFirstThunk);
  auto slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+desc->FirstThunk);
  for(;names->u1.AddressOfData;names++,slots++){
   if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
   auto entry=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
   if(std::strcmp(reinterpret_cast<char*>(entry->Name),name))continue;
   DWORD old=0;if(!VirtualProtect(&slots->u1.Function,sizeof(ULONG_PTR),PAGE_READWRITE,&old))return false;
   patches.push_back({&slots->u1.Function,slots->u1.Function});slots->u1.Function=replacement;
   DWORD ignored;return VirtualProtect(&slots->u1.Function,sizeof(ULONG_PTR),old,&ignored)!=FALSE;
  }
 }return false;
}
int main(int argc,char**argv){
 if(argc<3||argc>4)return 2;bool focusTest=argc==4&&std::string(argv[3])=="--focus-test";uint32_t id=static_cast<uint32_t>(std::stoul(argv[1]));if(!id)return 2;
 std::ofstream log(argv[2]);if(!log)return 3;
 HINSTANCE instance=GetModuleHandleW(nullptr);
 HWND window=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"HVI controlled API probe",WS_OVERLAPPEDWINDOW,0,0,320,160,nullptr,nullptr,instance,nullptr);
 if(!window)return 4;ShowWindow(window,SW_SHOWNOACTIVATE);
 IDirectInput8A* input=nullptr;IDirectInputDevice8A* device=nullptr;
 HRESULT hr=DirectInput8Create(instance,DIRECTINPUT_VERSION,IID_IDirectInput8A,reinterpret_cast<void**>(&input),nullptr);
 if(FAILED(hr))return 5;
 hr=input->CreateDevice(GUID_SysKeyboard,&device,nullptr);if(FAILED(hr)){input->Release();return 6;}
 HRESULT coop=device->SetCooperativeLevel(window,DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
 HRESULT format=device->SetDataFormat(&c_dfDIKeyboard);HRESULT acquire=device->Acquire();
 BYTE baseline[256]{};BOOL nativeKeyboard=GetKeyboardState(baseline);SHORT nativeAsync=GetAsyncKeyState('A');
 BYTE nativeDI[256]{};HRESULT nativeResult=device->GetDeviceState(256,nativeDI);
 log<<"native keyboard="<<nativeKeyboard<<" asyncA="<<(nativeAsync<0)<<" di="<<nativeResult<<" coop="<<coop<<" format="<<format<<" acquire="<<acquire<<std::endl;
 if(FAILED(coop)||FAILED(format))return 7;
 // Clone only this object's vtable; leave all other COM objects untouched.
 void** original=*reinterpret_cast<void***>(device);std::array<void*,32> table{};
 std::copy(original,original+32,table.begin());table[9]=reinterpret_cast<void*>(&virtualDevice);table[10]=reinterpret_cast<void*>(&virtualData);
 *reinterpret_cast<void***>(device)=table.data();
 if(!patchImport("GetKeyboardState",reinterpret_cast<ULONG_PTR>(&virtualKeyboard))||!patchImport("GetAsyncKeyState",reinterpret_cast<ULONG_PTR>(&virtualAsync)))return 8;
 HANDLE pipe=CreateNamedPipeW(hvi::pipeName(id).c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,nullptr);
 int result=0;
 if(pipe==INVALID_HANDLE_VALUE)result=9;
 else if(!ConnectNamedPipe(pipe,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED)result=10;
 else {
  uint32_t last=0;
  for(;;){
   MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
   hvi::Event e;DWORD n=0;if(!ReadFile(pipe,&e,sizeof(e),&n,nullptr)||n!=sizeof(e))break;
   if(e.magic!=0x31495648||e.version!=1||e.session!=id||e.sequence<=last||!state.apply(e)){result=11;break;}
   if(e.kind==hvi::Kind::Focus&&focusTest){
    if(e.a){SetForegroundWindow(window);SetFocus(window);}else {HWND other=FindWindowW(L"STATIC",L"HVI focus sink");if(other)SetForegroundWindow(other);}
   }
   BYTE keys[256]{},di[256]{};BOOL ok=GetKeyboardState(keys);SHORT a=GetAsyncKeyState('A'),b=GetAsyncKeyState('B');HRESULT d=device->GetDeviceState(256,di);
   DIDEVICEOBJECTDATA buffered[256]{};DWORD count=256;HRESULT bufferedResult=device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),buffered,&count,DIGDD_PEEK);
   bool background=GetForegroundWindow()!=window;
   bool consistent=ok&&SUCCEEDED(d)&&((keys['A']!=0)==(a<0))&&((keys['B']!=0)==(b<0))&&((di[DIK_A]!=0)==(a<0))&&((di[DIK_B]!=0)==(b<0));
   DIDEVICEOBJECTDATA consumed[256]{};DWORD consumedCount=256;HRESULT consumedResult=device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),consumed,&consumedCount,0);
   DWORD remaining=INFINITE;device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),nullptr,&remaining,DIGDD_PEEK);
   bool bufferOk=consumedCount==count&&remaining==0&&SUCCEEDED(bufferedResult)&&SUCCEEDED(consumedResult);
   for(DWORD i=0;i<count;i++)bufferOk=bufferOk&&buffered[i].dwSequence==consumed[i].dwSequence;
   consistent=consistent&&bufferOk;
   log<<"sample session="<<id<<" seq="<<e.sequence<<" A="<<(a<0)<<" B="<<(b<0)<<" background="<<background<<" consistent="<<consistent
      <<" shift="<<state.keyboard.keys[VK_SHIFT]<<" x="<<state.mouse.x<<" y="<<state.mouse.y<<" wheel="<<state.mouse.wheel<<" button0="<<state.mouse.buttons[0]<<" buffered="<<count<<" diBuffered="<<bufferedResult;
   log<<" keys=[";for(int k=0;k<256;k++)if(state.keyboard.keys[k])log<<k<<",";
   log<<"] buttons=[";for(int k=0;k<5;k++)if(state.mouse.buttons[k])log<<k<<",";
   log<<"] horizontalWheel="<<state.mouse.horizontalWheel<<std::endl;
   if((!focusTest&&!background)||!consistent){result=12;break;}last=e.sequence;
   if(!WriteFile(pipe,&last,sizeof(last),&n,nullptr)||n!=sizeof(last)){result=13;break;}
   if(e.kind==hvi::Kind::Stop)break;
  }
 }
 state={};if(pipe!=INVALID_HANDLE_VALUE){DisconnectNamedPipe(pipe);CloseHandle(pipe);}
 for(auto p:patches){DWORD old;if(VirtualProtect(p.slot,sizeof(ULONG_PTR),PAGE_READWRITE,&old)){*p.slot=p.original;DWORD ignored;VirtualProtect(p.slot,sizeof(ULONG_PTR),old,&ignored);}}
 *reinterpret_cast<void***>(device)=original;device->Unacquire();device->Release();input->Release();DestroyWindow(window);
 return result;
}