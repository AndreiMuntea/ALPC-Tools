/**
 * @file        ALPC-Tools/AlpcMon_Sys/globals.hpp
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

#pragma once

#include "precomp.hpp"
#include "WorkQueue.hpp"

/**
 * @brief       Creates the global data.
 *
 * @return      A proper ntstatus error code.
 *
 * @param[in]   RegistryKey     - The driver registry key.
 *
 * @param[in]   DriverObject    - The underlying driver object for our driver.
 *
 * @note        This method is intended to be called only from driver entry,
 *              thus it requires passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
GlobalDataCreate(
    _In_ _Const_ PCUNICODE_STRING RegistryKey,
    _In_ void* DriverObject
) noexcept(true);

/**
 * @brief       Destroys the global data.
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level as it is intended
 *              to be called from DriverUnload
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
GlobalDataDestroy(
    void
) noexcept(true);

/**
 * @brief       Getter for the global bus instance.
 *
 * @return      The underlying bus instance.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
xpf::EventBus* XPF_API
GlobalDataGetBusInstance(
    void
) noexcept(true);

/**
 * @brief       Getter for the version info of the os.
 *
 * @return      The os version info.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
const RTL_OSVERSIONINFOEXW* XPF_API
GlobalDataGetOsVersion(
    void
) noexcept(true);

/**
 * @brief       Getter for the registry key of the driver.
 *
 * @return      The driver's registry key.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
const xpf::StringView<wchar_t> XPF_API
GlobalDataGetRegistryKey(
    void
) noexcept(true);

/**
 * @brief       Getter for the DOS path where the driver install dir is.
 *
 * @return      The driver's install dir in DOS path
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
const xpf::StringView<wchar_t> XPF_API
GlobalDataGetDosInstallationDirectory(
    void
) noexcept(true);

/**
 * @brief       Getter for the driver object.
 *
 * @return      The driver object
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
void* XPF_API
GlobalDataGetDriverObject(
    void
) noexcept(true);

/**
 * @brief       Notify global data that all filtering routines are properly set.
 *
 * @return      Nothing
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
void XPF_API
GlobalDataMarkFilteringRegistrationFinished(
    void
) noexcept(true);

/**
 * @brief       Used to retrieve the state of filtering notification routines.
 *
 * @return      True if filtering registration was finished,
 *              false otherwise.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
bool XPF_API
GlobalDataIsFilteringRegistrationFinished(
    void
) noexcept(true);

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                The section below here contains dynamically resolved APIs                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

#ifndef DOXYGEN_SHOULD_SKIP_THIS
XPF_EXTERN_C_START();

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         RtlImageNtHeader                                                                        |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
typedef PIMAGE_NT_HEADERS (NTAPI* PFUNC_RtlImageNtHeader)(_In_ void* Base);

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         RtlImageNtHeaderEx                                                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
typedef NTSTATUS (NTAPI* PFUNC_RtlImageNtHeaderEx)(_In_ uint32_t Flags,
                                                   _In_ void* Base,
                                                   _In_ uint64_t Size,
                                                   _Out_ PIMAGE_NT_HEADERS* OutHeaders);
//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         PsIsProtectedProcess                                                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
typedef BOOLEAN (NTAPI* PFUNC_PsIsProtectedProcess)(_In_ PEPROCESS Process);

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         PsIsProtectedProcessLight                                                               |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
typedef BOOLEAN(NTAPI* PFUNC_PsIsProtectedProcessLight)(_In_ PEPROCESS Process);


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         PsGetProcessWow64Process                                                                |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
typedef PVOID(NTAPI* PFUNC_PsGetProcessWow64Process)(_In_ PEPROCESS Process);

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         Structure containing all above APIs                                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//
typedef struct _SYSMON_GLOBAL_DYNAMIC_EXPORT_DATA
{
    PFUNC_RtlImageNtHeader              ApiRtlImageNtHeader;
    PFUNC_RtlImageNtHeaderEx            ApiRtlImageNtHeaderEx;
    PFUNC_PsIsProtectedProcess          ApiPsIsProtectedProcess;
    PFUNC_PsIsProtectedProcessLight     ApiPsIsProtectedProcessLight;
    PFUNC_PsGetProcessWow64Process      ApiPsGetProcessWow64Process;
} SYSMON_GLOBAL_DYNAMIC_EXPORT_DATA;

XPF_EXTERN_C_END();
#endif  // DOXYGEN_SHOULD_SKIP_THIS

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         Getter for dynamic data structure.                                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

/**
 * @brief       Getter for the dynamically resolved exports.
 *
 * @return      The structure containing all dynamically resolved data.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
const SYSMON_GLOBAL_DYNAMIC_EXPORT_DATA* XPF_API
GlobalDataGetDynamicData(
    void
) noexcept(true);
