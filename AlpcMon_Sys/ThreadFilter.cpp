/**
 * @file        ALPC-Tools/AlpcMon_Sys/ThreadFilter.cpp
 *
 * @brief       In this file we define the functionality related to
 *              thread filtering.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "KmHelper.hpp"
#include "Events.hpp"
#include "globals.hpp"

#include "ThreadFilter.hpp"
#include "trace.hpp"

 /**
  * @brief   Put everything below here in paged section.
  */
XPF_SECTION_PAGED;

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Structure definition area                                                          |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief   We are compiling with older DDK to be compatible with windows 7.
 *          This does not mean we will not support filtering new features.
 *          We just have to replicate the definitions provided by M$.
 *
 * @details https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ne-ntddk-_pscreatethreadnotifytype
 */
typedef enum _ENUM_PSCREATETHREADNOTIFYTYPE
{
    /**
     * @brief  From MSDN: "The driver-registered callback function is executed on the new non-system thread,
     *                     which enables the callback function to perform tasks such as setting the initial thread context."
     */
    PsCreateThreadNotifyNonSystem,

    /**
     * @brief  From MSDN: "Indicates that the driver-registered callback function is invoked for threads of all subsystems.
     *                     Drivers can call NtQueryInformationThread to determine the underlying subsystem.
     *                     The query retrieves a SUBSYSTEM_INFORMATION_TYPE value."
     */
    PsCreateThreadNotifySubsystems
} ENUM_PSCREATETHREADNOTIFYTYPE;

/**
 *  @brief      Supported from Windows 10, version 1703. The PsSetCreateThreadNotifyRoutineEx  routine
 *              registers or removes a callback routine that notifies the caller when a thread is created or deleted.
 *
 *  @param[in]  NotifyType          - Indicates the type of thread  notification.
 *
 *  @param[in]  NotifyInformation   - Provides the address of the notification information for the specified type of thread notification.
 *
 * @return      An ntstatus error code.
 *
 * @note        https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreatethreadnotifyroutineex
 */
typedef NTSTATUS (NTAPI *PFUNC_PsSetCreateThreadNotifyRoutineEx)(
    _In_ ENUM_PSCREATETHREADNOTIFYTYPE NotifyType,
    _In_ PVOID NotifyInformation
) noexcept(true);

/**
 * @brief   A global variable that will store the address of PsSetCreateThreadNotifyRoutineEx
 *          or NULL if we fail to find it. The resolution is attempted during ThreadFilterStart.
 *          After we unregister the routine (ThreadFilterStop), this is set back to NULL.
 */
static volatile PFUNC_PsSetCreateThreadNotifyRoutineEx gApiPsSetCreateThreadNotifyRoutineEx = nullptr;

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Forward definition of methods private to this module only.                         |
/// |                              Skipped by doxygen as they are annotated below.                                    |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

#ifndef DOXYGEN_SHOULD_SKIP_THIS

_Function_class_(PCREATE_THREAD_NOTIFY_ROUTINE)
static void NTAPI
ThreadFilterThreadNotifyRoutineCallback(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
) noexcept(true);

_Function_class_(PCREATE_THREAD_NOTIFY_ROUTINE)
static void NTAPI
ThreadFilterThreadExecuteRoutineCallback(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
) noexcept(true);

#endif  // DOXYGEN_SHOULD_SKIP_THIS

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Helper methods private to this module only.                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 *  @brief      The callback routine notified when a thread is created or exits.
 *              When the *Ex variant is used, this is registered for PsCreateThreadNotifySubsystems.
 *
 *  @param[in]  ProcessId             - The process ID of the process in which the thread action happens.
 *
 *  @param[in]  ThreadId              - The thread ID of the thread.
 *
 *  @param[in]  Create                - Indicates whether the thread was created (TRUE) or deleted (FALSE).
 *
 * @return      void.
 *
 * @note        https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pcreate_thread_notify_routine
 */
