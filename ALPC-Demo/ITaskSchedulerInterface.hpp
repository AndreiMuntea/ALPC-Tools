/**
 * @file        ALPC-Tools/ALPC-Demo/ITaskSchedulerInterface.hpp
 *
 * @brief       In this file we have basic functionality for ITaskScheduler
 *              interface. It is fully documented here:
 *              https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-tsch/96c9b399-c373-4490-b7f5-78ec3849444e
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
 * @brief   {86d35949-83c9-4044-b424-db363231fd0c}.
 *
 * @details https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-tsch/fbab083e-f79f-4216-af4c-d5104a913d40
 */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gITaskSchedulerServiceIdentifier =
{
    .SyntaxGUID = { 0x86D35949, 0x83C9, 0x4044, { 0xB4, 0x24, 0xDB, 0x36, 0x32, 0x31, 0xFD, 0x0c } },
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
class ITaskSchedulerInterface final
{
 private:
    /**
     * @brief  Default constructor. It is protected
     *         as the static API Connect should be used instead.
     */
     ITaskSchedulerInterface(void) noexcept(true) = default;
 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
     ~ITaskSchedulerInterface(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are deleted.
     */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(ITaskSchedulerInterface, delete);

    /**
     * @brief          This method is used to create a ITaskSchedulerInterface port.
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
        _Inout_ xpf::Optional<ITaskSchedulerInterface>& Port
    ) noexcept(true)
    {
        /* Create the port. */
        Port.Emplace();

        /* And now connect and bind to the actual port. First we try NDR64. */
        (*Port).m_Port.Reset();
        NTSTATUS status = AlpcRpc::RpcAlpcClientPort::Connect(gITaskSchedulerServiceIdentifier,
                                                              gNdr64TransferSyntaxIdentifier,
                                                              (*Port).m_Port);
        if (NT_SUCCESS(status))
        {
            return status;
        }

        /* On fail, we retry with DCE-NDR. */
        (*Port).m_Port.Reset();
        return AlpcRpc::RpcAlpcClientPort::Connect(gITaskSchedulerServiceIdentifier,
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
     * @brief          SchRpcRun (Opnum 12)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-tsch/77f2250d-500a-40ee-be18-c82f7079c4f0
     *
     * @param[in]      path         - full path of a task.
     * @param[in]      cArgs        - the number of strings supplied in pArgs.
     * @param[in]      pArgs        - an array of strings of size cArgs.
     * @param[in]      flags        - see MSDN for possible values.
     * @param[in]      sessionId    - specify a terminal server session in which to run the task.
     * @param[in]      user         - the user context under which to run the task.
     * @param[out]     pGuid        - a GUID for the task instance created as result of this call.
     * @param[out]     hResult      - the return value of the call, caller responsibility to inspect this.
     *
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SchRpcRun(
        _In_ DceNdrWstring& path,
        _In_ DcePrimitiveType<uint32_t> cArgs,
        _In_ DceUniquePointer<DceConformantArray<DceNdrWstring>>& pArgs,
        _In_ DcePrimitiveType<uint32_t> flags,
        _In_ DcePrimitiveType<uint32_t> sessionId,
        _In_ DceUniquePointer<DceNdrWstring> user,
        _Out_ DcePrimitiveType<uuid_t>* pGuid,
        _Out_ DcePrimitiveType<uint32_t>* hResult
    ) noexcept(true)
    {
        //
        //  HRESULT SchRpcRun(
        //  [in, string] const wchar_t* path,
        //  [in] DWORD cArgs,
        //  [in, string, size_is(cArgs), unique] const wchar_t** pArgs,
        //  [in] DWORD flags,
        //  [in] DWORD sessionId,
        //  [in, unique, string] const wchar_t* user,
        //  [out] GUID* pGuid);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *pGuid = {};
        *hResult = {};

        /* Serialize input. */
        iBuffer.Marshall(path)
               .Marshall(cArgs)
               .Marshall(pArgs)
               .Marshall(flags)
               .Marshall(sessionId)
               .Marshall(user);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(12, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*pGuid)
               .Unmarshall(*hResult);
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
};  // class ITaskSchedulerInterface
};  // namespace DceNdr
};  // namespace AlpcRpc
