/**
 * @file        ALPC-Tools/ALPC-Demo/RpcAlpcClient.hpp
 *
 * @brief       In this file we implement the functionality of a RPC client
 *              which uses ALPC to communicate. It is not exhaustive, but rather
 *              minimalistic.
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
#include "AlpcPort.hpp"
#include "NtAlpcApi.hpp"
#include "DceNdr.hpp"


/**
 * @brief   The MS-RPC EndpointMapper.
 */
static constexpr xpf::StringView<wchar_t> gEpmapperPortName = {L"\\RPC Control\\epmapper"};

/**
 * @brief       {e1af8308-5d1f-11c9-91a4-08002b14a0fa}.
 *
 * @details     https://pubs.opengroup.org/onlinepubs/9629399/apdxo.htm
 */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gEpmapperInterface =
{
    .SyntaxGUID     = { 0xE1AF8308, 0x5D1F, 0x11C9, { 0x91, 0xA4, 0x08, 0x00, 0x2B, 0x14, 0xA0, 0xfa } },
    .SyntaxVersion  = { 3, 0 }
};

/**
 * @brief       {8a885d04-1ceb-11c9-9fe8-08002b104860}.
 *
 * @details     https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-rpce/b6090c2b-f44a-47a1-a13b-b82ade0137b2
 */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gDceNdrTransferSyntaxIdentifier =
{
    .SyntaxGUID     = { 0x8A885D04, 0x1CEB, 0x11C9, { 0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60 } },
    .SyntaxVersion  = { 2, 0 }
};

/**
 * @brief       {71710533-BEBA-4937-8319-B5DBEF9CCC36}.
 *
 * @details     https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rpce/dca648a5-42d3-432c-9927-2f22e50fa266
 *
 * @note        N.B This is not supported yet - but I plan to. So just define it here.
 */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gNdr64TransferSyntaxIdentifier =
{
    .SyntaxGUID = { 0x71710533, 0xBEBA, 0x4937, { 0x83, 0x19, 0xB5, 0xDB, 0xEF, 0x9C, 0xCC, 0x36 } },
    .SyntaxVersion = { 1, 0 }
};

