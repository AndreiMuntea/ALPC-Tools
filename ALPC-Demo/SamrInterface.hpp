/**
 * @file        ALPC-Tools/ALPC-Demo/SamrInterface.hpp
 *
 * @brief       In this file we have basic functionality for Samr
 *              interface. It is fully documented here:
 *              https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-samr/1cd138b9-cc1b-4706-b115-49e53189e32e
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
  * @brief   {12345778-1234-ABCD-EF00-0123456789AC}.
  */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gSamrInterface =
{
    .SyntaxGUID = { 0x12345778, 0x1234, 0xABCD, { 0xEF, 0x00, 0x01, 0x23, 0x45, 0x67, 0x89, 0xac  } },
    .SyntaxVersion = { 1, 0 }
};

namespace AlpcRpc
{
namespace DceNdr
{
/**
 * @brief       Wrapper over RPC_UNICODE_STRING
 */
class DceRpcUnicodeString : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceRpcUnicodeString(void) noexcept(true) = default;

     /**
      * @brief  Constructor with parameters which will initialize the members as well.
      *
      * @param[in]  Buffer          - initializer for m_Buffer.
      *
      */
     DceRpcUnicodeString(
         _In_ const DceUniquePointer<DceNdrWstring>& Buffer
     ) noexcept(true): DceSerializableObject(),
                       m_Buffer{Buffer}
     {
         this->m_Length = sizeof(wchar_t) * static_cast<uint16_t>(m_Buffer.Data()->Data().Size());
         this->m_MaximumLength = this->m_Length;
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceRpcUnicodeString(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted. 
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceRpcUnicodeString, default);

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
         const uint8_t alignment = (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64) ? 8
                                                                                      : 4;
         status = Stream.AlignForSerialization(alignment);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_Length.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_MaximumLength.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         return this->m_Buffer.Marshall(Stream, LrpcTransferSyntax);
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
         const uint8_t alignment = (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64) ? 8
                                                                                      : 4;

         status = Stream.AlignForDeserialization(alignment);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_Length.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_MaximumLength.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         return this->m_Buffer.Unmarshall(Stream, LrpcTransferSyntax);
     }

     /**
      * @brief          This method takes care of transforming the underlying buffer
      *                 As a string.
      *
      * @param[in,out]  StringBuffer - will store the data.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     GetBuffer(
         _Inout_ xpf::String<wchar_t, AlpcRpc::DceNdr::DceAllocator>& StringBuffer
     ) noexcept(true)
     {
         return HelperUniqueNdrWstringToWstring(this->m_Buffer,
                                                StringBuffer);
     }

 private:
     DcePrimitiveType<uint16_t> m_Length;
     DcePrimitiveType<uint16_t> m_MaximumLength;
     DceUniquePointer<DceNdrWstring> m_Buffer;
};  // class DceRpcUnicodeString

/**
 * @brief       Wrapper over RPC_SID
 */
class DceRpcSid : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceRpcSid(void) noexcept(true) = default;

