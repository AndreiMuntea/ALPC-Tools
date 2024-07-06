/**
 * @file        ALPC-Tools/AlpcMon_Sys/ProcessFilter.cpp
 *
 * @brief       In this file we define the functionality related to
 *              process filtering.
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

#include "ProcessCollector.hpp"

#include "ProcessFilter.hpp"
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
 * @details https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ne-ntddk-_pscreateprocessnotifytype
 */
typedef enum _ENUM_PSCREATEPROCESSNOTIFYTYPE
{
    /**
     * @brief  From MSDN: "Indicates that the driver-registered callback is invoked for processes of all subsystems,
     *         including the Win32 subsystem. Drivers can call NtQueryInformationProcess to determine
     *         the underlying subsystem. The query retrieves a SUBSYSTEM_INFORMATION_TYPE value."
     */
    ENUM_PsCreateProcessNotifySubsystems
} ENUM_PSCREATEPROCESSNOTIFYTYPE;

/**
 *  @brief      Supported from Windows 10, version 1703. The PsSetCreateProcessNotifyRoutineEx2 routine
 *              registers or removes a callback routine that notifies the caller when a process is created or deleted.
 *
 *  @param[in]  NotifyType          - Indicates the type of process notification.
 *
 *  @param[in]  NotifyInformation   - The address of the notification information for the specified type of process notification.
 *
 *  @param[in]  Remove              - A Boolean value that specifies whether PsSetCreateProcessNotifyRoutineEx2 will add or
 *                                    remove a specified routine from the list of callback routines. If this parameter is TRUE,
 *                                    the specified routine is removed from the list of callback routines. If this parameter
 *                                    is FALSE, the specified routine is added to the list of callback routines. If Remove is TRUE,
 *                                    the system also waits for all in-flight callback routines to complete before returning.
 *
 * @return      An ntstatus error code.
 *
 * @note        https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutineex2
 */
typedef NTSTATUS (NTAPI *PFUNC_PsSetCreateProcessNotifyRoutineEx2)(
    _In_ ENUM_PSCREATEPROCESSNOTIFYTYPE NotifyType,
    _In_ PVOID NotifyInformation,
    _In_ BOOLEAN Remove
) noexcept(true);

/**
 * @brief   A global variable that will store the address of PsSetCreateProcessNotifyRoutineEx2
 *          or NULL if we fail to find it. The resolution is attempted during ProcessFilterStart.
 *          After we unregister the routine (ProcessFilterStop), this is set back to NULL.
 */
static volatile PFUNC_PsSetCreateProcessNotifyRoutineEx2 gApiPsSetCreateProcessNotifyRoutineEx2 = nullptr;

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Forward definition of methods private to this module only.                         |
/// |                              Skipped by doxygen as they are annotated below.                                    |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

#ifndef DOXYGEN_SHOULD_SKIP_THIS

