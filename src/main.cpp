#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <atomic>

namespace {
HMODULE g_module = nullptr;
std::atomic<gco::Runtime*> g_runtime{nullptr};

void ScriptMain() {
    gco::Runtime runtime;
    g_runtime.store(&runtime, std::memory_order_release);
    runtime.runStage7Integrated();
    g_runtime.store(nullptr, std::memory_order_release);
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
        if (auto* runtime = g_runtime.load(std::memory_order_acquire); runtime != nullptr) {
            runtime->requestStop();
        }
        if (g_module != nullptr) {
            scriptUnregister(g_module);
            g_module = nullptr;
        }
        g_runtime.store(nullptr, std::memory_order_release);
        break;

    default:
        break;
    }

    return TRUE;
}
