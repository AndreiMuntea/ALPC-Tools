/**
 * @file        ALPC-Tools/ALPCMon_Dll/HookEngine.cpp
 *
 * @brief       This is responsible for installing the hooks.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "HookEngine.hpp"
#include "detours.h"

#include "AlpcMon.hpp"


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       HookEnginePrepareThreads                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Must_inspect_result_
static NTSTATUS XPF_API
HookEnginePrepareThreads(
    void
) noexcept(true)
{
    //
    // What this API does is adding all threads in the application to the current tranzaction.
    // From detours docs:
    //
    // When a detour transaction commits, Detours insures that all threads enlisted in the transaction
    // via the DetourUpdateThread API are updated if their instruction pointer lies within the rewritten
    // code in either the target function or the trampoline function.
    //
    // Threads not enlisted in the transaction are not updated when the transaction commits.
    // As a result, they may attempt to execute an illegal combination of oldand new code.
    //

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Buffer buffer;

    //
    // 1. Capture all threads.
    //
    for (size_t retry = 0; retry < 100; ++retry)
    {
        status = buffer.Resize(retry * 0x1000 + 0x1000);
        if (!NT_SUCCESS(status))
        {
            continue;
        }

        ULONG retLength = 0;
        status = ::NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::SystemProcessInformation,
                                            buffer.GetBuffer(),
                                            static_cast<ULONG>(buffer.GetSize()),
                                            &retLength);
        if (NT_SUCCESS(status))
        {
            if (retLength == 0)
            {
                status = STATUS_INVALID_BUFFER_SIZE;
                continue;
            }
            break;
        }
    }
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // 2. Search information about the current process.
    //
    SYSTEM_PROCESS_INFORMATION* processEntry = static_cast<SYSTEM_PROCESS_INFORMATION*>(buffer.GetBuffer());
    while (processEntry != nullptr)
    {
        if (HandleToUlong(processEntry->UniqueProcessId) == ::GetCurrentProcessId())
        {
            /* Found our process. */
            break;
        }
        if (0 == processEntry->NextEntryOffset)
        {
            /* Could not find info about this process. Shouldn't happen, but check here. */
            return STATUS_NOT_FOUND;
        }

        processEntry = static_cast<SYSTEM_PROCESS_INFORMATION*>(xpf::AlgoAddToPointer(processEntry,
                                                                                      processEntry->NextEntryOffset));
    }
    if (nullptr == processEntry)
    {
        /* Bail on error. */
        return STATUS_NOT_FOUND;
    }

    //
    // 3. Enqueue the threads in transaction.
    //
    SYSTEM_THREAD_INFORMATION* threadEntry = nullptr;
    for (ULONG i = 0; i < processEntry->NumberOfThreads; ++i)
    {
        threadEntry = static_cast<SYSTEM_THREAD_INFORMATION*>(xpf::AlgoAddToPointer(processEntry,
                                                                                    sizeof(SYSTEM_PROCESS_INFORMATION) +
                                                                                    i * sizeof(SYSTEM_THREAD_INFORMATION)));
        if (HandleToULong(threadEntry->ClientId.UniqueThread) == ::GetCurrentThreadId())
        {
            /* Current thread is treated separately. */
            continue;
        }

        /* Open a handle to this thread. */
        HANDLE threadHandle = ::OpenThread(MAXIMUM_ALLOWED,
                                           FALSE,
                                           HandleToULong(threadEntry->ClientId.UniqueThread));
        if (NULL == threadHandle || INVALID_HANDLE_VALUE == threadHandle)
        {
            return STATUS_INVALID_HANDLE;
        }

        /* Enqueue in transaction. */
        #pragma warning(suppress: 6001)  // SAL is confused here :(
            LONG detourError = DetourUpdateThread(threadHandle);

        /* Close the handle regardless. */
        BOOL closeResult = CloseHandle(threadHandle);
        XPF_VERIFY(FALSE != closeResult);

        /* Now inpsect the status - if we fail, we stop. */
        if (NO_ERROR != detourError)
        {
            return STATUS_INVALID_TRANSACTION;
        }
    }

    //
    // 4. Enqueue the current thread.
    //
    // Calling DetourUpdateThread with a non-pseudo handle
    // to the current thread is currently unsupported and will result in application hangs.
    //
    LONG detourErrorCrtThread = DetourUpdateThread(::GetCurrentThread());
    return (NO_ERROR != detourErrorCrtThread) ? STATUS_INVALID_TRANSACTION
                                              : STATUS_SUCCESS;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       HookEnginePrepareHooks                                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Must_inspect_result_