_Use_decl_annotations_
static void NTAPI
ThreadFilterThreadNotifyRoutineCallback(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pcreate_thread_notify_routine
    // The routine can be called at APC LEVEL AS WELL
    //
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::UniquePointer<xpf::IEvent> broadcastEvent{ SYSMON_PAGED_ALLOCATOR };

    HANDLE currentProcessPid = ::PsGetCurrentProcessId();
    HANDLE currentThreadTid = ::PsGetCurrentThreadId();

    //
    // Until all notifications are registered, we block new routines here.
    //
    while (!GlobalDataIsFilteringRegistrationFinished())
    {
        xpf::ApiSleep(100);
    }

    if (FALSE != Create)    /* Thread creation. */
    {
        SysMonLogTrace("Thread with tid %d is created in process with pid %d. "
                       "Current process pid %d current thread tid %d ",
                       HandleToUlong(ThreadId),
                       HandleToUlong(ProcessId),
                       HandleToUlong(currentProcessPid),
                       HandleToUlong(currentThreadTid));
        //
        // Now prepare the thread create event.
        //
        status = SysMon::ThreadCreateEvent::Create(broadcastEvent,
                                                   HandleToUlong(ProcessId),
                                                   HandleToUlong(ThreadId));
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("SysMon::ThreadCreateEvent::Create failed with status %!STATUS!",
                             status);
            return;
        }
    }
    else  /* Thread deletion. */
    {
        SysMonLogTrace("Thread with tid %d is terminated in process with pid %d. "
                       "Current process pid %d current thread tid %d ",
                       HandleToUlong(ThreadId),
                       HandleToUlong(ProcessId),
                       HandleToUlong(currentProcessPid),
                       HandleToUlong(currentThreadTid));
        //
        // Now prepare the thread terminate event.
        //
        status = SysMon::ThreadTerminateEvent::Create(broadcastEvent,
                                                      HandleToUlong(ProcessId),
                                                      HandleToUlong(ThreadId));
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("SysMon::ThreadTerminateEvent::Create failed with status %!STATUS!",
                             status);
            return;
        }
    }

    //
    // Dispatch synchronously (either create, either terminate).
    //
    status = GlobalDataGetBusInstance()->Dispatch(broadcastEvent.Get());
    if (!NT_SUCCESS(status))
    {
        SysMonLogWarning("Dispatch failed with status %!STATUS!",
                         status);
        return;
    }
}

/**
 *  @brief      The callback routine notified when a thread is created or exits.
 *              When the *Ex variant is used, this is registered for PsCreateThreadNotifyNonSystem.
 *
 *  @param[in]  ProcessId             - The process ID of the process in which the thread action happens.
 *
 *  @param[in]  ThreadId              - The thread ID of the thread.
 *
 *  @param[in]  Create                - Indicates whether the thread was created (TRUE) or deleted (FALSE).
 *
 *  @return     void.
 *
 *  @note       https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ne-ntddk-_pscreatethreadnotifytype
 */
