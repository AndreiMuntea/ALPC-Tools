/**
 * @file        ALPC-Tools/AlpcMon_Sys/PluginManager.cpp
 *
 * @brief       This is the plugin responsible with UM Hooking component.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "Events.hpp"
#include "globals.hpp"
#include "KmHelper.hpp"

#include "UmHookPlugin.hpp"
#include "trace.hpp"


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       PAGED SECTION AREA                                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


 /**
  * @brief   Put everything below here in paged section.
  */
XPF_SECTION_PAGED;

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       DEFINITION & GLOBAL DATA                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

#define UM_INJECTION_DATA_SYSTEM32_NTDLL_FLAG               0x00000001
#define UM_INJECTION_DATA_SYSWOW64_NTDLL_FLAG               0x00000002
#define UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG            0x00000004
#define UM_INJECTION_DATA_SYSWOW64_KERNEL32_FLAG            0x00000008
#define UM_INJECTION_DATA_SYSTEM32_USER32_FLAG              0x00000010
#define UM_INJECTION_DATA_SYSWOW64_USER32_FLAG              0x00000020
#define UM_INJECTION_DATA_SYSTEM32_WOW64_FLAG               0x00000040
#define UM_INJECTION_DATA_SYSTEM32_WOW64WIN_FLAG            0x00000080
#define UM_INJECTION_DATA_SYSTEM32_WOW64CPU_FLAG            0x00000100

/**
 * @brief   Structure to help us map the dll path to the flag.
 */
struct UmInjectionMetadata
{
    /**
     * @brief   The end component path of the dll.
     */
    const xpf::StringView<wchar_t> DllPath;

    /**
     * @brief   The mapping to its flag (one of the UM_INJECTION_DATA_*)
     */
    uint32_t DllFlag;
};

/**
 * @brief   Actual mapping between path and flags.
 */
static const constexpr UmInjectionMetadata UM_INJECTION_DLL_PATH_FLAGS[] =
{
    { L"\\System32\\ntdll.dll",         UM_INJECTION_DATA_SYSTEM32_NTDLL_FLAG },
    { L"\\SysWow64\\ntdll.dll",         UM_INJECTION_DATA_SYSWOW64_NTDLL_FLAG },

    { L"\\System32\\kernel32.dll",      UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG },
    { L"\\SysWow64\\kernel32.dll",      UM_INJECTION_DATA_SYSWOW64_KERNEL32_FLAG },

    { L"\\System32\\user32.dll",        UM_INJECTION_DATA_SYSTEM32_USER32_FLAG },
    { L"\\SysWow64\\user32.dll",        UM_INJECTION_DATA_SYSWOW64_USER32_FLAG },

    { L"\\System32\\WoW64.dll",         UM_INJECTION_DATA_SYSTEM32_WOW64_FLAG },
    { L"\\System32\\WoW64win.dll",      UM_INJECTION_DATA_SYSTEM32_WOW64WIN_FLAG },
    { L"\\System32\\WoW64cpu.dll",      UM_INJECTION_DATA_SYSTEM32_WOW64CPU_FLAG },
};

/**
 * @brief   The name of the injection dll for Win32.
 */
static constexpr xpf::StringView<wchar_t> gUmDllWin32Path   = L"AlpcMon_DllWin32.dll";

/**
 * @brief   The name of the injection dll for x64.
 */
static constexpr xpf::StringView<wchar_t> gUmDllx64Path     = L"AlpcMon_Dllx64.dll";

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Convenience wrappers over APC api                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void NTAPI
WrapperUmHookPluginQueueApcKernelRoutine(
    _In_ PKAPC Apc,
    _Inout_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ PVOID* NormalContext,
    _Inout_ PVOID* SystemArgument1,
    _Inout_ PVOID* SystemArgument2
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    XPF_UNREFERENCED_PARAMETER(NormalRoutine);
    XPF_UNREFERENCED_PARAMETER(NormalContext);
    XPF_UNREFERENCED_PARAMETER(SystemArgument1);
    XPF_UNREFERENCED_PARAMETER(SystemArgument2);

    SysMonLogTrace("WrapperUmHookPluginQueueApcKernelRoutine called for apc %p",
                   Apc);

    xpf::CriticalMemoryAllocator::FreeMemory(Apc);
}

