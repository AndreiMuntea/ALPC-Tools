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
GlobalDataGetInstallationDirectory(
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

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                The section below here contains exported APIs definitions                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

#ifndef DOXYGEN_SHOULD_SKIP_THIS
XPF_EXTERN_C_START();

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         RtlImageDirectoryEntryToData                                                            |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

NTSYSAPI PVOID NTAPI
RtlImageDirectoryEntryToData(
    _In_ PVOID BaseOfImage,
    _In_ BOOLEAN MappedAsImage,
    _In_ USHORT DirectoryEntry,
    _Out_ PULONG Size
);

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         KeInitializeApc                                                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

typedef enum _KAPC_ENVIRONMENT
{
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment,
    InsertApcEnvironment
} KAPC_ENVIRONMENT;

typedef VOID (NTAPI* PKNORMAL_ROUTINE)(
    _In_ PVOID NormalContext,
    _In_ PVOID SystemArgument1,
    _In_ PVOID SystemArgument2
);

typedef VOID (NTAPI* PKKERNEL_ROUTINE)(
    _In_ PKAPC Apc,
    _Inout_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ PVOID* NormalContext,
    _Inout_ PVOID* SystemArgument1,
    _Inout_ PVOID* SystemArgument2
);

typedef VOID (NTAPI* PKRUNDOWN_ROUTINE) (
    _In_ PKAPC Apc
);

NTSYSAPI VOID NTAPI
KeInitializeApc(
    _Out_ PRKAPC Apc,
    _In_ PRKTHREAD Thread,
    _In_ KAPC_ENVIRONMENT Environment,
    _In_ PKKERNEL_ROUTINE KernelRoutine,
    _In_opt_ PKRUNDOWN_ROUTINE RundownRoutine,
    _In_opt_ PKNORMAL_ROUTINE NormalRoutine,
    _In_ KPROCESSOR_MODE Mode,
    _In_opt_ PVOID NormalContext
);

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         KeInsertQueueApc                                                                        |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

NTSYSAPI BOOLEAN NTAPI
KeInsertQueueApc(
    _Inout_ PRKAPC Apc,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2,
    _In_ KPRIORITY Increment
);

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                         ZwSetSystemInformation                                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation = 0x0,

    SystemRegisterFirmwareTableInformationHandler = 0x4B,
} SYSTEM_INFORMATION_CLASS;

NTSYSAPI NTSTATUS NTAPI
ZwSetSystemInformation(
    _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _In_ PVOID SystemInformation,
    _In_ ULONG SystemInformationLength
);

XPF_EXTERN_C_END();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