_Use_decl_annotations_
static void NTAPI
ThreadFilterThreadExecuteRoutineCallback(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pcreate_thread_notify_routine
    // The routine can be called at APC LEVEL AS WELL
    //
    XPF_MAX_APC_LEVEL();

    HANDLE currentProcessPid = ::PsGetCurrentProcessId();
    HANDLE currentThreadTid = ::PsGetCurrentThreadId();

    //
    // Until all notifications are registered, we block new routines here.
    //
    while (!GlobalDataIsFilteringRegistrationFinished())
    {
        xpf::ApiSleep(100);
    }

    //
    // For now we just log a message here - not really setting context or doing any work.
    // This might prove useful in future, so keep the registration, and log the parameters.
    //

    SysMonLogTrace("Thread execute routine callback called for pid %d tid %d with create %!bool!. "
                   "Current process pid %d current thread tid %d",
                   HandleToUlong(ProcessId),
                   HandleToUlong(ThreadId),
                   (FALSE == Create) ? false : true,
                   HandleToUlong(currentProcessPid),
                   HandleToUlong(currentThreadTid));
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       ThreadFilterStart                                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ThreadFilterStart(
    void
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreatethreadnotifyroutine
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Registering thread notification routine...");

    //
    // First we check if we can use the newer method.
    //
    gApiPsSetCreateThreadNotifyRoutineEx = static_cast<PFUNC_PsSetCreateThreadNotifyRoutineEx>(
                                           KmHelper::WrapperMmGetSystemRoutine(L"PsSetCreateThreadNotifyRoutineEx"));
    if (gApiPsSetCreateThreadNotifyRoutineEx)
    {
        //
        // Here we register two callbacks:
        //  * PsCreateThreadNotifyNonSystem: The driver-registered callback function is executed on the new non-system thread,
        //                                   which enables the callback function to perform tasks such as setting
        //                                   the initial thread context.
        //
        // * PsCreateThreadNotifySubsystems: Indicates that the driver-registered callback function is invoked for threads
        //                                   of all subsystems. Drivers can call NtQueryInformationThread to determine
        //                                   the underlying subsystem.
        //
        SysMonLogInfo("PsSetCreateThreadNotifyRoutineEx found at %p.",
                      gApiPsSetCreateThreadNotifyRoutineEx);

        //
        // Register the first one PsCreateThreadNotifyNonSystem - this will be ThreadFilterThreadExecuteRoutineCallback.
        //
        status = gApiPsSetCreateThreadNotifyRoutineEx(ENUM_PSCREATETHREADNOTIFYTYPE::PsCreateThreadNotifyNonSystem,
                                                      ThreadFilterThreadExecuteRoutineCallback);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("Registering thread notification routine routine for "
                           "PsCreateThreadNotifyNonSystem failed with status = %!STATUS!",
                           status);
            return status;
        }

        //
        // Register the second one PsCreateThreadNotifySubsystems - this will be ThreadFilterThreadNotifyRoutineCallback.
        //
        status = gApiPsSetCreateThreadNotifyRoutineEx(ENUM_PSCREATETHREADNOTIFYTYPE::PsCreateThreadNotifySubsystems,
                                                      ThreadFilterThreadNotifyRoutineCallback);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("Registering thread notification routine routine for "
                           "PsCreateThreadNotifySubsystems failed with status = %!STATUS!",
                           status);
            //
            // Be a good citizen and remove the previously registered callback.
            //
            NTSTATUS removeStatus = ::PsRemoveCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)ThreadFilterThreadExecuteRoutineCallback);   // NOLINT(*)
            if (!NT_SUCCESS(removeStatus))
            {
                XPF_ASSERT(FALSE);
                SysMonLogCritical("Failed to unregister the already registered callback with status = %!STATUS!",
                                  status);
            }

            return status;
        }
    }
    else
    {
        SysMonLogInfo("PsSetCreateThreadNotifyRoutineEx not found! Will use the older variant.");

        status = ::PsSetCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)ThreadFilterThreadNotifyRoutineCallback);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("Registering notification routine failed with status = %!STATUS!",
                           status);
            return status;
        }
    }

    //
    // All good.
    //
    SysMonLogInfo("Successfully registered thread notification routine!");
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       ThreadFilterStop                                                          |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(APC_LEVEL)
void XPF_API
ThreadFilterStop(
    void
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-psremovecreatethreadnotifyroutine
    // The routine can be called at max APC
    //
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Unregistering thread notification routine...");

    //
    // We don't expect a failure. So assert here and investigate what happened.
    //
    if (gApiPsSetCreateThreadNotifyRoutineEx)
    {
        SysMonLogInfo("PsSetCreateThreadNotifyRoutineEx found at %p.",
                      gApiPsSetCreateThreadNotifyRoutineEx);

        status = ::PsRemoveCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)ThreadFilterThreadNotifyRoutineCallback);
        if (!NT_SUCCESS(status))
        {
            XPF_ASSERT(false);
            SysMonLogCritical("Unregistering notification routine failed with status = %!STATUS!",
                              status);
        }
        status = ::PsRemoveCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)ThreadFilterThreadExecuteRoutineCallback);
        if (!NT_SUCCESS(status))
        {
            XPF_ASSERT(false);
            SysMonLogCritical("Unregistering notification routine failed with status = %!STATUS!",
                              status);
        }

        gApiPsSetCreateThreadNotifyRoutineEx = nullptr;
    }
    else
    {
        SysMonLogInfo("PsSetCreateThreadNotifyRoutineEx not found! Will use the older variant.");

        status = ::PsRemoveCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)ThreadFilterThreadNotifyRoutineCallback);
        if (!NT_SUCCESS(status))
        {
            XPF_ASSERT(false);
            SysMonLogCritical("Unregistering notification routine failed with status = %!STATUS!",
                              status);
        }
    }

    SysMonLogInfo("Unregistered thread notification routine!");
}