     /**
      * @brief  Constructor with parameters which will initialize the members as well.
      *
      * @param[in]  Revision            - initializer for m_Revision.
      * @param[in]  SubAuthorityCount   - initializer for m_SubAuthorityCount.
      * @param[in]  IdentifierAuthority - initializer for m_IdentifierAuthority.
      * @param[in]  SubAuthority        - initializer for m_SubAuthority.
      *
      */
     DceRpcSid(
         _In_ uint8_t Revision,
         _In_ uint8_t SubAuthorityCount,
         _In_ uint8_t IdentifierAuthority[6],
         _In_ const DcePrimitiveType<uint32_t> SubAuthority[SID_MAX_SUB_AUTHORITIES]
     ) noexcept(true): DceSerializableObject(),
                       m_Revision{ Revision },
                       m_SubAuthorityCount{ SubAuthorityCount }
     {
        for (size_t i = 0; i < XPF_ARRAYSIZE(this->m_IdentifierAuthority); ++i)
        {
            this->m_IdentifierAuthority[i] = IdentifierAuthority[i];
        }
        for (size_t i = 0; i < this->m_SubAuthorityCount.Data(); ++i)
        {
            this->m_SubAuthority[i] = SubAuthority[i];
        }
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceRpcSid(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted. 
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceRpcSid, default);

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

         /* This structure is conformant as it contains a conformant array. */
         /* We first need to serialize the conformance of the array as per doc. */
         /* We may need to do something more generic; for now this suffice. */
         DceSizeT conformance = this->m_SubAuthorityCount.Data();
         status = conformance.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         status = this->m_Revision.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_SubAuthorityCount.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         for (size_t i = 0; i < XPF_ARRAYSIZE(this->m_IdentifierAuthority); ++i)
         {
             status = this->m_IdentifierAuthority[i].Marshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         for (size_t i = 0; i < conformance.Data(); ++i)
         {
             status = this->m_SubAuthority[i].Marshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }
         return STATUS_SUCCESS;
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 Data is simply read directly from the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
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
         DceSizeT conformance;

         /* Structure need to be aligned inside the stream. */
         const uint8_t alignment = (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64) ? 8
                                                                                      : 4;
         status = Stream.AlignForDeserialization(alignment);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* This structure is conformant as it contains a conformant array. */
         status = conformance.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         if (conformance.Data() > xpf::NumericLimits<size_t>::MaxValue())
         {
             return STATUS_INVALID_BUFFER_SIZE;
         }

         status = this->m_Revision.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         status = this->m_SubAuthorityCount.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         for (size_t i = 0; i < XPF_ARRAYSIZE(this->m_IdentifierAuthority); ++i)
         {
             status = this->m_IdentifierAuthority[i].Unmarshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         for (size_t i = 0; i < static_cast<size_t>(conformance.Data()); ++i)
         {
             status = this->m_SubAuthority[i].Unmarshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }
         return STATUS_SUCCESS;
     }

 private:
     DcePrimitiveType<uint8_t> m_Revision;
     DcePrimitiveType<uint8_t> m_SubAuthorityCount;
     DcePrimitiveType<uint8_t> m_IdentifierAuthority[6];
     DcePrimitiveType<uint32_t> m_SubAuthority[SID_MAX_SUB_AUTHORITIES];
};  // class DceRpcUnicodeString

/**
 * @brief   This class contains the minimalistic functionality to make RPC calls manually via ALPC.
 *          It is not fully featured. It only contains some methods. More can be added later.
 */
class SamrInterface final
{
 private:
    /**
     * @brief  Default constructor. It is protected
     *         as the static API Connect should be used instead.
     */
     SamrInterface(void) noexcept(true) = default;
 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
     ~SamrInterface(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are deleted.
     */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(SamrInterface, delete);

    /**
     * @brief          This method is used to create a SamrInterface port.
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
        _Inout_ xpf::Optional<SamrInterface>& Port
    ) noexcept(true)
    {
        /* Create the port. */
        Port.Emplace();

        /* And now connect and bind to the actual port. First we try NDR64. */
        (*Port).m_Port.Reset();
        NTSTATUS status = AlpcRpc::RpcAlpcClientPort::Connect(gSamrInterface,
                                                              gNdr64TransferSyntaxIdentifier,
                                                              (*Port).m_Port);
        if (NT_SUCCESS(status))
        {
            return status;
        }

        /* On fail, we retry with DCE-NDR. */
        (*Port).m_Port.Reset();
        return AlpcRpc::RpcAlpcClientPort::Connect(gSamrInterface,
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
     * @brief          SamrConnect   (Opnum 0)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-samr/defe2091-0a61-4dfa-be9a-2c1206d53a1f
     *
     * @param[in]      ServerName      - The first character of the NETBIOS name of the server;
     *                                   this parameter MAY be ignored on receipt.
     * @param[out]     ServerHandle    - An RPC context handle.
     * @param[in]      DesiredAccess   - An ACCESS_MASK that indicates the access requested for ServerHandle upon output.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     *
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SamrConnect(
        _In_ const DceUniquePointer<DceNdrWstring>& ServerName,
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* ServerHandle,
        _In_ DcePrimitiveType<uint32_t> DesiredAccess,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        // long SamrConnect(
        //   [in, unique] PSAMPR_SERVER_NAME ServerName,
        //   [out] SAMPR_HANDLE* ServerHandle,
        //   [in] unsigned long DesiredAccess);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *ServerHandle = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(ServerName)
               .Marshall(DesiredAccess);
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
        oBuffer.Unmarshall(*ServerHandle)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          SamrCloseHandle    (Opnum 1)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-samr/55d134df-e257-48ad-8afa-cb2ca45cd3cc
     *
     * @param[in,out]  SamHandle       - This parameter contains an opened handle.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SamrCloseHandle(
        _Inout_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* SamHandle,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        // long SamrCloseHandle([in, out] SAMPR_HANDLE* SamHandle);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(*SamHandle);
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
        oBuffer.Unmarshall(*SamHandle)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          SamrLookupDomainInSamServer    (Opnum 5)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-samr/47492d59-e095-4398-b03e-8a062b989123
     *
     * @param[in]      ServerHandle    - This parameter contains an opened handle.
     * @param[in]      Name            - A UTF-16 encoded string.
     * @param[out]     DomainId        - A SID value of a domain that corresponds to the Name passed in.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SamrLookupDomainInSamServer(
        _In_ const DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>& ServerHandle,
        _In_ const DceRpcUnicodeString& Name,
        _Out_ DceUniquePointer<DceRpcSid>* DomainId,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //  long SamrLookupDomainInSamServer(
        //     [in] SAMPR_HANDLE ServerHandle,
        //     [in] PRPC_UNICODE_STRING Name,
        //     [out] PRPC_SID* DomainId);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *DomainId = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(ServerHandle)
               .Marshall(Name);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(5, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*DomainId)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          SamrOpenDomain    (Opnum 7)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-samr/ba710c90-5b12-42f8-9e5a-d4aacc1329fa
     *
     * @param[in]      ServerHandle    - This parameter contains an opened handle.
     * @param[in]      DesiredAccess   - An ACCESS_MASK
     * @param[in]      DomainId        - A SID value of a domain hosted by the server side of this protocol.
     * @param[out]     DomainHandle    - An RPC context handle.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SamrOpenDomain(
        _In_ const DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>& ServerHandle,
        _In_ DcePrimitiveType<uint32_t> DesiredAccess,
        _In_ const DceRpcSid& DomainId,
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* DomainHandle,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //   long SamrOpenDomain(
        //      [in] SAMPR_HANDLE ServerHandle,
        //      [in] unsigned long DesiredAccess,
        //      [in] PRPC_SID DomainId,
        //      [out] SAMPR_HANDLE* DomainHandle);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *DomainHandle = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(ServerHandle)
               .Marshall(DesiredAccess)
               .Marshall(DomainId);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(7, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*DomainHandle)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          SamrCreateUser2InDomain     (Opnum 50)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-samr/a98d7fbb-1735-4fbf-b41a-ef363c899002
     *
     * @param[in]      DomainHandle    - This parameter contains an opened handle.
     * @param[in]      Name            - The value to use as the name of the user.
     * @param[in]      AccountType     - A 32-bit value indicating the type of account to create.
     * @param[in]      DesiredAccess   - An ACCESS_MASK - (USER_ALL_ACCESS corresponds to GENERIC_ALL).
     * @param[out]     UserHandle      - An RPC context handle.
     * @param[out]     GrantedAccess   - The access granted on UserHandle.
     * @param[out]     RelativeId      - The RID of the newly created user.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    SamrCreateUser2InDomain(
        _In_ const DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>& DomainHandle,
        _In_ const DceRpcUnicodeString& Name,
        _In_ DcePrimitiveType<uint32_t> AccountType,
        _In_ DcePrimitiveType<uint32_t> DesiredAccess,
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* UserHandle,
        _Out_ DcePrimitiveType<uint32_t>* GrantedAccess,
        _Out_ DcePrimitiveType<uint32_t>* RelativeId,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //     long SamrCreateUser2InDomain(
        //          [in] SAMPR_HANDLE DomainHandle,
        //          [in] PRPC_UNICODE_STRING Name,
        //          [in] unsigned long AccountType,
        //          [in] unsigned long DesiredAccess,
        //          [out] SAMPR_HANDLE* UserHandle,
        //          [out] unsigned long* GrantedAccess,
        //          [out] unsigned long* RelativeId);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *UserHandle = {};
        *GrantedAccess = {};
        *RelativeId = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(DomainHandle)
               .Marshall(Name)
               .Marshall(AccountType)
               .Marshall(DesiredAccess);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(50, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*UserHandle)
               .Unmarshall(*GrantedAccess)
               .Unmarshall(*RelativeId)
               .Unmarshall(*retValue);
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
};  // class SamrInterface
};  // namespace DceNdr
};  // namespace AlpcRpc
