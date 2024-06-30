/**
 * @file        ALPC-Tools/AlpcMon_Sys/globals.cpp
 *
 * @brief       This file contains the global data definitions
 *              that we use throughout the sysmon sensor.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "PluginManager.hpp"
#include "KmHelper.hpp"
#include "RegistryUtils.hpp"
#include "WorkQueue.hpp"

#include "globals.hpp"
#include "trace.hpp"

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       NON-PAGED SECTION AREA                                                    |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   Put everything below here in non-paged section.
 */
XPF_SECTION_DEFAULT;


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Global Data Structure                                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

/**
 * @brief   This contains the global variables that are required throughout the sensor.
 *          They are only created once, when the driver starts and they are destroyed
 *          when driver is unloaded.
 */
struct GlobalData
{
    /**
     * @brief   Used to throw events.
     */
    xpf::EventBus EventBus;
    /**
     * @brief   Keeps track of all plugins. 
     */
    xpf::Optional<SysMon::PluginManager> PluginManager;
    /**
     * @brief   The driver registry key.
     */
    xpf::String<wchar_t> RegistryKey{ SYSMON_NPAGED_ALLOCATOR };
    /**
     * @brief   The driver's running directory as Dos path.
     */
    xpf::String<wchar_t> DriverDirectoryDos{ SYSMON_NPAGED_ALLOCATOR };
    /**
     * @brief   The os version.
     */
    RTL_OSVERSIONINFOEXW OsVersion = { 0 };
    /**
     * @brief   Dynamically resolved export data.
     */
    SYSMON_GLOBAL_DYNAMIC_EXPORT_DATA DynamicExportData = { 0 };
    /**
     * @brief   The driver object describing our driver.
     */
    void* DriverObject = nullptr;
};  // struct GlobalData

/**
 * @brief   A global variable which contains all the required resources.
 *          Will be allocated from nonpaged pool in GlobalDataCreate.
 */