static void NTAPI
WrapperUmHookPluginQueueApcRundownRoutine(
    _In_ PKAPC Apc
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    SysMonLogTrace("WrapperUmHookPluginQueueApcRundownRoutine called for apc %p",
                   Apc);
}

_Must_inspect_result_
static NTSTATUS NTAPI
WrapperUmHookPluginQueueApc(
    _In_opt_ PKNORMAL_ROUTINE NormalRoutine,
    _In_ KPROCESSOR_MODE Mode,
    _In_opt_ PVOID NormalContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    BOOLEAN insertedApc = FALSE;
    KAPC* kApc = nullptr;

    //
    // If the thread is terminating, we bail.
    //
    if (FALSE != ::PsIsThreadTerminating(::PsGetCurrentThread()))
    {
        return STATUS_TOO_LATE;
    }

    //
    // On x64, we need to encode the routine when injecting in wow processes.
    //
    if constexpr (SysMon::CurrentOsArchitecture() == SysMon::OsArchitecture::amd64)
    {
        if (Mode == UserMode && KmHelper::WrapperIsWow64Process(PsGetCurrentProcess()))
        {
            NTSTATUS status = ::PsWrapApcWow64Thread(&NormalContext,
                                                     reinterpret_cast<PVOID*>(&NormalRoutine));
            if (!NT_SUCCESS(status))
            {
                return status;
            }
        }
    }

    //
    // First allocate an APC object.
    //
    kApc = static_cast<KAPC*>(xpf::CriticalMemoryAllocator::AllocateMemory(sizeof(KAPC)));
    if (nullptr == kApc)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    xpf::ApiZeroMemory(kApc, sizeof(KAPC));

    //
    // Now properly initialize it.
    //
    ::KeInitializeApc(kApc,
                      ::PsGetCurrentThread(),
                      KAPC_ENVIRONMENT::OriginalApcEnvironment,
                      WrapperUmHookPluginQueueApcKernelRoutine,
                      WrapperUmHookPluginQueueApcRundownRoutine,
                      NormalRoutine,
                      Mode,
                      NormalContext);
    //
    // Insert it.
    //
    insertedApc = ::KeInsertQueueApc(kApc,
                                     SystemArgument1,
                                     SystemArgument2,
                                     IO_NO_INCREMENT);
    if (!insertedApc)
    {
        xpf::CriticalMemoryAllocator::FreeMemory(&kApc);
        return STATUS_INVALID_STATE_TRANSITION;
    }
    return STATUS_SUCCESS;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Convenience helpers for injection                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

//
// The flow is as follows:
//  1. Plugin calls HelperUmHookPluginInject from the load image notify routine.
//  2. A kernel APC is scheduled, as in our injection process we are creatin a new section,
//     we risk of entering a deadlock with the previous load image notify routine.
//     The APC is HelperUmHookPluginApcNormalRoutine.
//  3. In here we are creating a section where we are copying the payload (dll name).
//     And we are scheduling the user APC responsible with loading our dll.
//

_Must_inspect_result_
static NTSTATUS NTAPI
HelperUmHookPluginMapSectionAndInject(
    _In_ _Const_ const SysMon::UmInjectionDllData& InjectionData
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    HANDLE sectionHandle = NULL;
    OBJECT_ATTRIBUTES objectAttributes = { 0 };

    SIZE_T sectionSize = 0;
    LARGE_INTEGER maximumSize = { 0 };
    PVOID baseAddress = nullptr;

    PKNORMAL_ROUTINE apcRoutine = nullptr;
    PVOID apcContext = nullptr;
    UNICODE_STRING dllPath = { 0 };

    SysMonLogInfo("Enqueing injection APC in process %d...",
                   InjectionData.ProcessId);
    //
    // Convert the view to unicode string.
    //
    status = KmHelper::HelperViewToUnicodeString(InjectionData.InjectedDllPath,
                                                 dllPath);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperViewToUnicodeString failed with status = %!STATUS!",
                       status);
        goto CleanUp;
    }

    //
    // We only want to write the path.
    //
    sectionSize = static_cast<size_t>(dllPath.Length) + sizeof(L'\0');
    maximumSize.QuadPart = sectionSize;

    //
    // We need to write dll path into the target process.
    // ZwAllocateVirtualMemory is not exposed when targeting windows 7.
    // So we create a section.
    //
    InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
    status = ::ZwCreateSection(&sectionHandle,
                               GENERIC_READ | GENERIC_WRITE,
                               &objectAttributes,
                               &maximumSize,
                               PAGE_READWRITE,
                               SEC_COMMIT,
                               NULL);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("ZwCreateSection failed with status = %!STATUS!",
                       status);

        sectionHandle = NULL;
        goto CleanUp;
    }

    //
    // Map the view as readwrite.
    //
    status = ::ZwMapViewOfSection(sectionHandle,
                                  ZwCurrentProcess(),
                                  &baseAddress,
                                  0,
                                  sectionSize,
                                  NULL,
                                  &sectionSize,
                                  SECTION_INHERIT::ViewUnmap,
                                  0,
                                  PAGE_READWRITE);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("ZwMapViewOfSection failed with status = %!STATUS!",
                       status);

        baseAddress = NULL;
        goto CleanUp;
    }

    //
    // Copy the path.
    //
    {
        const wchar_t nullTerminator = L'\0';

        status = KmHelper::HelperSafeWriteBuffer(static_cast<uint8_t*>(baseAddress),
                                                 dllPath.Buffer,
                                                 dllPath.Length);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperSafeWriteBuffer failed with status = %!STATUS!",
                            status);
            goto CleanUp;
        }
        status = KmHelper::HelperSafeWriteBuffer(static_cast<uint8_t*>(baseAddress) + dllPath.Length,
                                                 &nullTerminator,
                                                 sizeof(nullTerminator));
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperSafeWriteBuffer failed with status = %!STATUS!",
                            status);
            goto CleanUp;
        }
    }

    //
    // And schedule the APC. On x64 wow scenario we need to encode the parameters.
    // PsWrapApcWow64Thread will change to wow64.dll!Wow64ApcRoutine
    //
    // HMODULE LoadLibraryExW(
    //   [in] LPCWSTR lpLibFileName,
    //   [in, opt] HANDLE  hFile,
    //   [in] DWORD dwFlags
    // );
    //
    apcRoutine = static_cast<PKNORMAL_ROUTINE>(InjectionData.LoadDllRoutine);
    apcContext = baseAddress;

    if (nullptr == apcRoutine || nullptr == apcContext)
    {
        SysMonLogError("Invalid arguments for APC: routine(%p) context(%p)",
                       apcRoutine,
                       apcContext);

        status = STATUS_INVALID_PARAMETER;
        goto CleanUp;
    }

    status = WrapperUmHookPluginQueueApc(apcRoutine,
                                         UserMode,
                                         apcContext,
                                         NULL,
                                         0);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("WrapperUmHookPluginQueueApc failed with status = %!STATUS!",
                       status);
        goto CleanUp;
    }

    //
    // All good.
    //
    SysMonLogInfo("Successfully enqueued the injection APC in process %d section %p",
                   InjectionData.ProcessId,
                   baseAddress);
    status = STATUS_SUCCESS;

