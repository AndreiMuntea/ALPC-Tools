/**
 * @file        ALPC-Tools/ALPCMon_Dll/DllMain.cpp
 *
 * @brief       Entry point of the user mode dll.
 *              This is injected in running processes.
 *              Uses detours to hook ALPC* related APIs.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "HookEngine.hpp"

/**
 * @brief       Entry point into a dynamic-link library (DLL).
 *              When the system starts or terminates a process or thread,
 *              it calls the entry-point function for each loaded DLL using
 *              the first thread of the process. The system also calls the
 *              entry-point function for a DLL when it is loaded or unloaded
 *              using the LoadLibrary and FreeLibrary functions.
 *
 * @param[in]   Module          - A handle to the DLL module. The value is the base address of the DLL.
 *                                The HINSTANCE of a DLL is the same as the HMODULE of the DLL.
 * @param[in]   ReasonForCall   - The reason code that indicates why the DLL entry-point function is being called.
 * @param[in]   Reserved        - If Reason is DLL_PROCESS_ATTACH, it is NULL for dynamic loads and non-NULL for static loads.
 *                                If Reason is DLL_PROCESS_DETACH, it is NULL if FreeLibrary has been called or the DLL
 *                                load failed and non-NULL if the process is terminating.
 *
 * @return      We always return true to not disrupt the application startup.
 */
BOOL APIENTRY
DllMain(
    _In_ HMODULE Module,
    _In_ DWORD  ReasonForCall,
    _In_ LPVOID Reserved
)
{
    UNREFERENCED_PARAMETER(Module);
    UNREFERENCED_PARAMETER(Reserved);

    NTSTATUS status = STATUS_SUCCESS;

    switch (ReasonForCall)
    {
    case DLL_PROCESS_ATTACH:
    {
        status = xpf::SplitAllocatorInitializeSupport();
        if (!NT_SUCCESS(status))
        {
            break;
        }

        status = HookEngineInitialize();
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        HookEngineDeinitialize();
        xpf::SplitAllocatorDeinitializeSupport();
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    default:
        break;
    }

    return NT_SUCCESS(status) ? TRUE
                              : FALSE;
}
