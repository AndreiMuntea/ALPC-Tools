/**
 * @file        ALPC-Tools/AlpcMon_Sys/FirmwareTableHandlerFilter.cpp
 *
 * @brief       In this file we define the functionality related to
 *              registering a firmware table handler callback
 *              to filter requests, especially useful for our usermode
 *              hooking component. As we cannot create any ports, or open
 *              any device handlers, because the process may go into
 *              sleep, be suspended, and so on, we are using another
 *              functionality provided by the OS.
 *
 * @note        We use SystemRegisterFirmwareTableInformationHandler
 *              to register our own callback which can be used afterwards
 *              in NtQuerySystemInformation() calls. While this is not
 *              documented by M$, there are numerous open source examples
 *              on how to use this functionality, you just have to look them up.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "globals.hpp"
#include "Events.hpp"
#include "UmKmComms.hpp"

#include "FirmwareTableHandlerFilter.hpp"
#include "trace.hpp"

/**
 * @brief   Put everything below here in paged section.
 */
XPF_SECTION_PAGED;

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       FirmwareTableHandlerCallback                                              |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

static NTSTATUS __cdecl
FirmwareTableHandlerCallback(
    _In_opt_ PSYSTEM_FIRMWARE_TABLE_INFORMATION TableInfo
)
{
    XPF_MAX_PASSIVE_LEVEL();

    //
    // Sanity check.
    //
    if (NULL == TableInfo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // This is for other provider.
    //
    if (TableInfo->ProviderSignature != UM_KM_CALLBACK_SIGNATURE)
    {
        return STATUS_INVALID_SIGNATURE;
    }

    //
    // For now we only handle get requests.
    //
    if (TableInfo->Action != UM_KM_REQUEST_TYPE)
    {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Ensure we have enough stack. Do not handle messages
    // if we do not have at least half a page available.
    //
    if (::IoGetRemainingStackSize() < PAGE_SIZE / 2)
    {
        return STATUS_SUCCESS;
    }

    //
    // Create and dispatch the event - ignore any potential failures.
    //
    xpf::UniquePointer<xpf::IEvent> broadcastEvent;
    NTSTATUS status = SysMon::UmHookEvent::Create(broadcastEvent,
                                                  TableInfo);
    if (NT_SUCCESS(status))
    {
        status = GlobalDataGetBusInstance()->Dispatch(broadcastEvent.Get());
    }
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Dispatching UM hook event failed with status = %!STATUS!",
                       status);

        status = STATUS_SUCCESS;
    }

    return status;
}


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       FirmwareTableHandlerCallback                                              |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///
static NTSTATUS XPF_API
FirmwareTableHandlerChangeRoutine(
    _In_ void* DriverObject,
    _In_ bool Register
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();
    XPF_DEATH_ON_FAILURE(nullptr != DriverObject);

    SYSTEM_FIRMWARE_TABLE_HANDLER handler = { 0 };

    handler.ProviderSignature = UM_KM_CALLBACK_SIGNATURE;
    handler.Register = (Register) ? TRUE
                                  : FALSE;
    handler.FirmwareTableHandler = &FirmwareTableHandlerCallback;
    handler.DriverObject = DriverObject;

    return ::ZwSetSystemInformation(XPF_SYSTEM_INFORMATION_CLASS::XpfSystemRegisterFirmwareTableInformationHandler,
                                    &handler,
                                    sizeof(handler));
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       FirmwareTableHandlerFilterStart                                           |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
FirmwareTableHandlerFilterStart(
    _In_ void* DriverObject
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != DriverObject);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Registering firmware table handler routine...");

    status = FirmwareTableHandlerChangeRoutine(DriverObject, true);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Registering firmware table handler routine failed with status = %!STATUS!",
                       status);
        return status;
    }

    SysMonLogInfo("Successfully registered firmware table handler routine routine!");
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       FirmwareTableHandlerFilterStop                                            |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
void XPF_API
FirmwareTableHandlerFilterStop(
    _In_ void* DriverObject
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != DriverObject);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Unregistering firmware table handler routine...");

    status = FirmwareTableHandlerChangeRoutine(DriverObject, false);
    if (!NT_SUCCESS(status))
    {
        SysMonLogCritical("Unregistering firmware table handler routine failed with status = %!STATUS!",
                          status);
        XPF_ASSERT(false);
        return;
    }

    SysMonLogInfo("Successfully unregistered firmware table handler routine routine!");
}
