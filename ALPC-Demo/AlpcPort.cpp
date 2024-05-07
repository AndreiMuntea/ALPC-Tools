/**
 * @file        ALPC-Tools/ALPC-Demo/AlpcPort.cpp
 *
 * @brief       In this file we implement a wrapper over ALPC API
 *              which will help with sending and receiving data.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"
#include "AlpcPort.hpp"
#include "NtAlpcApi.hpp"


/**
 * @brief   This code will go into paged section.
 */
XPF_SECTION_PAGED;

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::AlpcPort::Connect(
    _In_ _Const_ const xpf::StringView<wchar_t>& PortName,
    _Inout_ xpf::Optional<AlpcRpc::AlpcPort>& Port
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UNICODE_STRING ustrPortName = { 0 };
    ALPC_PORT_ATTRIBUTES portAttributes = { 0 };

    XPF_MAX_PASSIVE_LEVEL();

    //
    // We will not initialize over an already initialized lock.
    // Assert here and bail early.
    //
    if ((Port.HasValue()))
    {
        XPF_DEATH_ON_FAILURE(false);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Start by creating a new port. This will be an empty one.
    // It will be initialized below.
    //
    Port.Emplace();

    //
    // We failed to create a port. It shouldn't happen.
    // Assert here and bail early.
    //
    if (!Port.HasValue())
    {
        XPF_DEATH_ON_FAILURE(false);
        return STATUS_NO_DATA_DETECTED;
    }
    AlpcRpc::AlpcPort& port = (*Port);

    //
    // Now create the port lock.
    //
    status = xpf::ReadWriteLock::Create(&port.m_PortLock);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // And now copy the string name.
    // The string buffer must be smaller than MAX_USHORT / 2 characters.
    //
    status = port.m_PortName.Append(PortName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if (port.m_PortName.BufferSize() == 0 ||
        port.m_PortName.BufferSize() >= USHORT_MAX / 2)
    {
        return STATUS_INVALID_PARAMETER;
    }
    ::RtlInitUnicodeString(&ustrPortName,
                           &port.m_PortName[0]);

    //
    // Now prepare the port attributes.
    //
    portAttributes.MaxMessageLength = AlpcRpc::AlpcPort::MAX_MESSAGE_SIZE;
    portAttributes.Flags = ALPC_PORTFLG_CAN_IMPERSONATE |
                           ALPC_PORTFLG_LPC_REQUESTS_ALLOWED |
                           ALPC_PORTFLG_CAN_DUPLICATE_OBJECTS;
    portAttributes.DupObjectTypes = 0xFFFFFFFF;
    portAttributes.MaxPoolUsage = xpf::NumericLimits<size_t>::MaxValue();
    portAttributes.MaxSectionSize = xpf::NumericLimits<size_t>::MaxValue();;
    portAttributes.MaxViewSize = xpf::NumericLimits<size_t>::MaxValue();;
    portAttributes.MaxTotalSectionSize = xpf::NumericLimits<size_t>::MaxValue();;

    portAttributes.SecurityQos.Length = sizeof(portAttributes.SecurityQos);
    portAttributes.SecurityQos.ImpersonationLevel = SECURITY_IMPERSONATION_LEVEL::SecurityImpersonation;
    portAttributes.SecurityQos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    portAttributes.SecurityQos.EffectiveOnly = FALSE;

    //
    // And now do the actual connection.
    //
    status = ::NtAlpcConnectPort(&port.m_PortHandle,
                                 &ustrPortName,
                                 NULL,
                                 &portAttributes,
                                 ALPC_MSGFLG_SYNC_REQUEST,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if (INVALID_HANDLE_VALUE == port.m_PortHandle || NULL == port.m_PortHandle)
    {
        port.m_PortHandle = NULL;
        return STATUS_INVALID_HANDLE;
    }

    //
    // All good.
    //
    return STATUS_SUCCESS;
}

void XPF_API
AlpcRpc::AlpcPort::Disconnect(
    void
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    //
    // Port was not created.
    //
    if (!this->m_PortLock.HasValue())
    {
        return;
    }

    //
    // Wait for send - receive operations.
    //
    xpf::ExclusiveLockGuard guard{ (*this->m_PortLock) };

    if (INVALID_HANDLE_VALUE != this->m_PortHandle && NULL != this->m_PortHandle)
    {
        NTSTATUS status = ::NtAlpcDisconnectPort(this->m_PortHandle,
                                                 0);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));

        status = ::NtClose(this->m_PortHandle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));
    }
    this->m_PortHandle = NULL;
}

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::AlpcPort::SendReceive(
    _In_ _Const_ const void* InputBuffer,
    _In_ size_t InputSize,
    _Inout_ xpf::Buffer<xpf::MemoryAllocator>& Output,
    _Inout_ xpf::Buffer<xpf::MemoryAllocator>& ViewOutput
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::Buffer<xpf::MemoryAllocator> sendBuffer;
    xpf::Buffer<xpf::MemoryAllocator> recvBuffer;
    xpf::Buffer<xpf::MemoryAllocator> attributesBuffer;

    xpf::StreamReader recvBuffReader{ recvBuffer };

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // Acquire lock to prevent port disconnection.
    //
    xpf::SharedLockGuard guard{ *this->m_PortLock };
    if (NULL == this->m_PortHandle)
    {
        return STATUS_PORT_DISCONNECTED;
    }

    //
    // Init i/o buffers.
    //
    status = this->InitializePortMessage(InputBuffer,
                                         InputSize,
                                         sendBuffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = this->InitializePortMessage(nullptr,
                                         AlpcRpc::AlpcPort::MAX_MESSAGE_SIZE - sizeof(PORT_MESSAGE),
                                         recvBuffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Now prepare the receive attributes.
    //
    status = this->InitializeMessageAttributes(attributesBuffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Now we send the message and wait for the answer.
    //
    SIZE_T receiveLength = recvBuffer.GetSize();
    status = ::NtAlpcSendWaitReceivePort(this->m_PortHandle,
                                         ALPC_MSGFLG_SYNC_REQUEST,
                                         static_cast<PORT_MESSAGE*>(sendBuffer.GetBuffer()),
                                         NULL,
                                         static_cast<PORT_MESSAGE*>(recvBuffer.GetBuffer()),
                                         &receiveLength,
                                         static_cast<ALPC_MESSAGE_ATTRIBUTES*>(attributesBuffer.GetBuffer()),
                                         NULL);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // And deserialize the output.
    //
    if (receiveLength < sizeof(PORT_MESSAGE))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    PORT_MESSAGE outPortMessage = { 0 };
    if (!recvBuffReader.ReadBytes(sizeof(PORT_MESSAGE), reinterpret_cast<uint8_t*>(&outPortMessage)))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    //
    // Skip until we reach offset to data.
    //
    for (uint16_t i = 0; i < outPortMessage.u2.s2.DataInfoOffset; ++i)
    {
        uint8_t discardedByte = 0;
        status = recvBuffReader.ReadBytes(1, &discardedByte);
        if (!NT_SUCCESS(status))
        {
            break;
        }
    }

    //
    // And capture the output
    //
    if (NT_SUCCESS(status))
    {
        status = Output.Resize(outPortMessage.u1.s1.DataLength);
        if (NT_SUCCESS(status))
        {
            if (!recvBuffReader.ReadBytes(outPortMessage.u1.s1.DataLength, static_cast<uint8_t*>(Output.GetBuffer())))
            {
                status = STATUS_INVALID_BUFFER_SIZE;
            }
        }
    }

    //
    // From A. Ionescu's syscan 2014 talk
    //  "Many ALPC servers don't reply to datagrams - ever
    //   But ALPC warns you when this is wrong - message header has
    //   LPC_CONTINUATION_REQUIRED flag set"
    // Let's be good citizens and check here.
    //
    if ((outPortMessage.u2.s2.Type & LPC_CONTINUATION_REQUIRED) != 0)
    {
        ALPC_MESSAGE_ATTRIBUTES* attributes = static_cast<ALPC_MESSAGE_ATTRIBUTES*>(attributesBuffer.GetBuffer());
        if ((attributes->ValidAttributes & ALPC_FLG_MSG_DATAVIEW_ATTR) != 0)
        {
            ALPC_DATA_VIEW_ATTR* view = static_cast<ALPC_DATA_VIEW_ATTR*>(::AlpcGetMessageAttribute(attributes,
                                                                          ALPC_FLG_MSG_DATAVIEW_ATTR));
            if (nullptr != view)
            {
                view->Flags |= ALPC_MSGVIEWATTR_RELEASE;

                /* Attempt to capture the view - best effort. */
                NTSTATUS viewCaptureStatus = ViewOutput.Resize(view->ViewSize);
                if (NT_SUCCESS(viewCaptureStatus))
                {
                    xpf::ApiCopyMemory(ViewOutput.GetBuffer(),
                                       view->ViewBase,
                                       view->ViewSize);
                }
            }
        }

        NTSTATUS releaseStatus = ::NtAlpcSendWaitReceivePort(this->m_PortHandle,
                                                             ALPC_MSGFLG_RELEASE_MESSAGE,
                                                             static_cast<PORT_MESSAGE*>(recvBuffer.GetBuffer()),
                                                             NULL,
                                                             NULL,
                                                             &receiveLength,
                                                             NULL,
                                                             NULL);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(releaseStatus));
    }

    //
    // Relay back the original status.
    //
    return status;
}

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::AlpcPort::InitializeMessageAttributes(
    _Inout_ xpf::Buffer<xpf::MemoryAllocator>& AttributesBuffer
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SIZE_T requiredSize = 0;
    UINT32 requiredAttributes = ULONG_MAX;

    //
    // First we compute the required size. Expect anything.
    //
    status = ::AlpcInitializeMessageAttribute(requiredAttributes,
                                              nullptr,
                                              0,
                                              &requiredSize);
    if (requiredSize == 0)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    //
    // Now resize the buffer.
    //
    status = AttributesBuffer.Resize(requiredSize);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Preinit with 0.
    //
    xpf::ApiZeroMemory(AttributesBuffer.GetBuffer(),
                       AttributesBuffer.GetSize());

    //
    // And finally initialize it.
    //
    return ::AlpcInitializeMessageAttribute(requiredAttributes,
                                            static_cast<ALPC_MESSAGE_ATTRIBUTES*>(AttributesBuffer.GetBuffer()),
                                            AttributesBuffer.GetSize(),
                                            &requiredSize);
}

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::AlpcPort::InitializePortMessage(
    _In_opt_ _Const_ const void* Buffer,
    _In_ size_t BufferSize,
    _Inout_ xpf::Buffer<xpf::MemoryAllocator>& PortMessage
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    size_t totalSize = 0;
    PORT_MESSAGE message = { 0 };

    xpf::StreamWriter bufferWriter{ PortMessage };

    if (!xpf::ApiNumbersSafeAdd(sizeof(PORT_MESSAGE), BufferSize, &totalSize))
    {
        return STATUS_INTEGER_OVERFLOW;
    }
    if (totalSize > AlpcRpc::AlpcPort::MAX_MESSAGE_SIZE)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }
    status = PortMessage.Resize(totalSize);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    message.u1.s1.DataLength = static_cast<uint16_t>(BufferSize);
    message.u1.s1.TotalLength = static_cast<uint16_t>(totalSize);

    if (!bufferWriter.WriteBytes(sizeof(message), reinterpret_cast<const uint8_t*>(&message)))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    if (BufferSize > 0 && nullptr != Buffer)
    {
        if (!bufferWriter.WriteBytes(BufferSize, static_cast<const uint8_t*>(Buffer)))
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    return STATUS_SUCCESS;
}
