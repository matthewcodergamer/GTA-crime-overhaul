#include "Runtime.h"

#include <Windows.h>
#include <main.h>

namespace {
HMODULE g_module = nullptr;

void ScriptMain() {
    gco::Runtime runtime;
    runtime.run();
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_module = module;
        DisableThreadLibraryCalls(module);
        scriptRegister(module, ScriptMain);
        break;

    case DLL_PROCESS_DETACH:
        if (g_module != nullptr) {
            scriptUnregister(g_module);
            g_module = nullptr;
        }
        break;

    default:
        break;
    }

    return TRUE;
}
