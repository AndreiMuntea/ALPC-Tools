/**
 * @file        ALPC-Tools/ALPC-Demo/AlpcPort.hpp
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

#pragma once

#include "precomp.hpp"

namespace AlpcRpc
{
/**
 * @brief   This class uses the undocumented ALPC-Api to
 *          facilitate a connection and a wrapper over sending and receiving messages.
 *          It is specialized for RPC purposes, so attributes include impersonation.
 */
class AlpcPort final
{
 private:
    /**
     * @brief  Default constructor. It is private
     *         as the static API Connect should be used instead.
     */
    AlpcPort(void) noexcept(true) = default;

 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
    ~AlpcPort(void) noexcept(true)
    {
        this->Disconnect();
    }

    /**
     * @brief  Copy and Move are deleted.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(AlpcRpc::AlpcPort, delete);

    /**
     * @brief          This method is used to connect to a given port name.
     *
     * @param[in]      PortName - the port where we should connect to.
     *
     * @param[in,out]  Port     - an Optional object which will contain the
     *                            port if we manage to connect to, or it will be
     *                            empty on failure.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Connect(
        _In_ _Const_ const xpf::StringView<wchar_t>& PortName,
        _Inout_ xpf::Optional<AlpcRpc::AlpcPort>& Port
    ) noexcept(true);

    /**
     * @brief          This method is used to disconnect a connected port
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           Waits for all send-receive messages to finish before
     *                 disconnecting the port.
     */
    void XPF_API
    Disconnect(
        void
    ) noexcept(true);

    /**
     * @brief          This method is used to send a message and wait a response.
     *
     * @param[in]      InputBuffer  - the message to be sent.
     *
     * @param[in]      InputSize    - The number of bytes contained by InputBuffer.
     *
     * @param[in,out]  Output       - will contain the response message.
     *
     * @param[in,out]  ViewOutput   - will capture the response view buffer, if any.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SendReceive(
        _In_ _Const_ const void* InputBuffer,
        _In_ size_t InputSize,
        _Inout_ xpf::Buffer& Output,
        _Inout_ xpf::Buffer& ViewOutput
    ) noexcept(true);

 private:
    /**
     * @brief          This method is used to initialize the message attributes buffer
     *                 which will be received from the server.
     *                 While we are not sending any attributes, we can receive them from server-side.
     *                 So we need to take care of them.
     *
     * @param[in,out]  AttributesBuffer - will contain the attributes message buffer.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    InitializeMessageAttributes(
        _Inout_ xpf::Buffer& AttributesBuffer
    ) noexcept(true);

    /**
     * @brief          This method is used to initialize a port message.
     *
     * @param[in]      Buffer       - optional parameter which will be copied to Port message if present.
     *
     * @param[in]      BufferSize   - Port message will be guaranteed to be large enough to store these bytes.
     *
     * @param[in,out]  PortMessage  - the port message to be initialized.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    InitializePortMessage(
        _In_opt_ _Const_ const void* Buffer,
        _In_ size_t BufferSize,
        _Inout_ xpf::Buffer& PortMessage
    ) noexcept(true);

 private:
    static constexpr uint16_t MAX_MESSAGE_SIZE = 0x1000;

    xpf::Optional<xpf::ReadWriteLock> m_PortLock;
    xpf::String<wchar_t> m_PortName;
    HANDLE m_PortHandle = NULL;

    /**
     * @brief   Default MemoryAllocator is our friend as it requires access to the private
     *          default constructor. It is used in the Create() method to ensure that
     *          no partially constructed objects are created but instead they will be
     *          all fully initialized.
     */
     friend class xpf::MemoryAllocator;
};  // AlpcPort
};  // namespace AlpcRpc
