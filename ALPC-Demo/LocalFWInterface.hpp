/**
 * @file        ALPC-Tools/ALPC-Demo/LocalFwInterface.hpp
 *
 * @brief       In this file we have basic functionality for LocalFwInterface
 *              interface. This is an undocumented interface. However, on a close
 *              inspection with IDA, the APIs are mostly the same as in the documented remote one:
 *              https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fasp/1503b9d7-7fec-4793-9972-6ad58720c9db
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
#include "RpcAlpcClient.hpp"

/**
 * @brief   {2FB92682-6599-42DC-AE13-BD2CA89BD11C}.
 */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gLocalFwInterface =
{
    .SyntaxGUID = { 0x2FB92682, 0x6599, 0x42DC, { 0xAE, 0x13, 0xBD, 0x2C, 0xA8, 0x9B, 0xD1, 0x1c } },
    .SyntaxVersion = { 1, 0 }
};

namespace AlpcRpc
{
namespace DceNdr
{

/**
 * @brief   This class contains the minimalistic functionality to make RPC calls manually via ALPC.
 *          It is not fully featured. It only contains some methods. More can be added later.
 */
class LocalFwInterface final
{
 private:
    /**
     * @brief  Default constructor. It is protected
     *         as the static API Connect should be used instead.
     */
     LocalFwInterface(void) noexcept(true) = default;
 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
     ~LocalFwInterface(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are deleted.
     */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(LocalFwInterface, delete);

    /**
     * @brief          This method is used to create a LocalFwInterface port.
     *
     * @param[in,out]  Port                    - an Optional object which will contain the
     *                                           port if we manage to connect to, or it will be
     *                                           empty on failure.
     *
     * @return         A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::Optional<LocalFwInterface>& Port
    ) noexcept(true)
    {
        /* Create the port. */
        Port.Emplace();

        /* And now connect and bind to the actual port. First we try NDR64. */
        (*Port).m_Port.Reset();
        NTSTATUS status = AlpcRpc::RpcAlpcClientPort::Connect(gLocalFwInterface,
                                                              gNdr64TransferSyntaxIdentifier,
                                                              (*Port).m_Port);
        if (NT_SUCCESS(status))
        {
            return status;
        }

        /* On fail, we retry with DCE-NDR. */
        (*Port).m_Port.Reset();
        return AlpcRpc::RpcAlpcClientPort::Connect(gLocalFwInterface,
                                                   gDceNdrTransferSyntaxIdentifier,
                                                   (*Port).m_Port);
    }

    /**
     * @brief       Getter for the underlying transfer syntax flags used by this port instance.
     *
     * @return      The value of the used transfer syntax flags.
     */
    inline uint32_t
    TransferSyntaxFlags(
        void
    ) noexcept(true)
    {
        return (*this->m_Port).TransferSyntaxFlags();
    }

    /**
     * @brief          FWOpenPolicyStore  (Opnum 0)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fasp/2157e39a-1bf0-4e0c-abae-0811fd918b11
     *
     * @param[in]      BinaryVersion   - This parameter specifies the RPC interface binary version.
     *                                   This implies versions of the methods and versions of the structures.
     * @param[in]      StoreType       - This parameter specifies the policy store type that the client wants to open.
     *                                   We should always be opening FW_STORE_TYPE_LOCAL (0x2).
     * @param[in]      AccessRight     - This parameter specifies the read or read/write access rights that
     *                                   the client is requesting on the store.
     *                                      FW_POLICY_ACCESS_RIGHT_READ         - 0x1
     *                                      FW_POLICY_ACCESS_RIGHT_READ_WRITE   - 0x2
     * @param[in]      dwFlags         - This parameter is not used. The server MUST ignore this parameter.
     *                                   The client SHOULD pass a value of zero.
     * @param[out]     phPolicyStore   - This parameter contains a handle to the opened store.
     *                                   Can be closed with FWClosePolicyStore.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    FWOpenPolicyStore(
        _In_ DcePrimitiveType<uint16_t> BinaryVersion,
        _In_ DceEnumerationType StoreType,
        _In_ DceEnumerationType AccessRight,
        _In_ DcePrimitiveType<uint32_t> dwFlags,
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* phPolicyStore,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //  unsigned long RRPC_FWOpenPolicyStore(
        //      [in] FW_CONN_HANDLE rpcConnHandle,
        //      [in] unsigned short BinaryVersion,
        //      [in, range(FW_STORE_TYPE_INVALID+1, FW_STORE_TYPE_MAX-1)] FW_STORE_TYPE StoreType,
        //      [in, range(FW_POLICY_ACCESS_RIGHT_INVALID+1, FW_POLICY_ACCESS_RIGHT_MAX-1)] FW_POLICY_ACCESS_RIGHT AccessRight,
        //      [in] unsigned long dwFlags,
        //      [out] PFW_POLICY_STORE_HANDLE phPolicyStore);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *phPolicyStore = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(BinaryVersion)
               .Marshall(StoreType)
               .Marshall(AccessRight)
               .Marshall(dwFlags);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(0, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*phPolicyStore)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          FWClosePolicyStore   (Opnum 1)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fasp/2157e39a-1bf0-4e0c-abae-0811fd918b11
     *
     * @param[in,out]  phPolicyStore   - This parameter contains a handle to the opened store.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    FWClosePolicyStore(
        _Inout_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* phPolicyStore,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //  unsigned long FWClosePolicyStore(
        //         [in] FW_CONN_HANDLE rpcConnHandle,
        //         [in, out] PFW_POLICY_STORE_HANDLE phPolicyStore);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(*phPolicyStore);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(1, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*phPolicyStore)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          FWDeleteAllFirewallRules   (Opnum 8)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fasp/ca2da2f2-b8ef-4981-abba-fed5c713990d
     *
     * @param[in]      hPolicyStore    - This parameter contains a handle to the opened store.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    FWDeleteAllFirewallRules(
        _In_ const DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>& hPolicyStore,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //   unsigned long FWDeleteAllFirewallRules(
        //      [in] FW_CONN_HANDLE rpcConnHandle,
        //      [in] FW_POLICY_STORE_HANDLE hPolicyStore);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(hPolicyStore);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(8, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*retValue);
        return oBuffer.Status();
    }

 private:
     xpf::Optional<AlpcRpc::RpcAlpcClientPort> m_Port;

    /**
     * @brief   Default MemoryAllocator is our friend as it requires access to the private
     *          default constructor. It is used in the Create() method to ensure that
     *          no partially constructed objects are created but instead they will be
     *          all fully initialized.
     */
     friend class xpf::MemoryAllocator;
};  // class LocalFwInterface
};  // namespace DceNdr
};  // namespace AlpcRpc