_Function_class_(PCREATE_PROCESS_NOTIFY_ROUTINE_EX)
static void NTAPI
ProcessFilterProcessNotifyRoutineCallback(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
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
 *  @brief      The callback routine notified when a process is created or exits.
 *
 *  @param[in]  Process             - A pointer to the EPROCESS structure that represents the process.
 *
 *  @param[in]  ProcessId           - The process ID of the process.
 *
 *  @param[in]  CreateInfo          - A pointer to a PS_CREATE_NOTIFY_INFO structure that contains information
 *                                    about the new process. If this parameter is NULL, the specified process is exiting.
 *
 * @return      void.
 *
 * @note        https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pcreate_process_notify_routine_ex
 */
_Use_decl_annotations_
static void NTAPI
ProcessFilterProcessNotifyRoutineCallback(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pcreate_process_notify_routine_ex
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(NULL != Process);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::StringView<wchar_t> processPath;
    xpf::UniquePointer<xpf::IEvent> broadcastEvent{ SYSMON_PAGED_ALLOCATOR };
    SysMon::ProcessArchitecture architecture = SysMon::ProcessArchitecture::MAX;

    //
    // Until all notifications are registered, we block new routines here.
    //
    while (!GlobalDataIsFilteringRegistrationFinished())
    {
        xpf::ApiSleep(100);
    }

    //
    // Grab the architecture.
    //
    if constexpr (SysMon::CurrentOsArchitecture() == SysMon::OsArchitecture::ix86)
    {
        architecture = SysMon::ProcessArchitecture::x86;
    }
    else if constexpr (SysMon::CurrentOsArchitecture() == SysMon::OsArchitecture::amd64)
    {
        architecture = KmHelper::WrapperIsWow64Process(Process) ? SysMon::ProcessArchitecture::WoWX86onX64
                                                                : SysMon::ProcessArchitecture::x64;
    }

    if (NULL != CreateInfo)     /* Process Creation Scenario */
    {
        //
        // Log for tracing.
        //
        SysMonLogInfo("Process with pid %d and eprocess %p is being created."
                      "FileName = %wZ. Parent pid = %d. Architecture = %d",
                      HandleToUlong(ProcessId),
                      Process,
                      CreateInfo->ImageFileName,
                      HandleToUlong(CreateInfo->ParentProcessId),
                      static_cast<uint32_t>(architecture));
        //
        // Now prepare the process create event.
        //
        status = KmHelper::HelperUnicodeStringToView(*CreateInfo->ImageFileName,
                                                     processPath);
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("HelperUnicodeStringToView failed with status %!STATUS!",
                             status);
            return;
        }

        //
        // Add item to collector for caching, done before dispatching the event,
        // as listener might query some extra dara about the process.
        // So we need to have it available.
        //
        // Q - should process collector register to process create event instead?
        //     this way we'll decouple the filter from the collector.
        //
        ProcessCollectorHandleCreateProcess(HandleToUlong(ProcessId),
                                            processPath);

        //
        // And dispatch event.
        //
        status = SysMon::ProcessCreateEvent::Create(broadcastEvent,
                                                    HandleToUlong(ProcessId),
                                                    architecture,
                                                    processPath);
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("SysMon::ProcessCreateEvent::Create failed with status %!STATUS!",
                             status);
            return;
        }
        status = GlobalDataGetBusInstance()->Dispatch(broadcastEvent.Get());
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("Dispatch failed with status %!STATUS!",
                             status);
            return;
        }
    }
    else  /* Process Termination Scenario */
    {
        //
        // Log for tracing.
        //
        SysMonLogInfo("Process with pid %d and eprocess %p is being terminated.",
                      HandleToUlong(ProcessId),
                      Process);

        //
        // Prepare the process terminate event.
        //
        status = SysMon::ProcessTerminateEvent::Create(broadcastEvent,
                                                       HandleToUlong(ProcessId));
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("SysMon::ProcessTerminateEvent::Create failed with status %!STATUS!",
                             status);
            return;
        }
        status = GlobalDataGetBusInstance()->Dispatch(broadcastEvent.Get());
        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("Dispatch failed with status %!STATUS!",
                             status);
            return;
        }

        //
        // Remove the process from collector - done after dispatching terminate,
        // as listeners might want to query extra data on the event.
        //
        // Q - should process collector register to process terminate event instead?
        //     this way we'll decouple the filter from the collector.
        //
        ProcessCollectorHandleTerminateProcess(HandleToULong(ProcessId));
    }
}

/**
 * @brief       Given an opend process handle, it retrieves the image path.
 *
 * @param[in]   ProcessHandle - handle to the opened process.
 * @param[out]  ImagePath     - the path of the image.
 *
 * @return      A proper NTSTATUS error code.
 */
static NTSTATUS XPF_API
ProcessFilterGetProcessImagePath(
    _In_ HANDLE ProcessHandle,
    _Out_ xpf::String<wchar_t>* ImagePath
) noexcept(true)
{
    /* The routine can be called only at max PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    /* Preinit output. */
    ImagePath->Reset();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG informationLength = 0;

    xpf::Buffer processImageBuffer{ ImagePath->GetAllocator() };
    xpf::StringView<wchar_t> processImageView;

    /* We may need to do this in a loop as we don't know how much memory to preallocate. */
    for (size_t i = 1; i <= 100; ++i)
    {
        status = processImageBuffer.Resize(i * PAGE_SIZE);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        status = ::ZwQueryInformationProcess(ProcessHandle,
                                             PROCESSINFOCLASS::ProcessImageFileName,
                                             processImageBuffer.GetBuffer(),
                                             static_cast<ULONG>(processImageBuffer.GetSize()),
                                             &informationLength);
        if (NT_SUCCESS(status))
        {
            /* Grabbed the image name. */
            break;
        }
    }
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Grab a friendlier view. */
    status = KmHelper::HelperUnicodeStringToView(*static_cast<UNICODE_STRING*>(processImageBuffer.GetBuffer()),
                                                 processImageView);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* And send it to output. */
    return ImagePath->Append(processImageView);
}

