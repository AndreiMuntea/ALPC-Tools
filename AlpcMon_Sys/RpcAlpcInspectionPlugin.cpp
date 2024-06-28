/**
 * @file        ALPC-Tools/AlpcMon_Sys/RpcAlpcInspectionPlugin.cpp
 *
 * @brief       This is the plugin responsible with the actual inspection
 *              of RPC messages coming from UM hooking component.
 *              These are on par with the ones from ALPC-Demo.
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
#include "UmKmComms.hpp"
#include "RpcEngine.hpp"

#include "RpcAlpcInspectionPlugin.hpp"
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
// |                                       SysMon::RpcAlpcInspectionPlugin::Create                                   |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::RpcAlpcInspectionPlugin::Create(
    _Inout_ xpf::SharedPointer<SysMon::IPlugin>& Plugin,
    _In_ const uint64_t& PluginId
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::SharedPointer<SysMon::RpcAlpcInspectionPlugin> plugin{ SYSMON_NPAGED_ALLOCATOR };

    SysMonLogInfo("Creating RpcAlpcInspectionPlugin...");

    //
    // First create the plugin.
    //
    plugin = xpf::MakeSharedWithAllocator<SysMon::RpcAlpcInspectionPlugin>(SYSMON_NPAGED_ALLOCATOR,
                                                                           PluginId);
    if (plugin.IsEmpty())
    {
        SysMonLogError("Insufficient resources to create the plugin");

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Cast it as IPlugin.
    //
    Plugin = xpf::DynamicSharedPointerCast<SysMon::IPlugin, SysMon::RpcAlpcInspectionPlugin>(xpf::Move(plugin));
    if (Plugin.IsEmpty())
    {
        SysMonLogError("Insufficient resources to cast the plugin");

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // All good.
    //
    SysMonLogInfo("Created RpcAlpcInspectionPlugin.");
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::RpcAlpcInspectionPlugin::OnEvent                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::RpcAlpcInspectionPlugin::OnEvent(
    _Inout_ xpf::IEvent* Event,
    _Inout_ xpf::EventBus* Bus
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    XPF_UNREFERENCED_PARAMETER(Bus);

    switch (Event->EventId())
    {
        case static_cast<xpf::EVENT_ID>(SysMon::EventId::UmHookMessage):
        {
            this->OnUmHookEvent(Event);
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
// |                                       SysMon::RpcAlpcInspectionPlugin::OnUmHookEvent                            |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void XPF_API
SysMon::RpcAlpcInspectionPlugin::OnUmHookEvent(
    _In_ const xpf::IEvent* Event
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    //
    // First get a specific event and the underlying buffer.
    //
    const SysMon::UmHookEvent& eventInstanceRef = *(static_cast<const SysMon::UmHookEvent*>(Event));
    const uint32_t processId = HandleToUlong(::PsGetCurrentProcessId());

    //
    // If we have no message, we are done.
    //
    UM_KM_MESSAGE_HEADER* messageHeader = static_cast<UM_KM_MESSAGE_HEADER*>(eventInstanceRef.Message());
    if (nullptr == messageHeader)
    {
        return;
    }

    //
    // Handle differently based on type.
    //
    uint64_t messageType = UmKmMessageGetType(messageHeader);
    if (messageType == UM_KM_MESSAGE_TYPE_ALPC_PORT_CONNECTED)
    {
        UM_KM_ALPC_PORT_CONNECTED* portConnectedMessage = reinterpret_cast<UM_KM_ALPC_PORT_CONNECTED*>(messageHeader);
        SysMonLogInfo("Process with pid %d connected to port %S on handle %I64d",
                       processId,
                       portConnectedMessage->PortName,
                       portConnectedMessage->PortHandle);
    }
    else if (messageType == UM_KM_MESSAGE_TYPE_INTERESTING_RPC_MESSAGE)
    {
        UM_KM_INTERESTING_RPC_MESSAGE* rpcInterestingMessage = reinterpret_cast<UM_KM_INTERESTING_RPC_MESSAGE*>(messageHeader);
        SysMon::RpcEngine::Analyze(&rpcInterestingMessage->Buffer[0],
                                   sizeof(rpcInterestingMessage->Buffer),
                                   rpcInterestingMessage->InterfaceGuid,
                                   rpcInterestingMessage->ProcedureNumber,
                                   rpcInterestingMessage->TransferSyntaxFlag,
                                   rpcInterestingMessage->PortHandle);
    }
}
