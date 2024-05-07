/**
 * @file        ALPC-Tools/ALPC-Demo/IEventServiceInterface.hpp
 *
 * @brief       In this file we have basic functionality for IEventService
 *              interface. It is fully documented here:
 *              https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-even6/2d808edd-719a-4c69-b34a-df766adb5f0c
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
  * @brief   {f6beaff7-1e19-4fbb-9f8f-b89e2018337c}.
  *
  * @details https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-even6/2d808edd-719a-4c69-b34a-df766adb5f0c
  */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gIEventServiceIdentifier =
{
    .SyntaxGUID = { 0xF6BEAFF7, 0x1E19, 0x4FBB, { 0x9F, 0x8F, 0xB8, 0x9E, 0x20, 0x18, 0x33, 0x7c } },
    .SyntaxVersion = { 1, 0 }
};

namespace AlpcRpc
{
namespace DceNdr
{
/**
 * @brief       Wrapper over RpcInfo from IEventService interface.
 *
 * @details     In the interface, this structure is declared as:
 *              typedef struct tag_RpcInfo
 *              {
 *                DWORD m_error,
 *                      m_subErr,
 *                      m_subErrParam;
 *              } RpcInfo;
 */
class DceRpcInfo : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceRpcInfo(void) noexcept(true) = default;

     /**
      * @brief  Constructor with parameters which will initialize the members as well.
      *
      * @param[in]  Error       - initializer for m_Error.
      * @param[in]  SubErr      - initializer for m_SubErr.
      * @param[in]  SubErrParam - initializer for m_SubErrParam.
      *
      */
     DceRpcInfo(
         _In_ uint32_t Error,
         _In_ uint32_t SubErr,
         _In_ uint32_t SubErrParam
     ) noexcept(true): DceSerializableObject(),
                       m_Error{Error},
                       m_SubErr{SubErr},
                       m_SubErrParam{SubErrParam}
     {
         XPF_NOTHING();
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceRpcInfo(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted. 
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceRpcInfo, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *                 Data is simply written directly into the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         /* Structure need to be aligned inside the stream. */
         const uint8_t alignment = (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64) ? 8
                                                                                      : 4;
         status = Stream.AlignForSerialization(alignment);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         status = this->m_Error.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_SubErr.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         return this->m_SubErrParam.Marshall(Stream, LrpcTransferSyntax);
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 Data is simply read directly from the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         /* Structure need to be aligned inside the stream. */
         const uint8_t alignment = (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64) ? 8
                                                                                      : 4;
         status = Stream.AlignForDeserialization(alignment);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         status = this->m_Error.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_SubErr.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         return this->m_SubErrParam.Unmarshall(Stream, LrpcTransferSyntax);
     }

     /**
      * @brief          Getter for m_Error.
      *
      * @return         The underlying m_Error data value.
      */
     inline uint32_t XPF_API
     Error(
        void
     ) noexcept(true)
     {
         return this->m_Error.Data();
     }

     /**
      * @brief          Getter for m_SubErr.
      *
      * @return         The underlying m_SubErr data value.
      */
     inline uint32_t XPF_API
     SubErr(
        void
     ) noexcept(true)
     {
         return this->m_SubErr.Data();
     }

     /**
      * @brief          Getter for m_SubErrParam.
      *
      * @return         The underlying m_SubErrParam data value.
      */
     inline uint32_t XPF_API
     SubErrParam(
        void
     ) noexcept(true)
     {
         return this->m_SubErrParam.Data();
     }

 private:
     DcePrimitiveType<uint32_t> m_Error;
     DcePrimitiveType<uint32_t> m_SubErr;
     DcePrimitiveType<uint32_t> m_SubErrParam;
};  // class class RpcInfo

/**
 * @brief   This class contains the minimalistic functionality to make RPC calls manually via ALPC.
 *          It is not fully featured. It only contains some methods. More can be added later.
 */
class IEventServiceInterface final
{
 private:
    /**
     * @brief  Default constructor. It is protected
     *         as the static API Connect should be used instead.
     */
     IEventServiceInterface(void) noexcept(true) = default;
 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
     ~IEventServiceInterface(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are deleted.
     */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(IEventServiceInterface, delete);

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
        _Inout_ xpf::Optional<IEventServiceInterface>& Port
    ) noexcept(true)
    {
        /* Create the port. */
        Port.Emplace();

        /* And now connect and bind to the actual port. First we try NDR64. */
        (*Port).m_Port.Reset();
        NTSTATUS status = AlpcRpc::RpcAlpcClientPort::Connect(gIEventServiceIdentifier,
                                                              gNdr64TransferSyntaxIdentifier,
                                                              (*Port).m_Port);
        if (NT_SUCCESS(status))
        {
            return status;
        }

        /* On fail, we retry with DCE-NDR. */
        (*Port).m_Port.Reset();
        return AlpcRpc::RpcAlpcClientPort::Connect(gIEventServiceIdentifier,
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
     * @brief          EvtRpcRegisterControllableOperation  (Opnum 4)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-even6/0dcaa086-6ff4-4f02-96ba-0a52380a7f0e
     *
     * @param[out]     handle          - A context handle for a control object. Can be closed with EvtRpcClose.
     * @param[out]     error_status    - The return value of the call, caller responsibility to inspect this.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    EvtRpcRegisterControllableOperation(
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* handle,
        _Out_ DcePrimitiveType<uint32_t>* error_status
    ) noexcept(true)
    {
        //
        // error_status_t EvtRpcRegisterControllableOperation([out, context_handle] PCONTEXT_HANDLE_OPERATION_CONTROL* handle);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *handle = {};
        *error_status = {};

        /* Serialize input. */
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(4, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*handle)
               .Unmarshall(*error_status);
        return oBuffer.Status();
    }

    /**
     * @brief          EvtRpcClearLog (Opnum 6)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-even6/6286c156-2f70-4618-b876-09f60fb21a98
     *
     * @param[in]      control         - A handle to an operation control object. Can be obtained with EvtRpcRegisterControllableOperation.
     * @param[in]      channelPath     - A pointer to a string that contains the path of the channel to be cleared.
     * @param[in]      backupPath      - A pointer to a string that contains the path of the file in which events are
     *                                   to be saved before the clear is performed. A value of NULL indicates that
     *                                   no backup event log is to be created.
     * @param[in]      flags           - A 32-bit unsigned integer that MUST be set to zero.
     * @param[out]     error           - The method MUST return ERROR_SUCCESS (0x00000000) on success; otherwise,
     *                                   it MUST return an implementation-specific nonzero value.
     * @param[out]     error_status    - The return value of the call, caller responsibility to inspect this.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    EvtRpcClearLog(
        _In_ const DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>& control,
        _In_ const DceNdrWstring& channelPath,
        _In_ const DceUniquePointer<DceNdrWstring>& backupPath,
        _In_ DcePrimitiveType<uint32_t> flags,
        _Out_ DceRpcInfo* error,
        _Out_ DcePrimitiveType<uint32_t>* error_status
    ) noexcept(true)
    {
        //
        //   error_status_t EvtRpcClearLog(
        //   [in, context_handle] PCONTEXT_HANDLE_OPERATION_CONTROL control,
        //   [in, range(0, MAX_RPC_CHANNEL_NAME_LENGTH), string] LPCWSTR channelPath,
        //   [in, unique, range(0, MAX_RPC_FILE_PATH_LENGTH), string] LPCWSTR backupPath,
        //   [in] DWORD flags,
        //   [out] RpcInfo* error);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *error = {};
        *error_status = {};

        /* Serialize input. */
        iBuffer.Marshall(control)
               .Marshall(channelPath)
               .Marshall(backupPath)
               .Marshall(flags);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(6, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*error)
               .Unmarshall(*error_status);
        return oBuffer.Status();
    }

    /**
     * @brief          EvtRpcClose   (Opnum 13)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-even6/a5173272-a59e-41d5-b1e6-7366c163ff24
     *
     * @param[out]     handle          - A context handle for a control object.
     * @param[out]     error_status    - The return value of the call, caller responsibility to inspect this.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    EvtRpcClose(
        _Inout_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* handle,
        _Out_ DcePrimitiveType<uint32_t>* error_status
    ) noexcept(true)
    {
        //
        //  error_status_t EvtRpcClose([in, out, context_handle] void** handle);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *error_status = {};

        /* Serialize input. */
        iBuffer.Marshall(*handle);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(13, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*handle)
               .Unmarshall(*error_status);
        return oBuffer.Status();
    }

