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
// Current virtual state, not Win32's message-queue historical GetKeyState semantics.
HVI_API SHORT WINAPI HviGetKeyState(int key);
struct HviMouseState {
 DWORD size=sizeof(HviMouseState),version=1;
 uint64_t generation=0;
 int64_t x=0,y=0,dx=0,dy=0,wheel=0,horizontalWheel=0;
 BYTE buttons[5]{},reserved[3]{};
};
// Non-destructive application-local coordinates and cumulative deltas/wheels.
// This is deliberately not a GetCursorPos screen-coordinate replacement.
HVI_API BOOL WINAPI HviGetMouseState(HviMouseState* out);
HVI_API HRESULT WINAPI HviCreateMouse(BOOL wide,void** out);
HVI_API HRESULT WINAPI HviCreateKeyboard(BOOL wide,void** out);
HVI_API BOOL WINAPI HviGetSnapshot(HviSnapshot* out);
HVI_API BOOL WINAPI HviReadEvents(HviDiagnosticEvent* out,DWORD* count);
// Independent cursor, non-destructive bounded command journal. Order never resets
// during this DLL lifetime; sequence is connection-local and may reset on reconnect.
struct HviJournalRecord {uint64_t order=0,time=0;hvi::Event command;};
struct HviJournalRead {
 DWORD size=sizeof(HviJournalRead),version=1,count=0,resync=0;
 uint64_t cursor=0,latest=0,lost=0,generation=0;
 BYTE keyboard[256]{};BYTE buttons[5]{};BYTE padding[3]{};
 int64_t x=0,y=0,dx=0,dy=0,wheel=0,horizontalWheel=0;
};
HVI_API BOOL WINAPI HviReadJournal(HviJournalRead* read,HviJournalRecord* records,DWORD capacity);