CleanUp:
    if (!NT_SUCCESS(status) && baseAddress != NULL)
    {
        NTSTATUS unmapStatus = ::ZwUnmapViewOfSection(ZwCurrentProcess(),
                                                      baseAddress);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(unmapStatus));

        baseAddress = NULL;
    }
    if (NULL != sectionHandle)
    {
        NTSTATUS closeStatus = ::ZwClose(sectionHandle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(closeStatus));

        sectionHandle = NULL;
    }
    return status;
}

static void NTAPI
HelperUmHookPluginMapSectionApc(
  _In_ PVOID NormalContext,
  _In_ PVOID SystemArgument1,
  _In_ PVOID SystemArgument2
)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SysMon::UmInjectionDllData* data = nullptr;

    XPF_UNREFERENCED_PARAMETER(SystemArgument1);
    XPF_UNREFERENCED_PARAMETER(SystemArgument2);
    XPF_DEATH_ON_FAILURE(nullptr != NormalContext);

    data = static_cast<SysMon::UmInjectionDllData*>(NormalContext);

    SysMonLogInfo("Executing the map section APC. Preparing to do the actual injection in process %d.",
                  data->ProcessId);

    //
    // Do the actual injection.
    //
    status = HelperUmHookPluginMapSectionAndInject(*data);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperUmHookPluginMapSectionAndInject failed with status = %!STATUS!",
                       status);
    }

    SysMonLogInfo("Finished executing map section apc for process %d",
                  data->ProcessId);
    //
    // Free resources.
    //
    xpf::MemoryAllocator::Destruct(data);
    xpf::CriticalMemoryAllocator::FreeMemory(data);
}

