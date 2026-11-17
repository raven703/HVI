#pragma once
#include <windows.h>
// Owned-process, SDL2.dll-IAT-only diagnostics. No remote patching/injection.
void compatTraceInstall(HMODULE module);
void compatTraceWrite(const char* path,const char* mode,unsigned long long polls,unsigned long long virtualEvents);