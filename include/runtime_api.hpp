#pragma once
#include "input_state.hpp"
#ifdef HVI_RUNTIME_BUILD
#define HVI_API extern "C" __declspec(dllexport)
#else
#define HVI_API extern "C" __declspec(dllimport)
#endif
// Versioned, fixed-layout diagnostics. No STL or owning pointers cross the DLL ABI.
struct HviSnapshot {
 DWORD size=sizeof(HviSnapshot),version=1,session=0,pid=0,connected=0,clientPid=0,lastError=0,liveObjects=0;
 uint64_t generation=0,accepted=0,rejected=0,timeouts=0,ioErrors=0,pendingCancellations=0,reconnects=0;
 int64_t x=0,y=0,dx=0,dy=0,wheel=0;
 BYTE buttons[5]{},reserved[3]{};
};
struct HviDiagnosticEvent {uint64_t order=0,time=0;DWORD sequence=0,kind=0;int32_t control=0;int64_t value=0;};
HVI_API BOOL WINAPI HviInitialize(UINT session);
HVI_API void WINAPI HviShutdown();
HVI_API void WINAPI HviSetVirtual(BOOL enabled);
HVI_API BOOL WINAPI HviGetKeyboardState(PBYTE out);
HVI_API SHORT WINAPI HviGetAsyncKeyState(int key);
HVI_API HRESULT WINAPI HviCreateMouse(BOOL wide,void** out);
HVI_API HRESULT WINAPI HviCreateKeyboard(BOOL wide,void** out);
HVI_API BOOL WINAPI HviGetSnapshot(HviSnapshot* out);
HVI_API BOOL WINAPI HviReadEvents(HviDiagnosticEvent* out,DWORD* count);