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

#pragma once

#include "AlpcMon.hpp"
#include "HookEngine.hpp"

#include "NtAlpcApi.hpp"

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                               Data for tracking ALPC-Messages.                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static constexpr const uuid_t gInterestingInterfaces[] =
{
    /* SamrInterface            -  {12345778-1234-ABCD-EF00-0123456789AC} */
    { 0x12345778, 0x1234, 0xABCD, { 0xEF, 0x00, 0x01, 0x23, 0x45, 0x67, 0x89, 0xac  } },

    /* SvcCtlInterface          -  {367ABB81-9844-35F1-AD32-98F038001003} */
    { 0x367ABB81, 0x9844, 0x35F1, { 0xAD, 0x32, 0x98, 0xF0, 0x38, 0x00, 0x10, 0x03 } },

    /* LocalFwInterface         -  {2FB92682-6599-42DC-AE13-BD2CA89BD11C} */
    { 0x2FB92682, 0x6599, 0x42DC, { 0xAE, 0x13, 0xBD, 0x2C, 0xA8, 0x9B, 0xD1, 0x1c } },

    /* ITaskSchedulerService    -  {86d35949-83c9-4044-b424-db363231fd0c} */
    { 0x86D35949, 0x83C9, 0x4044, { 0xB4, 0x24, 0xDB, 0x36, 0x32, 0x31, 0xFD, 0x0c } },

    /* IEventService            -  {f6beaff7-1e19-4fbb-9f8f-b89e2018337c} */
    { 0xF6BEAFF7, 0x1E19, 0x4FBB, { 0x9F, 0x8F, 0xB8, 0x9E, 0x20, 0x18, 0x33, 0x7c } },
};

struct AlpcMonitoringPortData
{
    uuid_t      BoundInterface      = { 0 };
    uint64_t    TransferSyntaxFlags = { 0 };
    uint64_t    PortHandle          = { 0 };
};

struct AlpcMonitoringData
{
    xpf::BusyLock MonitoredDataLock;
    xpf::Vector<AlpcMonitoringPortData> MonitoredPortHandles;
};
static AlpcMonitoringData gAlpcMonitoringData;

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                               Helpers to track a specific port                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void
XPF_API
AlpcMonitoringDataStopTrackingPort(
    _In_ uint32_t PortHandle
) noexcept(true)
{
    /* Remove the port from the list. */
    xpf::ExclusiveLockGuard guard{ gAlpcMonitoringData.MonitoredDataLock };
    for (size_t i = 0; i < gAlpcMonitoringData.MonitoredPortHandles.Size(); ++i)
    {
        if (gAlpcMonitoringData.MonitoredPortHandles[i].PortHandle == PortHandle)
        {
            (void) gAlpcMonitoringData.MonitoredPortHandles.Erase(i);
            break;
        }
    }
}

static void
XPF_API
AlpcMonitoringDataStartTrackingPort(
    _In_ uint32_t PortHandle,
    _In_ const uuid_t& BoundInterface,
    _In_ const uint64_t& TransferSyntaxFlags
) noexcept(true)
{
    /* In case we're having a handle re-use conflict, we need to erase it. */
    AlpcMonitoringDataStopTrackingPort(PortHandle);

    /* Create a new entry. */
    AlpcMonitoringPortData data =
    {
        .BoundInterface         = BoundInterface,
        .TransferSyntaxFlags    = TransferSyntaxFlags,
        .PortHandle             = PortHandle
    };

    /* Ignore any failures. Best effort. */
    xpf::ExclusiveLockGuard guard{ gAlpcMonitoringData.MonitoredDataLock };
    (void) gAlpcMonitoringData.MonitoredPortHandles.Emplace(data);
}