namespace AlpcRpc
{
/**
 * @brief   This is the base class for rpc interfaces.
 */
class RpcAlpcClientPort final
{
 private:
    /**
     * @brief  Default constructor. It is protected
     *         as the static API Connect should be used instead.
     */
     RpcAlpcClientPort(void) noexcept(true) = default;

 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
     ~RpcAlpcClientPort(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are deleted.
     */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(AlpcRpc::RpcAlpcClientPort, delete);

    /**
     * @brief          This method is used to connect to a given port. It takes care of binding automatically.
     *                 Uses epmapper to automatically discover the port name.
     *
     * @param[in]      ObjectIdentifier        - GUID and SYNTAX version of the interface we want to connect to.
     * @param[in]      TransferSyntaxFlags     - one of the values of LRPC_TRANSFER_SYNTAX_*
     * @param[in,out]  Port                    - an Optional object which will contain the
     *                                           port if we manage to connect to, or it will be
     *                                           empty on failure.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Connect(
        _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& ObjectIdentifier,
        _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& TransferSyntaxFlags,
        _Inout_ xpf::Optional<AlpcRpc::RpcAlpcClientPort>& Port
    ) noexcept(true);

    /**
     * @brief          This method is used to connect to a given port via its port name. It takes care of binding automatically.
     *
     * @param[in]      PortName                - Name of the port we want to connect to.
     * @param[in]      ObjectIdentifier        - GUID and SYNTAX version of the interface we want to connect to.
     * @param[in]      TransferSyntaxFlags     - one of the values of LRPC_TRANSFER_SYNTAX_*
     * @param[in,out]  Port                    - an Optional object which will contain the
     *                                           port if we manage to connect to, or it will be
     *                                           empty on failure.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Connect(
        _In_ _Const_ const xpf::StringView<wchar_t>& PortName,
        _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& ObjectIdentifier,
        _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& TransferSyntaxFlags,
        _Inout_ xpf::Optional<AlpcRpc::RpcAlpcClientPort>& Port
    ) noexcept(true);

    /**
     * @brief           Calls a method from an already bounded port.
     *
     * @param[in]       ProcNum                 - procedure number in the interface.
     * @param[in]       MarshallBuffer          - buffer containing input parameters serialized in NDR format.
     * @param[in,out]   UnmarshallBuffer        - buffer containing output parameters serialized in NDR format.
     *
     * @return          A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    NTSTATUS
    CallProcedure(
        _In_ uint16_t ProcNum,
        _In_ _Const_ const AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
        _Inout_  AlpcRpc::DceNdr::DceMarshallBuffer& UnmarshallBuffer
    ) noexcept(true);

    /**
     * @brief       Getter for the underlying transfer syntax flags used by this port instance.
     *
     * @return      The value of m_TransferSyntaxFlags;
     */
    inline uint32_t
    TransferSyntaxFlags(
        void
    ) noexcept(true)
    {
        return this->m_TransferSyntaxFlags;
    }

 private:
     xpf::Optional<AlpcRpc::AlpcPort> m_AlpcPort;
     uint16_t m_BindingId = xpf::NumericLimits<uint16_t>::MaxValue();

     ALPC_RPC_SYNTAX_IDENTIFIER m_ObjectIdentifier = { 0 };
     ALPC_RPC_SYNTAX_IDENTIFIER m_TransferSyntax = { 0 };
     uint32_t m_TransferSyntaxFlags = xpf::NumericLimits<uint32_t>::MaxValue();

    /**
     * @brief   Default MemoryAllocator is our friend as it requires access to the private
     *          default constructor. It is used in the Create() method to ensure that
     *          no partially constructed objects are created but instead they will be
     *          all fully initialized.
     */
     friend class xpf::MemoryAllocator;
};  // class RpcAlpcClientPort

/**
 * @brief          This method is a helper API to convert a string to a DceNdr serializable string format.
 *
 * @param[in]      View                 - A view over the string to be converted.
 * @param[in,out]  NdrString            - Representation of string in a serializable NDR format.
 * @param[in]      NullTerminateString  - If true, an extra null terminator will be appended to NdrUniqueString
 *
 * @return         A proper NTSTATUS error code.
 */
_Must_inspect_result_
NTSTATUS XPF_API
HelperWstringToNdr(
    _In_ _Const_ const xpf::StringView<wchar_t>& View,
    _Inout_ AlpcRpc::DceNdr::DceNdrWstring& NdrString,
    _In_ bool NullTerminateString
) noexcept(true);

/**
 * @brief          This method is a helper API to convert a string to a DceNdr serializable string format.
 *
 * @param[in]      View                 - A view over the string to be converted.
 * @param[in,out]  NdrUniqueString      - Representation of string in a serializable NDR format as unique pointer.
 * @param[in]      NullTerminateString  - If true, an extra null terminator will be appended to NdrUniqueString
 *
 * @return         A proper NTSTATUS error code.
 */
_Must_inspect_result_
NTSTATUS XPF_API
HelperWstringToUniqueNdr(
    _In_ _Const_ const xpf::StringView<wchar_t>& View,
    _Inout_ AlpcRpc::DceNdr::DceUniquePointer<AlpcRpc::DceNdr::DceNdrWstring>& NdrUniqueString,
    _In_ bool NullTerminateString
) noexcept(true);

/**
 * @brief          This method is a helper API to convert a dce ndr string to a wstring.
 *
 * @param[in]      NdrString    - Representation of string in a serializable NDR format.
 * @param[in,out]  String       - Will hold the string data.
 *
 * @return         A proper NTSTATUS error code.
 */
_Must_inspect_result_
inline NTSTATUS XPF_API
HelperNdrWstringToWstring(
    _In_ _Const_ const AlpcRpc::DceNdr::DceNdrWstring& NdrString,
    _Inout_ xpf::String<wchar_t, AlpcRpc::DceNdr::DceAllocator>& String
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::String<wchar_t, AlpcRpc::DceNdr::DceAllocator> newBuffer;

    size_t length = NdrString.Data().Size();
    for (size_t i = 0; i < length; ++i)
    {
        wchar_t character = NdrString.Data()[i].Data();

        status = newBuffer.Append(xpf::StringView(&character, 1));
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    String = xpf::Move(newBuffer);
    return STATUS_SUCCESS;
}

/**
 * @brief          This method is a helper API to convert an unique dce ndr string to a wstring.
 *
 * @param[in]      NdrUniqueString      - Representation of string in a serializable NDR format.
 * @param[in,out]  String               - Will hold the string data.
 *
 * @return         A proper NTSTATUS error code.
 */
_Must_inspect_result_
inline NTSTATUS XPF_API
HelperUniqueNdrWstringToWstring(
    _In_ _Const_ const AlpcRpc::DceNdr::DceUniquePointer<AlpcRpc::DceNdr::DceNdrWstring>& NdrUniqueString,
    _Inout_ xpf::String<wchar_t, AlpcRpc::DceNdr::DceAllocator>& String
) noexcept(true)
{
    if (nullptr == NdrUniqueString.Data())
    {
        String.Reset();
        return STATUS_SUCCESS;
    }

    return HelperNdrWstringToWstring(*NdrUniqueString.Data(),
                                     String);
}
};  // namespace AlpcRpc