static void NTAPI
HelperUmHookPluginInject(
    _In_ _Const_ const SysMon::UmInjectionDllData& InjectionData
) noexcept(true)
{
    //
    // As we are on the load image flow, we can't do a ZwCreateSection.
    // So we instead will deffer the work here.
    //

    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SysMon::UmInjectionDllData* copy = nullptr;

    SysMonLogInfo("Enqueing map section APC in process %d...",
                   InjectionData.ProcessId);

    //
    // Sanity check that we are in the good context.
    //
    if (::PsGetCurrentProcessId() != ULongToHandle(InjectionData.ProcessId))
    {
        XPF_ASSERT(FALSE);

        SysMonLogError("Can not enqueue an APC from a different process. Expepcted %d. Actual %d",
                       InjectionData.ProcessId,
                       HandleToUlong(::PsGetCurrentProcessId()));
        return;
    }

    //
    // We need a clone here so we don't force the UM plugin to keep a reference.
    //
    copy = static_cast<SysMon::UmInjectionDllData*>(
           xpf::CriticalMemoryAllocator::AllocateMemory(sizeof(SysMon::UmInjectionDllData)));
    if (nullptr == copy)
    {
        SysMonLogError("Could not clone UmInjectionDllData");
        return;
    }
    xpf::MemoryAllocator::Construct(copy);

    //
    // Fill the fields.
    //
    copy->ProcessId = InjectionData.ProcessId;
    copy->RequiredDlls = InjectionData.RequiredDlls;
    copy->LoadedDlls = InjectionData.LoadedDlls;
    copy->MatchingDll = InjectionData.MatchingDll;
    copy->LoadDllRoutine = InjectionData.LoadDllRoutine;
    copy->LoadDllRoutineName = InjectionData.LoadDllRoutineName;
    copy->InjectedDllPath = InjectionData.InjectedDllPath;

    //
    // Now queue an APC.
    //
    status = WrapperUmHookPluginQueueApc(HelperUmHookPluginMapSectionApc,
                                         KernelMode,
                                         copy,
                                         NULL,
                                         NULL);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("WrapperUmHookPluginQueueApc failed with status = %!STATUS!",
                       status);

        xpf::MemoryAllocator::Destruct(copy);
        xpf::CriticalMemoryAllocator::FreeMemory(copy);
    }
    else
    {
        SysMonLogInfo("Successfully enqueued map section APC in process %d...",
                      InjectionData.ProcessId);
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::Create                                              |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::UmHookPlugin::Create(
    _Inout_ xpf::SharedPointer<SysMon::IPlugin>& Plugin,
    _In_ const uint64_t& PluginId
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::SharedPointer<SysMon::UmHookPlugin> plugin{ SYSMON_NPAGED_ALLOCATOR };
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Creating UmHookPlugin...");

    //
    // First create the plugin.
    //
    plugin = xpf::MakeSharedWithAllocator<SysMon::UmHookPlugin>(SYSMON_NPAGED_ALLOCATOR,
                                                                PluginId);
    if (plugin.IsEmpty())
    {
        SysMonLogError("Insufficient resources to create the plugin");

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::UmHookPlugin& umHookPlugin = (*plugin);

    //
    // Create the lock.
    //
    status = xpf::ReadWriteLock::Create(&umHookPlugin.m_ProcessDataLock);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("xpf::ReadWriteLock::Create failed with status = %!STATUS!",
                       status);
        return status;
    }

    //
    // Now construct the win32 injection dll.
    //
    status = umHookPlugin.m_UmDllWin32Path.Append(GlobalDataGetDosInstallationDirectory());
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("m_UmDllWin32Path create failed with status = %!STATUS!",
                       status);
        return status;
    }
    status = umHookPlugin.m_UmDllWin32Path.Append(gUmDllWin32Path);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("m_UmDllWin32Path create failed with status = %!STATUS!",
                       status);
        return status;
    }
    SysMonLogInfo("Using win32 injection dll from path %S",
                  umHookPlugin.m_UmDllWin32Path.View().Buffer());

    //
    // Now construct the x64 injection dll.
    //
    status = umHookPlugin.m_UmDllX64Path.Append(GlobalDataGetDosInstallationDirectory());
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("m_UmDllX64Path create failed with status = %!STATUS!",
                       status);
        return status;
    }
    status = umHookPlugin.m_UmDllX64Path.Append(gUmDllx64Path);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("m_UmDllX64Path create failed with status = %!STATUS!",
                       status);
        return status;
    }
    SysMonLogInfo("Using x64 injection dll from path %S",
                  umHookPlugin.m_UmDllX64Path.View().Buffer());

    //
    // On windows 7 we have some extra dependencies on user32.dll.
    //
    const RTL_OSVERSIONINFOEXW* osVersion = GlobalDataGetOsVersion();
    if (osVersion->dwMajorVersion == 6 && osVersion->dwMinorVersion == 1)
    {
        umHookPlugin.m_IsWindows7 = true;
    }

    //
    // Cast it as IPlugin.
    //
    Plugin = xpf::DynamicSharedPointerCast<SysMon::IPlugin, SysMon::UmHookPlugin>(xpf::Move(plugin));
    if (Plugin.IsEmpty())
    {
        SysMonLogError("Insufficient resources to cast the plugin");

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // All good.
    //
    SysMonLogInfo("Created UmHookPlugin.");
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::OnEvent                                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::UmHookPlugin::OnEvent(
    _Inout_ xpf::IEvent* Event,
    _Inout_ xpf::EventBus* Bus
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    XPF_UNREFERENCED_PARAMETER(Bus);

    switch (Event->EventId())
    {
        case static_cast<xpf::EVENT_ID>(SysMon::EventId::ProcessCreate):
        {
            this->OnProcessCreateEvent(Event);
            break;
        }
        case static_cast<xpf::EVENT_ID>(SysMon::EventId::ProcessTerminate):
        {
            this->OnProcessTerminateEvent(Event);
            break;
        }
        case static_cast<xpf::EVENT_ID>(SysMon::EventId::ImageLoad):
        {
            this->OnImageLoadEvent(Event);
            break;
        }
        default:
        {
            break;
        }
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::OnProcessCreateEvent                                |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::UmHookPlugin::OnProcessCreateEvent(
    _In_ const xpf::IEvent* Event
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PEPROCESS eProcess = nullptr;
    bool isEprocessProtected = false;

    //
    // First get a specific event.
    //
    const SysMon::ProcessCreateEvent& eventInstanceRef = *(static_cast<const SysMon::ProcessCreateEvent*>(Event));

    SysMonLogTrace("Handling UmHookPlugin::OnProcessCreateEvent for pid %d",
                   eventInstanceRef.ProcessPid());

    //
    // We should be in the context of the parent process. Have to lookup.
    //
    status = ::PsLookupProcessByProcessId(ULongToHandle(eventInstanceRef.ProcessPid()),
                                          &eProcess);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to retrieve eprocess. status = %!STATUS!",
                       status);
        return;
    }

    //
    // Now query what we need with eprocess.
    //
    isEprocessProtected = KmHelper::WrapperIsProtectedProcess(eProcess);

    //
    // Dereference the object - we are on process create routine.
    // While we should not trigger its deletion, use the safe api,
    // so we are sure we avoid deadlocks.
    //
    ::ObDereferenceObjectDeferDelete(eProcess);
    eProcess = nullptr;

    //
    // If process is protected, we bail.
    //
    if (isEprocessProtected)
    {
        SysMonLogInfo("Process with pid %d is protected! Will not inject!",
                      eventInstanceRef.ProcessPid());
        return;
    }

    //
    // Intialize the structure which describes the required metadata.
    //
    SysMon::UmInjectionDllData dllData;

    dllData.ProcessId = eventInstanceRef.ProcessPid();
    dllData.LoadedDlls = 0;

    //
    // ntdll is always required. By default we wait for native dll to load.
    // On windows 7 we also depend on kernel32 always.
    // Native ones must be loaded in wow as well.
    //
    // For injection we are going to use LoadLibraryExW.
    //
    dllData.RequiredDlls = UM_INJECTION_DATA_SYSTEM32_NTDLL_FLAG;
    if (this->m_IsWindows7)
    {
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG;
    }
    dllData.LoadDllRoutineName = "LoadLibraryExW";

    //
    // We have a dependency on kernel32.dll - We are using LoadLibraryExW
    // to inject our DLL. This is required as we need 3 parameters.
    // We do not want to deal with cfg and marking the memory as valid call target,
    // so we are going to take the extra dependency here as it makes our life easier.
    //
    if (eventInstanceRef.ProcessArchitecture() == SysMon::ProcessArchitecture::WoWX86onX64)
    {
        //
        // We need to wait for the wow subsystem to be properly initialized.
        // This brings a whole load of dependencies
        //
        // On windows 7 we also depend on user32 from syswow64.
        //
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSWOW64_NTDLL_FLAG;
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSWOW64_KERNEL32_FLAG;
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_WOW64_FLAG;
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_WOW64WIN_FLAG;
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_WOW64CPU_FLAG;
        if (this->m_IsWindows7)
        {
            dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_USER32_FLAG;
            dllData.RequiredDlls |= UM_INJECTION_DATA_SYSWOW64_USER32_FLAG;
        }

        dllData.MatchingDll = UM_INJECTION_DATA_SYSWOW64_KERNEL32_FLAG;
        dllData.InjectedDllPath = this->m_UmDllWin32Path.View();
    }
    else if (eventInstanceRef.ProcessArchitecture() == SysMon::ProcessArchitecture::x64)
    {
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG;

        dllData.MatchingDll = UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG;
        dllData.InjectedDllPath = this->m_UmDllX64Path.View();
    }
    else if (eventInstanceRef.ProcessArchitecture() == SysMon::ProcessArchitecture::x86)
    {
        dllData.RequiredDlls |= UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG;

        dllData.MatchingDll = UM_INJECTION_DATA_SYSTEM32_KERNEL32_FLAG;
        dllData.InjectedDllPath = this->m_UmDllWin32Path.View();
    }

    SysMonLogInfo("Prepared injection data for pid %d. Required DLLs: %d. Matching dll for LdrLoad: %d. ",
                  dllData.ProcessId,
                  dllData.RequiredDlls,
                  dllData.MatchingDll);

    //
    // Now we extend our list with this structure.
    // If for some reason we did not received a process termination event and we have
    // a pid reuse, we overwrite the structure.
    //
    xpf::ExclusiveLockGuard guard{*this->m_ProcessDataLock};
    this->RemoveInjectionDataForPid(eventInstanceRef.ProcessPid());

    /* Not much we can do if this fails. Simply skip process. */
    status = this->m_ProcessData.Emplace(xpf::Move(dllData));
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to insert injection data for pid %d. Required DLLs %d. status = %!STATUS!",
                       dllData.ProcessId,
                       dllData.RequiredDlls,
                       status);
    }
    else
    {
        SysMonLogTrace("Successfully handled UmHookPlugin::OnProcessCreateEvent - created injection data for pid %d",
                       eventInstanceRef.ProcessPid());
    }
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::OnProcessTerminateEvent                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::UmHookPlugin::OnProcessTerminateEvent(
    _In_ const xpf::IEvent* Event
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    //
    // First get a specific event.
    //
    const SysMon::ProcessTerminateEvent& eventInstanceRef = *(static_cast<const SysMon::ProcessTerminateEvent*>(Event));

    SysMonLogTrace("Handling UmHookPlugin::OnProcessTerminateEvent for pid %d",
                   eventInstanceRef.ProcessPid());

    //
    // Erase injection data for this process.
    //
    xpf::ExclusiveLockGuard guard{*this->m_ProcessDataLock};
    this->RemoveInjectionDataForPid(eventInstanceRef.ProcessPid());

    SysMonLogTrace("Handled UmHookPlugin::OnProcessTerminateEvent for pid %d",
                   eventInstanceRef.ProcessPid());
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::OnImageLoadEvent                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::UmHookPlugin::OnImageLoadEvent(
    _In_ const xpf::IEvent* Event
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    //
    // First get a specific event.
    //
    const SysMon::ImageLoadEvent& eventInstanceRef = *(static_cast<const SysMon::ImageLoadEvent*>(Event));

    SysMonLogTrace("Handling UmHookPlugin::OnImageLoadEvent for pid %d - Image %S",
                   eventInstanceRef.ProcessPid(),
                   eventInstanceRef.ImagePath().View().Buffer());
    //
    // Take exclusive lock as we also might erase data.
    //
    xpf::ExclusiveLockGuard guard{ *this->m_ProcessDataLock };

    SysMon::UmInjectionDllData* injectionData = this->FindInjectionDataForPid(eventInstanceRef.ProcessPid());
    if (nullptr != injectionData)
    {
        if (injectionData->LoadedDlls == injectionData->RequiredDlls)
        {
            /* We have all required dlls - enqueue an apc to do injection. */
            HelperUmHookPluginInject(*injectionData);

            /* No point in keeping this data. */
            this->RemoveInjectionDataForPid(injectionData->ProcessId);
        }
        else
        {
            /* Injection data is present - now check if the loaded dll is one of the known ones. */
            uint32_t systemDllFlag = 0;
            for (size_t i = 0; i < XPF_ARRAYSIZE(UM_INJECTION_DLL_PATH_FLAGS); ++i)
            {
                if (eventInstanceRef.ImagePath().View().EndsWith(UM_INJECTION_DLL_PATH_FLAGS[i].DllPath, false))
                {
                    systemDllFlag = UM_INJECTION_DLL_PATH_FLAGS[i].DllFlag;
                    break;
                }
            }

            /* Mark the dll as loaded - do not attempt injection here, wait next dll as we need to wait for relocs .*/
            /* We do this as we use LoadLibraryExW which may be forwarded to kernelbase dll. */
            /* The next loaded dll will be kernelbase so it will be safe to perform injection then. */
            injectionData->LoadedDlls |= systemDllFlag;

            /* If this dll is the one we need to find the routine, we lookup here. */
            if (injectionData->MatchingDll == systemDllFlag)
            {
                injectionData->LoadDllRoutine = KmHelper::HelperFindExport(eventInstanceRef.ImageBase(),
                                                                           eventInstanceRef.ImageSize(),
                                                                           true,
                                                                           injectionData->LoadDllRoutineName.Buffer());
            }
        }
    }

    SysMonLogTrace("Handled UmHookPlugin::OnImageLoadEvent for pid %d - Image %S",
                   eventInstanceRef.ProcessPid(),
                   eventInstanceRef.ImagePath().View().Buffer());
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::RemoveInjectionDataForPid                           |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::UmHookPlugin::RemoveInjectionDataForPid(
    _In_ uint32_t ProcessPid
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    size_t i = 0;

    while (i < this->m_ProcessData.Size())
    {
        if (this->m_ProcessData[i].ProcessId == ProcessPid)
        {
            NTSTATUS status = this->m_ProcessData.Erase(i);
            XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));
        }
        else
        {
            ++i;
        }
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::UmHookPlugin::FindInjectionDataForPid                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

SysMon::UmInjectionDllData* XPF_API
SysMon::UmHookPlugin::FindInjectionDataForPid(
    _In_ uint32_t ProcessPid
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    for (size_t i = 0; i < this->m_ProcessData.Size(); ++i)
    {
        if (this->m_ProcessData[i].ProcessId == ProcessPid)
        {
            return xpf::AddressOf(this->m_ProcessData[i]);
        }
    }
    return nullptr;
}