static NTSTATUS XPF_API
HookEnginePrepareHooks(
    bool Install
) noexcept(true)
{
    //
    // What this API does is installing or uninstalling all hooks in the current transaction.
    //

    #ifndef DOXYGEN_SHOULD_SKIP_THIS
    #define HOOK_ENGINE_EDIT_HOOK(hook)                                                                          /* NOLINT(*) */  \
    {                                                                                                            /* NOLINT(*) */  \
        if (Install)                                                                                             /* NOLINT(*) */  \
        {                                                                                                        /* NOLINT(*) */  \
            HMODULE moduleHandle = ::GetModuleHandleW(hook.DllName.Buffer());                                    /* NOLINT(*) */  \
            if (NULL == moduleHandle)                                                                            /* NOLINT(*) */  \
            {                                                                                                    /* NOLINT(*) */  \
                return STATUS_NOT_FOUND;                                                                         /* NOLINT(*) */  \
            }                                                                                                    /* NOLINT(*) */  \
            void* api = ::GetProcAddress(moduleHandle, hook.ApiName.Buffer());                                   /* NOLINT(*) */  \
            if (NULL == api)                                                                                     /* NOLINT(*) */  \
            {                                                                                                    /* NOLINT(*) */  \
                return STATUS_NOT_FOUND;                                                                         /* NOLINT(*) */  \
            }                                                                                                    /* NOLINT(*) */  \
            hook.OriginalApi = api;                                                                              /* NOLINT(*) */  \
        }                                                                                                        /* NOLINT(*) */  \
        LONG detourError = (Install) ? DetourAttach(&hook.OriginalApi, hook.HookApi)                             /* NOLINT(*) */  \
                                     : DetourDetach(&hook.OriginalApi, hook.HookApi);                            /* NOLINT(*) */  \
        if (NO_ERROR != detourError)                                                                             /* NOLINT(*) */  \
        {                                                                                                        /* NOLINT(*) */  \
            return STATUS_INVALID_TRANSACTION;                                                                   /* NOLINT(*) */  \
        }                                                                                                        /* NOLINT(*) */  \
    };
    #endif  // DOXYGEN_SHOULD_SKIP_THIS

    HOOK_ENGINE_EDIT_HOOK(gNtAlpcConnectPortHook);
    HOOK_ENGINE_EDIT_HOOK(gNtAlpcSendWaitReceivePortHook);
    HOOK_ENGINE_EDIT_HOOK(gNtAlpcDisconnectPortHook);

    #undef HOOK_ENGINE_EDIT_HOOK

    return STATUS_SUCCESS;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       HookEngineChangeState                                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
_Must_inspect_result_
static NTSTATUS XPF_API
HookEngineChangeState(
    _In_ bool InstallHooks
) noexcept(true)
{
    LONG error = NO_ERROR;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // 1. Prepare detour transaction.
    //
    error = DetourTransactionBegin();
    if (error != NO_ERROR)
    {
        if (InstallHooks)
        {
            XPF_ASSERT(false);
        }
        return STATUS_INVALID_TRANSACTION;
    }

    //
    // 2. Prepare the application threads.
    //
    status = HookEnginePrepareThreads();
    if (!NT_SUCCESS(status))
    {
        if (InstallHooks)
        {
            XPF_ASSERT(false);
        }
        goto CleanUp;
    }

    //
    // 3. Install or uninstall the actual hooks.
    //
    status = HookEnginePrepareHooks(InstallHooks);
    if (!NT_SUCCESS(status))
    {
        if (InstallHooks)
        {
            XPF_ASSERT(false);
        }
        goto CleanUp;
    }

    //
    // 4. Commit the transaction.
    //
    error = DetourTransactionCommit();
    if (error != NO_ERROR)
    {
        if (InstallHooks)
        {
            XPF_ASSERT(false);
        }

        status = STATUS_INVALID_TRANSACTION;
        goto CleanUp;
    }

    //
    // All good.
    //
    status = STATUS_SUCCESS;

CleanUp:
    if (!NT_SUCCESS(status))
    {
        error = DetourTransactionAbort();
        XPF_VERIFY(NO_ERROR == error);
    }
    return status;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       HookEngineInitialize                                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Must_inspect_result_
NTSTATUS XPF_API
HookEngineInitialize(
    void
) noexcept(true)
{
    return HookEngineChangeState(true);
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       HookEngineDeinitialize                                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
HookEngineDeinitialize(
    void
) noexcept(true)
{
    NTSTATUS status = HookEngineChangeState(false);
    XPF_VERIFY(NT_SUCCESS(status));
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       HookEngineNotifyKernel                                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

NTSTATUS XPF_API
HookEngineNotifyKernel(
    _Inout_ UM_KM_MESSAGE_HEADER* Message
) noexcept(true)
{
    uint32_t messageSize = 0;
    ULONG retLength = 0;

    //
    // Sanity checks.
    //
    XPF_ASSERT(NULL != Message);
    XPF_ASSERT(Message->RequestType == UM_KM_REQUEST_TYPE);
    XPF_ASSERT(Message->ProviderSignature == UM_KM_CALLBACK_SIGNATURE);
    XPF_ASSERT(Message->Reserved == 0);

    //
    // Compute the full message size.
    //
    bool isSuccess = xpf::ApiNumbersSafeAdd(uint32_t{ sizeof(UM_KM_MESSAGE_HEADER) },
                                            Message->BufferLength,
                                            &messageSize);
    if (!isSuccess)
    {
        return STATUS_INTEGER_OVERFLOW;
    }

    //
    // SystemFirmwareTableInformation - 0x4c
    // https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/query.htm
    //
    return ::NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(0x4C),
                                      Message,
                                      messageSize,
                                      &retLength);
}