/**
 * @brief       Given an opend process object, it retrieves the image name,
 *              as stored in the EPROCESS.
 *
 * @param[in]   Process       - The opened eprocess
 * @param[out]  ImageName     - the image name.
 *
 * @return      A proper NTSTATUS error code.
 */
static NTSTATUS XPF_API
ProcessFilterGetProcessImageName(
    _In_ PEPROCESS Process,
    _Out_ xpf::String<wchar_t>* ImageName
) noexcept(true)
{
    /* The routine can be called only at max PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    /* Preinit output. */
    ImageName->Reset();

    /* Grab the unicode variant of the image name. */
    return xpf::StringConversion::UTF8ToWide(::PsGetProcessImageFileName(Process),
                                             *ImageName);
}

/**
 * @brief   This routine is used to gather information about the already running processes.
 *          We will block the creation of new ones until this is finished.
 *          This must be called after registering the notification.
 */
static void XPF_API
ProcessFilterGatherPreexistingProcesses(
    void
) noexcept(true)
{
    /* The routine can be called only at max PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Buffer processBuffer{ SYSMON_PAGED_ALLOCATOR };
    XPF_SYSTEM_PROCESS_INFORMATION* processInformation = nullptr;

    /* We may need to do this in a loop as we don't know how much memory to preallocate. */
    for (size_t i = 1; i <= 100; ++i)
    {
        status = processBuffer.Resize(i * PAGE_SIZE);
        if (!NT_SUCCESS(status))
        {
            return;
        }

        ULONG informationLength = 0;
        status = ::ZwQuerySystemInformation(XPF_SYSTEM_INFORMATION_CLASS::XpfSystemProcessInformation,
                                            processBuffer.GetBuffer(),
                                            static_cast<ULONG>(processBuffer.GetSize()),
                                            &informationLength);
        if (NT_SUCCESS(status))
        {
            /* Snapshotted the processes. */
            break;
        }
    }

    /* Could not snapshot the processes. */
    if (!NT_SUCCESS(status))
    {
        return;
    }

    /* Success */
    processInformation = static_cast<XPF_SYSTEM_PROCESS_INFORMATION*>(processBuffer.GetBuffer());


    /* Walk all processes and best effort cache them. */
    while(processInformation != nullptr)
    {
        xpf::String<wchar_t> processPath{ SYSMON_PAGED_ALLOCATOR };
        PEPROCESS processObject = nullptr;
        HANDLE processHandle = nullptr;

        /* We're entering a new process loop. */
        status = STATUS_SUCCESS;

        /* If the process is the idle process we handle it separately. */
        /* This process can't be opened like a normal process. */
        if (processInformation->UniqueProcessId == 0)
        {
            ProcessCollectorHandleCreateProcess(HandleToULong(processInformation->UniqueProcessId),
                                                L"idle");
            goto ContinueLoop;
        }

        /* Find the associated eprocess. */
        status = ::PsLookupProcessByProcessId(processInformation->UniqueProcessId,
                                              &processObject);
        if (!NT_SUCCESS(status))
        {
            goto ContinueLoop;
        }

        /* Grab a handle to the eprocess. */
        status = ::ObOpenObjectByPointer(processObject,
                                         OBJ_KERNEL_HANDLE,
                                         NULL,
                                         PROCESS_ALL_ACCESS,
                                         *PsProcessType,
                                         KernelMode,
                                         &processHandle);
        if (!NT_SUCCESS(status))
        {
            goto ContinueLoop;
        }

        /* Grab the image path. */
        status = ProcessFilterGetProcessImagePath(processHandle,
                                                  &processPath);
        if (NT_SUCCESS(status))
        {
            /* If the path is empty, we'll only grab the name - can happen for registry or other system processes. */
            if (processPath.IsEmpty())
            {
                status = ProcessFilterGetProcessImageName(processObject,
                                                          &processPath);
            }
        }
        if (!NT_SUCCESS(status))
        {
            goto ContinueLoop;
        }

        /* Add it to collector. */
        ProcessCollectorHandleCreateProcess(HandleToULong(processInformation->UniqueProcessId),
                                            processPath.View());

    ContinueLoop:
        if (processHandle != nullptr)
        {
            NTSTATUS closeStatus = ::ObCloseHandle(processHandle,
                                                   KernelMode);
            processHandle = nullptr;
            XPF_DEATH_ON_FAILURE(NT_SUCCESS(closeStatus));
        }
        if (processObject != nullptr)
        {
            ::ObDereferenceObjectDeferDelete(processObject);
            processObject = nullptr;
        }

        if (!NT_SUCCESS(status))
        {
            SysMonLogWarning("Failed to gather information about the process with pid %d (%wZ) %!STATUS!",
                             HandleToULong(processInformation->UniqueProcessId),
                             &processInformation->ImageName,
                             status);
        }

        void* next = (processInformation->NextEntryOffset != 0) ? xpf::AlgoAddToPointer(processInformation,
                                                                                        processInformation->NextEntryOffset)
                                                                : nullptr;
        processInformation = static_cast<XPF_SYSTEM_PROCESS_INFORMATION*>(next);
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       ProcessFilterStart                                                        |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ProcessFilterStart(
    void
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutine
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Registering process notification routine...");

    //
    // First we check if we can use the newer method.
    //
    gApiPsSetCreateProcessNotifyRoutineEx2 = static_cast<PFUNC_PsSetCreateProcessNotifyRoutineEx2>(
                                             KmHelper::WrapperMmGetSystemRoutine(L"PsSetCreateProcessNotifyRoutineEx2"));
    if (gApiPsSetCreateProcessNotifyRoutineEx2)
    {
        SysMonLogInfo("PsSetCreateProcessNotifyRoutineEx2 found at %p.",
                      gApiPsSetCreateProcessNotifyRoutineEx2);

        status = gApiPsSetCreateProcessNotifyRoutineEx2(ENUM_PSCREATEPROCESSNOTIFYTYPE::ENUM_PsCreateProcessNotifySubsystems,
                                                        ProcessFilterProcessNotifyRoutineCallback,
                                                        FALSE);
    }
    else
    {
        SysMonLogInfo("PsSetCreateProcessNotifyRoutineEx2 not found! Will use the older variant.");

        status = ::PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)ProcessFilterProcessNotifyRoutineCallback,
                                                     FALSE);
    }

    //
    // Let's inspect the result.
    //
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Registering notification routine failed with status = %!STATUS!",
                       status);
        return status;
    }

    //
    // Now gather information about the preexisting processes.
    // We do it inline with the registration, to prevent new notifications from happening.
    //
    ProcessFilterGatherPreexistingProcesses();

    //
    // All good.
    //
    SysMonLogInfo("Successfully registered process notification routine!");
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       ProcessFilterStop                                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessFilterStop(
    void
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutine
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Unregistering process notification routine...");

    if (gApiPsSetCreateProcessNotifyRoutineEx2)
    {
        SysMonLogInfo("PsSetCreateProcessNotifyRoutineEx2 found at %p.",
                      gApiPsSetCreateProcessNotifyRoutineEx2);

        status = gApiPsSetCreateProcessNotifyRoutineEx2(ENUM_PSCREATEPROCESSNOTIFYTYPE::ENUM_PsCreateProcessNotifySubsystems,
                                                        ProcessFilterProcessNotifyRoutineCallback,
                                                        TRUE);
        gApiPsSetCreateProcessNotifyRoutineEx2 = nullptr;
    }
    else
    {
        SysMonLogInfo("PsSetCreateProcessNotifyRoutineEx2 not found! Will use the older variant.");

        status = ::PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)ProcessFilterProcessNotifyRoutineCallback,
                                                     TRUE);
    }

    //
    // We don't expect a failure. So assert here and investigate what happened.
    //
    if (!NT_SUCCESS(status))
    {
        XPF_ASSERT(false);

        SysMonLogCritical("Unregistering notification routine failed with status = %!STATUS!",
                          status);
        return;
    }

    //
    // All good.
    //
    SysMonLogInfo("Successfully unregistered process notification routine!");
}