    /**
     * @brief          EvtRpcGetChannelList   (Opnum 19)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-even6/61cc2c5b-22c6-4f8e-8155-04ea1280ac90
     *
     * @param[in]      flags           - A 32-bit unsigned integer that MUST be set to zero when sent.
     * @param[out]     numChannelPaths - A pointer to a 32-bit unsigned integer that contains the number of channel names.
     * @param[out]     channelPaths    - A pointer to an array of strings that contain all the channel names.
     * @param[out]     error_status    - The return value of the call, caller responsibility to inspect this.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    EvtRpcGetChannelList(
        _In_ DcePrimitiveType<uint32_t> flags,
        _Out_ DcePrimitiveType<uint32_t>* numChannelPaths,
        _Out_ DceUniquePointer<DceConformantPointerArray<DceNdrWstring>>* channelPaths,
        _Out_ DcePrimitiveType<uint32_t>* error_status
    ) noexcept(true)
    {
        //
        //   error_status_t EvtRpcGetChannelList(
        //      /* [in] RPC_BINDING_HANDLE binding, {the binding handle will be generated by MIDL} */
        //      [in] DWORD flags,
        //      [out] DWORD* numChannelPaths,
        //      [out, size_is(,*numChannelPaths), range(0, MAX_RPC_CHANNEL_COUNT), string]  LPWSTR** channelPaths);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *numChannelPaths = {};
        *channelPaths = {};
        *error_status = {};

        /* Serialize input. */
        iBuffer.Marshall(flags);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(19, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*numChannelPaths)
               .Unmarshall(*channelPaths)
               .Unmarshall(*error_status);
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