static GlobalData* gGlobalData = nullptr;


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataGetBusInstance                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
xpf::EventBus* XPF_API
GlobalDataGetBusInstance(
    void
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    return xpf::AddressOf(gGlobalData->EventBus);
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataGetOsVersion                                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//


_Use_decl_annotations_
const RTL_OSVERSIONINFOEXW* XPF_API
GlobalDataGetOsVersion(
    void
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    return xpf::AddressOf(gGlobalData->OsVersion);
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataGetDynamicData                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//


_Use_decl_annotations_
const SYSMON_GLOBAL_DYNAMIC_EXPORT_DATA* XPF_API
GlobalDataGetDynamicData(
    void
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    return xpf::AddressOf(gGlobalData->DynamicExportData);
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataGetRegistryKey                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//


_Use_decl_annotations_
const xpf::StringView<wchar_t> XPF_API
GlobalDataGetRegistryKey(
    void
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    return gGlobalData->RegistryKey.View();
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataGetDosInstallationDirectory                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//


_Use_decl_annotations_
const xpf::StringView<wchar_t> XPF_API
GlobalDataGetDosInstallationDirectory(
    void
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    return gGlobalData->DriverDirectoryDos.View();
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataGetDriverObject                                                 |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
void* XPF_API
GlobalDataGetDriverObject(
    void
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    return gGlobalData->DriverObject;
}

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
// |                                       GlobalDataCreate                                                          |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
GlobalDataCreate(
    _In_ _Const_ PCUNICODE_STRING RegistryKey,
    _In_ void* DriverObject
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();
    XPF_DEATH_ON_FAILURE(nullptr != RegistryKey);
    XPF_DEATH_ON_FAILURE(nullptr != DriverObject);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::StringView<wchar_t> regKey;
    xpf::Buffer regKeyBuffer{ SYSMON_NPAGED_ALLOCATOR };

    SysMonLogInfo("Creating global data...");

    //
    // First we create the actual global data structure.
    //
    gGlobalData = static_cast<GlobalData*>(xpf::CriticalMemoryAllocator::AllocateMemory(sizeof(GlobalData)));
    if (nullptr == gGlobalData)
    {
        SysMonLogError("Insufficient resources to allocate the global data! Will bail.");
        status = STATUS_INSUFFICIENT_RESOURCES;

        goto CleanUp;
    }
    xpf::MemoryAllocator::Construct(gGlobalData);

    //
    // And now we save the registry key.
    //
    status = KmHelper::HelperUnicodeStringToView(*RegistryKey, regKey);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperUnicodeStringToView failed with %!STATUS!",
                       status);
        goto CleanUp;
    }

    status = gGlobalData->RegistryKey.Append(regKey);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Saving registry key failed with %!STATUS!",
                       status);
        goto CleanUp;
    }

    //
    // Now grab a reference to the driver object.
    //
    gGlobalData->DriverObject = DriverObject;
    ObReferenceObject(gGlobalData->DriverObject);

    //
    // And now we query the information about the driver directory.
    //
    status = KmHelper::WrapperRegistryQueryValueKey(gGlobalData->RegistryKey.View(),
                                                    L"InstallDirectory",
                                                    REG_SZ,
                                                    &regKeyBuffer);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("WrapperRegistryQueryValueKey failed with %!STATUS!",
                       status);
        goto CleanUp;
    }
    status = gGlobalData->DriverDirectoryDos.Append(static_cast<const wchar_t*>(regKeyBuffer.GetBuffer()));
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Saving driver directory from registry key failed with %!STATUS!",
                       status);
        goto CleanUp;
    }

    //
    // Find info about the running OS version.
    //
    gGlobalData->OsVersion.dwOSVersionInfoSize = sizeof(gGlobalData->OsVersion);
    status = ::RtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(xpf::AddressOf(gGlobalData->OsVersion)));
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("RtlGetVersion failed with %!STATUS!",
                       status);
        goto CleanUp;
    }

    //
    // Resolve exports.
    //
    gGlobalData->DynamicExportData.ApiRtlImageNtHeader = static_cast<PFUNC_RtlImageNtHeader>(
                                                         KmHelper::WrapperMmGetSystemRoutine(L"RtlImageNtHeader"));                         // NOLINT(*)
    gGlobalData->DynamicExportData.ApiRtlImageNtHeaderEx = static_cast<PFUNC_RtlImageNtHeaderEx>(
                                                           KmHelper::WrapperMmGetSystemRoutine(L"RtlImageNtHeaderEx"));                     // NOLINT(*)
    gGlobalData->DynamicExportData.ApiPsIsProtectedProcess = static_cast<PFUNC_PsIsProtectedProcess>(
                                                             KmHelper::WrapperMmGetSystemRoutine(L"PsIsProtectedProcess"));                 // NOLINT(*)
    gGlobalData->DynamicExportData.ApiPsIsProtectedProcessLight = static_cast<PFUNC_PsIsProtectedProcessLight>(
                                                                  KmHelper::WrapperMmGetSystemRoutine(L"PsIsProtectedProcessLight"));       // NOLINT(*)
    gGlobalData->DynamicExportData.ApiPsGetProcessWow64Process = static_cast<PFUNC_PsGetProcessWow64Process>(
                                                                 KmHelper::WrapperMmGetSystemRoutine(L"PsGetProcessWow64Process"));         // NOLINT(*)
    //
    // Now the plugin manager.
    //
    status = SysMon::PluginManager::Create(gGlobalData->PluginManager,
                                           GlobalDataGetBusInstance());
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("SysMon::PluginManager::Create failed with %!STATUS!",
                       status);
        goto CleanUp;
    }

    //
    // All good.
    //
    status = STATUS_SUCCESS;
    SysMonLogInfo("Successfully created global data");

CleanUp:
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Creating the global data failed with status %!STATUS!",
                       status);

        GlobalDataDestroy();
    }
    return status;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       GlobalDataDestroy                                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
_Use_decl_annotations_
void XPF_API
GlobalDataDestroy(
    void
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    SysMonLogInfo("Destroying global data...");

    if (gGlobalData != nullptr)
    {
        //
        // Start with plugin manager.
        //
        gGlobalData->PluginManager.Reset();

        //
        // Then rundown the event bus.
        //
        gGlobalData->EventBus.Rundown();

        //
        // Then the structure.
        //
        xpf::MemoryAllocator::Destruct(gGlobalData);

        //
        // Now remove one reference from the driver object.
        //
        if (nullptr != gGlobalData->DriverObject)
        {
            ::ObDereferenceObjectDeferDelete(gGlobalData->DriverObject);
            gGlobalData->DriverObject = nullptr;
        }

        //
        // Then free the resources.
        //
        xpf::CriticalMemoryAllocator::FreeMemory(gGlobalData);
        gGlobalData = nullptr;
    }

    SysMonLogInfo("Global data destroyed!");
}
