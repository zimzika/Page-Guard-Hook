// dllmain.cpp : Define o ponto de entrada para o aplicativo DLL.
#include "framework.h"

#define print(x, ...) printf_s(x, __VA_ARGS__)

void DllEntry(HMODULE hMod);

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(DllEntry), nullptr, 0, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

__forceinline void OpenConsole()
{
    FILE* f;
    AllocConsole();
    freopen_s(&f, "CONOUT$", "w", stdout);
}

uintptr_t endScene = 0;
uintptr_t dwRetAddr = 0;

HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    using tCallEndScene = HRESULT(__stdcall*)(LPDIRECT3DDEVICE9);

    D3DRECT r = { 50,50,100,100 };

    pDevice->Clear(1, &r, D3DCLEAR_TARGET, D3DCOLOR_RGBA(255, 255, 255, 255), 0, 0);

    static tCallEndScene CallEndScene = (tCallEndScene)dwRetAddr;

    return CallEndScene(pDevice);
}

LONG __stdcall getUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
    {
        if (pExceptionInfo->ContextRecord->Eip == (uintptr_t)endScene)
        {
            pExceptionInfo->ContextRecord->Eip = (uintptr_t)hkEndScene;
        }

        pExceptionInfo->ContextRecord->EFlags |= 0x100;

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
    {
        DWORD dwOld;
        SYSTEM_INFO sSysInfo;

        GetSystemInfo(&sSysInfo);
        VirtualProtect((void*)endScene, sSysInfo.dwPageSize, PAGE_EXECUTE | PAGE_GUARD, &dwOld);

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void DllEntry(HMODULE hMod)
{
    OpenConsole();

    print("Dll started!\n");

    // vtable pointer = d3d9test.exe+0xCE08

    uintptr_t* pTable = *(uintptr_t**)((uintptr_t)GetModuleHandleA("d3d9test.exe") + 0xCE08);
    uintptr_t* vTable = (uintptr_t*)pTable[0];

    print("EndScene: 0x%X\n", vTable[42]);

    endScene = vTable[42];

    auto trampoline = VirtualAlloc(nullptr, 12, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
    {
        print("VirtualAlloc failed!\n");
        return;
    }

    memcpy(trampoline, (void*)endScene, 7);

    uintptr_t gatewayRelativeAddr = ((uintptr_t)endScene - (uintptr_t)trampoline) - 5;
    *(char*)((uintptr_t)trampoline + 7) = 0xE9;
    *(uintptr_t*)((uintptr_t)trampoline + 7 + 1) = gatewayRelativeAddr;

    dwRetAddr = (uintptr_t)trampoline;

    SetUnhandledExceptionFilter(getUnhandledExceptionFilter);

    DWORD dwOld = 0;
    SYSTEM_INFO sSysInfo;

    GetSystemInfo(&sSysInfo);
    VirtualProtect((void*)endScene, sSysInfo.dwPageSize, PAGE_EXECUTE | PAGE_GUARD, &dwOld);

    while (true)
    {
        if (GetAsyncKeyState(VK_END) & 1)
        {
            print("Exit!\n");
            FreeConsole();
            FreeLibraryAndExitThread(hMod, -1);
        }
        Sleep(1);
    }
}