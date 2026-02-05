#pragma once

#include <string>

namespace gpuoverlay {

// Inject DLL into target process. Returns true on success.
// dllPath: full path to GPUOverlayHook.dll
// pid: target process ID
bool inject_dll(const std::wstring& dllPath, unsigned long pid);

}  // namespace gpuoverlay
