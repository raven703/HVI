#pragma once
#include <windows.h>
#include <dinput.h>
#include "runtime_api.hpp"
// Test automation reads the last actual GUI API poll, not a controller command log.
constexpr DWORD HVI_GUI_QUERY=0x48564951;
struct GuiObservation {
 DWORD size=sizeof(GuiObservation),session=0,pid=0,foreground=0,rawCount=0;
 HRESULT keyboardResult=E_FAIL,mouseResult=E_FAIL;
 BYTE win32[256]{},directInput[256]{},native[256]{};
 DIMOUSESTATE2 mouse{};HviSnapshot runtime;
 uint64_t keyboardBuffered=0,mouseBuffered=0,polls=0;
};