static bool
XPF_API
AlpcMonitoringDataIsPortMonitored(
    _In_ uint32_t PortHandle,
    _Out_ uuid_t* BoundInterface,
    _Out_ uint64_t* TransferSyntaxFlags
) noexcept(true)
{
    *BoundInterface = { 0 };
    *TransferSyntaxFlags = 0;

    xpf::SharedLockGuard guard{ gAlpcMonitoringData.MonitoredDataLock };
    for (size_t i = 0; i < gAlpcMonitoringData.MonitoredPortHandles.Size(); ++i)
    {
        if (gAlpcMonitoringData.MonitoredPortHandles[i].PortHandle == PortHandle)
        {
            *BoundInterface = gAlpcMonitoringData.MonitoredPortHandles[i].BoundInterface;
            *TransferSyntaxFlags = gAlpcMonitoringData.MonitoredPortHandles[i].TransferSyntaxFlags;
            return true;
        }
    }
    return false;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                               Helpers to interpret data from a specific message.                                |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static bool
XPF_API
AlpcMessageIsInterfaceInteresting(
    _In_ const uuid_t& Interface
) noexcept(true)
{
    /* Monitor only a specific range of interfaces. */
    for (size_t i = 0; i < XPF_ARRAYSIZE(gInterestingInterfaces); ++i)
    {
        if (Interface == gInterestingInterfaces[i])
        {
            return true;
        }
    }
    return false;
}

static void
XPF_API
AlpcMessageHandleRequest(
    _In_ HANDLE PortHandle,
    _In_opt_ PORT_MESSAGE* Message
) noexcept(true)
{
    void* buffer = nullptr;
    uint64_t messageType = 0;

    __try
    {
        /* No message presented. */
        if (nullptr == Message)
        {
            __leave;
        }

        /* An invalid message was presented. */
        if (Message->u1.s1.DataLength >= Message->u1.s1.TotalLength)
        {
            __leave;
        }

        /* All messages starts with a message type. */
        if (Message->u1.s1.DataLength < sizeof(uint64_t))
        {
            __leave;
        }

        /* Grab the data buffer. */
        buffer = xpf::AlgoAddToPointer(Message,
                                       Message->u1.s1.TotalLength - Message->u1.s1.DataLength);
        /* Now check the message type. */
        messageType = *(static_cast<uint64_t*>(buffer));

        /* Handle depending on type. */
        if (messageType == LRPC_MESSAGE_TYPE::lmtBind)
        {
            /* Invalid size for this message. */
            if (Message->u1.s1.DataLength < sizeof(LRPC_BIND_MESSAGE))
            {
                __leave;
            }
            /* Get the specific bind message. */
            LRPC_BIND_MESSAGE* bindMessage = static_cast<LRPC_BIND_MESSAGE*>(buffer);

            /* And now the bound interface and transfer syntax. */
            uuid_t boundInterface = bindMessage->Interface.SyntaxGUID;
            uint64_t transferSyntaxFlags = bindMessage->TransferSyntaxFlags;

            /* If the interface is interesting, we track the port. */
            if (AlpcMessageIsInterfaceInteresting(boundInterface))
            {
                uint32_t portHandle = HandleToULong(PortHandle);
                AlpcMonitoringDataStartTrackingPort(portHandle, boundInterface, transferSyntaxFlags);
            }
        }
        else if (messageType == LRPC_MESSAGE_TYPE::lmtRequest)
        {
            UM_KM_INTERESTING_RPC_MESSAGE message = { 0 };

            /* Initialize message header. */
            message.Header.ProviderSignature = UM_KM_CALLBACK_SIGNATURE;
            message.Header.RequestType = UM_KM_REQUEST_TYPE;
            message.Header.BufferLength = sizeof(UM_KM_INTERESTING_RPC_MESSAGE) - sizeof(UM_KM_MESSAGE_HEADER);

            /* Initialize message body. */
            message.MessageType = UM_KM_MESSAGE_TYPE_INTERESTING_RPC_MESSAGE;

            /* If the port is not tracked, we bail. */
            uint32_t portHandle = HandleToULong(PortHandle);
            if (!AlpcMonitoringDataIsPortMonitored(portHandle, &message.InterfaceGuid, &message.TransferSyntaxFlag))
            {
                __leave;
            }

            /* Invalid size for this message. We cap this so we can capture the buffer. */
            if (Message->u1.s1.DataLength < sizeof(LRPC_REQUEST_MESSAGE))
            {
                __leave;
            }
            if (Message->u1.s1.DataLength >= sizeof(message.Buffer))
            {
                __leave;
            }

            /* Get the specific request message. */
            LRPC_REQUEST_MESSAGE* requestMessage = static_cast<LRPC_REQUEST_MESSAGE*>(buffer);

            /* Grab the called procnum. */
            message.ProcedureNumber = requestMessage->Procnum;

            /* Capture the buffer. */
            xpf::ApiCopyMemory(&message.Buffer[0],
                               static_cast<uint8_t*>(buffer) + sizeof(LRPC_REQUEST_MESSAGE),
                               Message->u1.s1.DataLength - sizeof(LRPC_REQUEST_MESSAGE));

            /* Dispatch the message to kernel. */
            (void) HookEngineNotifyKernel(&message.Header);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        XPF_NOTHING();
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                               NtAlpcConnectPortHook                                                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

NTSTATUS NTAPI
NtAlpcConnectPortHook(
    _Out_ HANDLE* PortHandle,
    _In_ UNICODE_STRING* PortName,
    _In_opt_ OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_opt_ void* PortAttributes,
    _In_ UINT32 Flags,
    _In_opt_ SID* RequiredServerSid,
    _Inout_updates_bytes_to_opt_(*BufferLength, *BufferLength) void* ConnectionMessage,
    _Inout_opt_ SIZE_T* BufferLength,
    _Inout_opt_ void* OutMessageAttributes,
    _Inout_opt_ void* InMessageAttributes,
    _In_opt_ LARGE_INTEGER* Timeout
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    auto originalApi = static_cast<decltype(&NtAlpcConnectPortHook)>(gNtAlpcConnectPortHook.OriginalApi);

    UM_KM_ALPC_PORT_CONNECTED message = { 0 };
    size_t portNameMaxSize = 0;

    __try
    {
        /* Call the original API. */
        status = originalApi(PortHandle,
                             PortName,
                             ObjectAttributes,
                             PortAttributes,
                             Flags,
                             RequiredServerSid,
                             ConnectionMessage,
                             BufferLength,
                             OutMessageAttributes,
                             InMessageAttributes,
                             Timeout);
        if (!NT_SUCCESS(status))
        {
            __leave;
        }

        /* On success, we notify the kernel about this newly created connection. */
        portNameMaxSize = sizeof(message.PortName) - sizeof(L'\0');
        if (PortName->Length > portNameMaxSize)
        {
            /* We don't expect port names with huge names. But if this happens, assert here. */
            /* We need to increase the size in the actual message. */
            XPF_ASSERT(false);
            __leave;
        }

        /* Initialize message header. */
        message.Header.ProviderSignature = UM_KM_CALLBACK_SIGNATURE;
        message.Header.RequestType = UM_KM_REQUEST_TYPE;
        message.Header.BufferLength = sizeof(UM_KM_ALPC_PORT_CONNECTED) - sizeof(UM_KM_MESSAGE_HEADER);

        /* Initialize message body. */
        message.MessageType = UM_KM_MESSAGE_TYPE_ALPC_PORT_CONNECTED;
        xpf::ApiCopyMemory(&message.PortName[0],
                           PortName->Buffer,
                           PortName->Length);

        /* Notify the kernel. Ignore the response - discard the fail.*/
        status = HookEngineNotifyKernel(&message.Header);
        if (!NT_SUCCESS(status))
        {
            status = STATUS_SUCCESS;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* We don't want to be on the stack if this crashes. */
        status = STATUS_UNHANDLED_EXCEPTION;
    }
    return status;
}

HookEngineApi gNtAlpcConnectPortHook =
{
    .DllName        = L"ntdll.dll",
    .ApiName        = "NtAlpcConnectPort",
    .OriginalApi    = nullptr,
    .HookApi        = NtAlpcConnectPortHook
};

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                               NtAlpcDisconnectPortHook                                                          |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

NTSTATUS NTAPI
NtAlpcDisconnectPortHook(
    _In_ HANDLE PortHandle,
    _In_ UINT32 Flags
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    uint32_t handleValue = 0;
    auto originalApi = static_cast<decltype(&NtAlpcDisconnectPortHook)>(gNtAlpcDisconnectPortHook.OriginalApi);

    __try
    {
        /* First we untrack the port. */
        handleValue = HandleToULong(PortHandle);
        AlpcMonitoringDataStopTrackingPort(handleValue);

        /* Then we call the original API. */
        status = originalApi(PortHandle,
                             Flags);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* We don't want to be on the stack if this crashes. */
        status = STATUS_UNHANDLED_EXCEPTION;
    }
    return status;
}

HookEngineApi gNtAlpcDisconnectPortHook =
{
    .DllName = L"ntdll.dll",
    .ApiName = "NtAlpcDisconnectPort",
    .OriginalApi = nullptr,
    .HookApi = NtAlpcDisconnectPortHook
};

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                               NtAlpcSendWaitReceivePort                                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

NTSTATUS NTAPI
NtAlpcSendWaitReceivePortHook(
    _In_ HANDLE PortHandle,
    _In_ UINT32 Flags,
    _In_reads_bytes_opt_(MessageToSend->u1.s1.TotalLength) PORT_MESSAGE* MessageToSend,
    _Inout_opt_ ALPC_MESSAGE_ATTRIBUTES* SendMessageAttributes,
    _Out_writes_bytes_to_opt_(*BufferLength, *BufferLength) PORT_MESSAGE* MessageToReceive,
    _Inout_opt_ SIZE_T* BufferLength,
    _Inout_opt_ ALPC_MESSAGE_ATTRIBUTES* ReceiveMessageAttributes,
    _In_opt_ LARGE_INTEGER* Timeout
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    auto originalApi = static_cast<decltype(&NtAlpcSendWaitReceivePortHook)>(gNtAlpcSendWaitReceivePortHook.OriginalApi);

    __try
    {
        /* Handle the request. */
        AlpcMessageHandleRequest(PortHandle,
                                 MessageToSend);

        /* Then we call the original API. */
        status = originalApi(PortHandle,
                             Flags,
                             MessageToSend,
                             SendMessageAttributes,
                             MessageToReceive,
                             BufferLength,
                             ReceiveMessageAttributes,
                             Timeout);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* We don't want to be on the stack if this crashes. */
        status = STATUS_UNHANDLED_EXCEPTION;
    }
    return status;
}

HookEngineApi gNtAlpcSendWaitReceivePortHook =
{
    .DllName = L"ntdll.dll",
    .ApiName = "NtAlpcSendWaitReceivePort",
    .OriginalApi = nullptr,
    .HookApi = NtAlpcSendWaitReceivePortHook
};
