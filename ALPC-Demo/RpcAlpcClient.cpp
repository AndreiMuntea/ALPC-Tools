/**
 * @file        ALPC-Tools/ALPC-Demo/RpcAlpcClient.cpp
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

#include "precomp.hpp"
#include "RpcAlpcClient.hpp"
#include "DceNdr.hpp"

/**
 * @brief   This code will go into paged section.
 */
XPF_SECTION_PAGED;

/**
 * @brief   For simplicity use a global counter to represent a binding id.
 */
static volatile uint16_t gCrtInterfaceBinding = 0;


namespace AlpcRpc
{
namespace DceNdr
{
/**
 * @brief   This is useful to serialize tower data.
 *          It is private as it should not be used outside this module.
 */
class DceNdrEpmTower final : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceNdrEpmTower(void) noexcept(true) = default;

     /**
      * @brief          Used to initialize an object using provided data.
      *
      * @param[in]      TowerSize   - The size of the tower, in bytes
      * @param[in]      Tower       - The tower to be stored as bytes.
      */
     DceNdrEpmTower(
         _In_ _Const_ uint32_t TowerSize,
         _In_ _Const_ const LRPC_EPM_TOWER& Tower
     ) noexcept(true) : DceSerializableObject()
     {
         XPF_MAX_PASSIVE_LEVEL();

         xpf::Vector<DcePrimitiveType<uint8_t>, DceAllocator> bytesTower;
         const uint8_t* bytes = reinterpret_cast<const uint8_t*>(xpf::AddressOf(Tower));

         for (uint32_t i = 0; i < TowerSize; ++i)
         {
             NTSTATUS status = bytesTower.Emplace(bytes[i]);
             if (!NT_SUCCESS(status))
             {
                 return;
             }
         }

         this->m_Tower = xpf::MakeShared<xpf::Vector<DcePrimitiveType<uint8_t>, DceAllocator>,
                                         DceAllocator>(xpf::Move(bytesTower));
         this->m_TowerSize = TowerSize;
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceNdrEpmTower(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceNdrEpmTower, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
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
         XPF_MAX_PASSIVE_LEVEL();

         NTSTATUS status = this->m_TowerSize.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         return this->m_Tower.Marshall(Stream, LrpcTransferSyntax);
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 First the referent is read. If it is null, we are done.
      *                 Otherwise, we also read the data.
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
         XPF_MAX_PASSIVE_LEVEL();

         NTSTATUS status = this->m_TowerSize.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         return this->m_Tower.Unmarshall(Stream, LrpcTransferSyntax);
     }

     /**
      * @brief      Getter for the underlying tower endpoint.
      *
      * @return     The name of the endpoint represented by this tower.
      */
     inline xpf::String<wchar_t> XPF_API
     TowerEndpoint(
         void
     ) const noexcept(true)
     {
         XPF_MAX_PASSIVE_LEVEL();

         xpf::Vector<uint8_t> rawData;
         xpf::String<wchar_t> endpoint;

         LRPC_EPM_TOWER* tower = nullptr;
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         for (size_t i = 0; i < this->m_Tower.Data().Size(); ++i)
         {
             const DcePrimitiveType<uint8_t> byte = this->m_Tower.Data()[i];
             status = rawData.Emplace(byte.Data());
             if (!NT_SUCCESS(status))
             {
                 rawData.Clear();
                 break;
             }
         }

         if (!rawData.IsEmpty())
         {
             tower = reinterpret_cast<LRPC_EPM_TOWER*>(xpf::AddressOf(rawData[0]));
             xpf::String<char> ansiEndpoint;

             status = ansiEndpoint.Append("\\RPC Control\\");
             if (!NT_SUCCESS(status))
             {
                 return endpoint;
             }

             status = ansiEndpoint.Append(xpf::StringView<char>(tower->Floor4.EndpointName,
                                                                tower->Floor4.RhsByteCount));
             if (!NT_SUCCESS(status))
             {
                 return endpoint;
             }

             status = xpf::StringConversion::UTF8ToWide(ansiEndpoint.View(),
                                                        endpoint);
             if (!NT_SUCCESS(status))
             {
                 endpoint.Reset();
                 return endpoint;
             }
         }

         return endpoint;
     }

 private:
     DcePrimitiveType<uint32_t> m_TowerSize = 0;
     DceConformantArray<DcePrimitiveType<uint8_t>> m_Tower;
};  // class DceNdrTower


/**
 * @brief           Binds an ALPC port to a specific interface.
 *
 * @param[in,out]   Port                    - an already connected ALPC-Port.
 * @param[in]       Interface               - the interface where the port must be binded to.
 * @param[in]       TransferSyntaxFlags     - one of the values of LRPC_TRANSFER_SYNTAX_*
 * @param[out]      BindId                  - an unique indentifier which represents the binding of port-interface.
 *
 * @return          An NTSTATUS error code.
 *
 * @note            This is not the most efficient approach, as it drops the ALPC buffer and captures a copy.
 *                  We can definitely make some improvements here. For now, leave it as is.
 */
_Must_inspect_result_
static NTSTATUS
BindToInterface(
    _Inout_ AlpcRpc::AlpcPort& Port,
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& Interface,
    _In_ uint32_t TransferSyntaxFlags,
    _Out_ uint16_t& BindId
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    LRPC_BIND_MESSAGE bindMessageReq = { 0 };
    LRPC_BIND_MESSAGE bindMessageAns = { 0 };

    xpf::Buffer<xpf::MemoryAllocator> output;
    xpf::StreamReader outputStream{ output };

    xpf::Buffer<xpf::MemoryAllocator> viewOutput;

    BindId = xpf::ApiAtomicIncrement(&gCrtInterfaceBinding);

    //
    // Select the transfer syntax.
    //
    bindMessageReq.MessageType = LRPC_MESSAGE_TYPE::lmtBind;
    bindMessageReq.Interface = Interface;

    bindMessageReq.TransferSyntaxFlags = TransferSyntaxFlags;
    if (LRPC_TRANSFER_SYNTAX_DCE == TransferSyntaxFlags)
    {
        bindMessageReq.DceNdrSyntaxBindIdentifier = BindId;
    }
    else if (LRPC_TRANSFER_SYNTAX_NDR64 == TransferSyntaxFlags)
    {
        bindMessageReq.Ndr64SyntaxBindIdentifier = BindId;
    }
    else
    {
        return STATUS_NOINTERFACE;
    }

    //
    // Send the bind message.
    //
    status = Port.SendReceive(&bindMessageReq,
                              sizeof(bindMessageReq),
                              output,
                              viewOutput);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Validate the response. It should be large enough to contain a bind message.
    //
    if (!outputStream.ReadBytes(sizeof(bindMessageAns), reinterpret_cast<uint8_t*>(&bindMessageAns)))
    {
        return STATUS_INVALID_MESSAGE;
    }
    if (bindMessageAns.MessageType != LRPC_MESSAGE_TYPE::lmtBind)
    {
        return STATUS_INVALID_MESSAGE;
    }
    return (bindMessageAns.BindingStatus == STATUS_SUCCESS) ? STATUS_SUCCESS
                                                            : STATUS_NOINTERFACE;
}

/**
 * @brief           Calls a method from an already bounded port.
 *
 * @param[in,out]   Port                    - an already connected ALPC-Port.
 * @param[in]       BindId                  - the identifier returned by BindToInterface.
 * @param[in]       InterfaceGuid           - the interface where we want to bind to.
 * @param[in]       ProcNum                 - procedure number in the interface.
 * @param[in]       MarshallBuffer          - buffer containing input parameters serialized in NDR format.
 * @param[in,out]   UnmarshallBuffer        - buffer containing output parameters serialized in NDR format.
 *
 * @return          An NTSTATUS error code.
 *
 * @note            This is not the most efficient approach, as it drops the ALPC buffer and captures a copy.
 *                  We can definitely make some improvements here. For now, leave it as is.
 */
_Must_inspect_result_
static NTSTATUS
CallMethod(
    _Inout_ AlpcRpc::AlpcPort& Port,
    _In_ uint16_t BindId,
    _In_ GUID InterfaceGuid,
    _In_ uint16_t ProcNum,
    _In_ _Const_ const AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _Inout_  AlpcRpc::DceNdr::DceMarshallBuffer& UnmarshallBuffer
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    LRPC_REQUEST_MESSAGE reqMessage = { 0 };
    LRPC_RESPONSE_MESSAGE ansMessage = { 0 };
    LRPC_FAULT_MESSAGE faultMessage = { 0 };

    xpf::Buffer<xpf::MemoryAllocator> requestBuffer;
    xpf::StreamWriter requestBufferWriter{ requestBuffer };
    size_t requestSize = 0;

    xpf::Buffer<xpf::MemoryAllocator> responseBuffer;
    xpf::StreamReader responseBufferReader{ responseBuffer };

    xpf::Buffer<xpf::MemoryAllocator> viewResponseBuffer;
    xpf::StreamReader viewResponseBufferReader{ viewResponseBuffer };

    //
    // Compute required size.
    //
    status = xpf::ApiNumbersSafeAdd(sizeof(reqMessage),
                                    MarshallBuffer.Buffer().GetSize(),
                                    &requestSize);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = requestBuffer.Resize(requestSize);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Prepare the request. Use a dummy call identifier.
    // This will be used to validate the response.
    //
    reqMessage.MessageType = LRPC_MESSAGE_TYPE::lmtRequest;
    reqMessage.Flags = LRPC_REQUEST_FLAG_UUID_SPECIFIED;
    reqMessage.Uuid = InterfaceGuid;
    reqMessage.BindingId = BindId;
    reqMessage.Procnum = ProcNum;
    reqMessage.CallId = 0xDEADC0DE;

    if (!requestBufferWriter.WriteBytes(sizeof(reqMessage), reinterpret_cast<const uint8_t*>(&reqMessage)))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    if (MarshallBuffer.Buffer().GetSize() != 0)
    {
        if (!requestBufferWriter.WriteBytes(MarshallBuffer.Buffer().GetSize(),
                                            reinterpret_cast<const uint8_t*>(MarshallBuffer.Buffer().GetBuffer())))
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // Sent the request.
    //
    status = Port.SendReceive(requestBuffer.GetBuffer(),
                              requestBuffer.GetSize(),
                              responseBuffer,
                              viewResponseBuffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Validate the response. It should be large enough to contain the response message.
    // For now we do not support views. We can easily adjust when the need arise.
    //
    if (!responseBufferReader.ReadBytes(sizeof(ansMessage), reinterpret_cast<uint8_t*>(&ansMessage)))
    {
        if (!responseBufferReader.ReadBytes(sizeof(faultMessage), reinterpret_cast<uint8_t*>(&faultMessage)))
        {
            return STATUS_INVALID_MESSAGE;
        }
        if (faultMessage.MessageType != LRPC_MESSAGE_TYPE::lmtFault)
        {
            return STATUS_INVALID_MESSAGE;
        }
        return NTSTATUS_FROM_WIN32(faultMessage.RpcStatus);
    }
    if (ansMessage.MessageType != LRPC_MESSAGE_TYPE::lmtResponse)
    {
        return STATUS_INVALID_MESSAGE;
    }
    if (ansMessage.CallId != reqMessage.CallId)
    {
        return STATUS_INVALID_MESSAGE;
    }

    //
    // And now capture the output - we have two cases - when the output is in a view,
    // and when it is continous memory.
    //
    xpf::Buffer<xpf::MemoryAllocator> ndrOutParameters;

    if (ansMessage.Flags & LRPC_RESPONSE_FLAG_VIEW_PRESENT)
    {
        status = ndrOutParameters.Resize(viewResponseBuffer.GetSize());
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        if (!viewResponseBufferReader.ReadBytes(ndrOutParameters.GetSize(),
                                                reinterpret_cast<uint8_t*>(ndrOutParameters.GetBuffer())))
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
    {
        status = ndrOutParameters.Resize(responseBuffer.GetSize() - sizeof(ansMessage));
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        if (!responseBufferReader.ReadBytes(ndrOutParameters.GetSize(),
                                            reinterpret_cast<uint8_t*>(ndrOutParameters.GetBuffer())))
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }


    //
    // All good - setup unmarshall buffer.
    //
    UnmarshallBuffer.MarshallRawBuffer(ndrOutParameters);
    return UnmarshallBuffer.Status();
}

/**
 * @brief           Finds the underlying alpc port coresponding to a given interface,
 *                  and attempts to connect to it.
 *
 * @param[in]       ObjectIdentifier        - GUID and SYNTAX version of the interface we want to connect to.
 * @param[in]       TransferSyntaxFlags     - one of the values of LRPC_TRANSFER_SYNTAX_*
 * @param[out]      ConnectedPort           - Will be a connected port on success.
 * @param[out]      BindId                  - an unique indentifier which represents the binding of port-interface.
 *
 * @return          An NTSTATUS error code.
 */
_Must_inspect_result_
static NTSTATUS
FindEndpointAndConnect(
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& ObjectIdentifier,
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& TransferSyntaxFlags,
    _Inout_ xpf::Optional<AlpcRpc::AlpcPort>& ConnectedPort,
    _Inout_ uint16_t& BindId
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    xpf::Optional<AlpcRpc::AlpcPort> epMapperPort;
    uint16_t epMapperBinding = 0;

    LRPC_EPM_TOWER epmTower = { 0 };
    uint32_t epmTowerSize = 0;

    //
    // Map tranfesr syntax to flags. For epmapper we always use DCE.
    // This is available on both x86 and x64.
    // It is used only for dynamic port name discovery.
    //
    AlpcRpc::DceNdr::DceMarshallBuffer marshallBuffer(LRPC_TRANSFER_SYNTAX_DCE);
    AlpcRpc::DceNdr::DceMarshallBuffer unmarshallBuffer(LRPC_TRANSFER_SYNTAX_DCE);

    BindId = 0;
    ConnectedPort.Reset();

    //
    // First step -> connect to endpoint mapper and bind to its interface.
    //
    status = AlpcRpc::AlpcPort::Connect(gEpmapperPortName, epMapperPort);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = AlpcRpc::DceNdr::BindToInterface(*epMapperPort,
                                               gEpmapperInterface,
                                               LRPC_TRANSFER_SYNTAX_DCE,
                                               epMapperBinding);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Now construct the epmapper tower.
    //
    epmTower.FloorCount = 4;

    epmTower.Floor1.LhsByteCount = sizeof(UINT8) + sizeof(GUID) + sizeof(UINT16);
    epmTower.Floor1.ProtocolId = EPM_PROTOCOL_UUID_DERIVED;
    epmTower.Floor1.Guid = ObjectIdentifier.SyntaxGUID;
    epmTower.Floor1.MajorVersion = ObjectIdentifier.SyntaxVersion.MajorVersion;
    epmTower.Floor1.RhsByteCount = sizeof(UINT16);
    epmTower.Floor1.MinorVersion = ObjectIdentifier.SyntaxVersion.MinorVersion;
    epmTowerSize += sizeof(epmTower.Floor1);

    epmTower.Floor2.LhsByteCount = sizeof(UINT8) + sizeof(GUID) + sizeof(UINT16);
    epmTower.Floor2.ProtocolId = EPM_PROTOCOL_UUID_DERIVED;
    epmTower.Floor2.Guid = TransferSyntaxFlags.SyntaxGUID;
    epmTower.Floor2.MajorVersion = TransferSyntaxFlags.SyntaxVersion.MajorVersion;
    epmTower.Floor2.RhsByteCount = sizeof(UINT16);
    epmTower.Floor2.MinorVersion = TransferSyntaxFlags.SyntaxVersion.MinorVersion;
    epmTowerSize += sizeof(epmTower.Floor2);

    epmTower.Floor3.LhsByteCount = sizeof(UINT8);
    epmTower.Floor3.ProtocolId = EPM_PROTOCOL_NCALRPC;
    epmTower.Floor3.RhsByteCount = sizeof(UINT16);
    epmTower.Floor3.Reserved = 0;
    epmTowerSize += sizeof(epmTower.Floor3);

    epmTower.Floor4.LhsByteCount = sizeof(UINT8);
    epmTower.Floor4.ProtocolId = EPM_PROTOCOL_NAMED_PIPE;
    epmTower.Floor4.RhsByteCount = 0;
    epmTower.Floor4.EndpointName[0] = '\0';
    epmTower.Floor4.EndpointName[1] = '\0';
    epmTowerSize += sizeof(epmTower.Floor4);

    //
    // Now we marshall the parameters for "ept_map"
    //  void ept_map(
    //   [in] handle_t hEpMapper,
    //   [in, ptr] UUID* obj,
    //   [in, ptr] twr_p_t map_tower,
    //   [in, out] ept_lookup_handle_t* entry_handle,
    //   [in, range(0,500)] unsigned long max_towers,
    //   [out] unsigned long* num_towers,
    //   [out, ptr, size_is(max_towers), length_is(*num_towers)] twr_p_t* ITowers,
    //   [out] error_status* status
    // );
    //

    /* hEpMapper is not required as we are doing the call manually by connecting to alpc port. */
    DceUniquePointer<DcePrimitiveType<GUID>> obj{ ObjectIdentifier.SyntaxGUID };
    DceUniquePointer<DceNdrEpmTower> map_tower{ xpf::Move(DceNdrEpmTower{epmTowerSize, epmTower}) };
    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> entry_handle;
    DcePrimitiveType<uint32_t> max_towers = 1;
    DcePrimitiveType<uint32_t> num_towers = 0;
    DceConformantVaryingPointerArray<DceNdrEpmTower> ITowers;
    DcePrimitiveType<NTSTATUS> error_status;

    //
    // Marshall input parameters
    //
    marshallBuffer.Marshall(obj)
                  .Marshall(map_tower)
                  .Marshall(entry_handle)
                  .Marshall(max_towers);
    status = marshallBuffer.Status();
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Call the method.
    //
    status = AlpcRpc::DceNdr::CallMethod(*epMapperPort,
                                         epMapperBinding,
                                         ObjectIdentifier.SyntaxGUID,
                                         0x3,
                                         marshallBuffer,
                                         unmarshallBuffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Deserialize output parameters.
    //
    unmarshallBuffer.Unmarshall(entry_handle)
                    .Unmarshall(num_towers)
                    .Unmarshall(ITowers)
                    .Unmarshall(error_status);
    if (!NT_SUCCESS(unmarshallBuffer.Status()))
    {
        return unmarshallBuffer.Status();
    }
    if (error_status.Data() != STATUS_SUCCESS)
    {
        return STATUS_FAIL_CHECK;
    }

    //
    // Now we bind to the port with the specified guid and we use the proper transfer syntax.
    //

    uint32_t transferSyntaxFlags = ULONG_MAX;
    if (TransferSyntaxFlags.SyntaxGUID == gDceNdrTransferSyntaxIdentifier.SyntaxGUID)
    {
        transferSyntaxFlags = LRPC_TRANSFER_SYNTAX_DCE;
    }
    else if (TransferSyntaxFlags.SyntaxGUID == gNdr64TransferSyntaxIdentifier.SyntaxGUID)
    {
        transferSyntaxFlags = LRPC_TRANSFER_SYNTAX_NDR64;
    }

    //
    // Now we retrieved the potential endpoints. Let's attempt connection to each of them.
    // We just need the first one.
    //
    for (size_t i = 0; i < ITowers.Data().Size(); ++i)
    {
        ConnectedPort.Reset();
        BindId = 0;

        const auto& crtTower = ITowers.Data()[i];
        const auto towerEndpoint = crtTower.Data()->TowerEndpoint();

        status = AlpcRpc::AlpcPort::Connect(towerEndpoint.View(), ConnectedPort);
        if (!NT_SUCCESS(status))
        {
            continue;
        }
        status = AlpcRpc::DceNdr::BindToInterface(*ConnectedPort,
                                                   ObjectIdentifier,
                                                   transferSyntaxFlags,
                                                   BindId);
        if (!NT_SUCCESS(status))
        {
            continue;
        }

        return STATUS_SUCCESS;
    }

    //
    // If we are here, we did not manage to connect to any endpoint.
    //
    return STATUS_CONNECTION_REFUSED;
}
};  // namespace DceNdr
};  // namespace AlpcRpc

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::HelperWstringToNdr(
    _In_ _Const_ const xpf::StringView<wchar_t>& View,
    _Inout_ AlpcRpc::DceNdr::DceNdrWstring& NdrString,
    _In_ bool NullTerminateString
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* First we emplace all characters in a DcePrimitiveType wchar_t array. */
    xpf::Vector<AlpcRpc::DceNdr::DcePrimitiveType<wchar_t>,
                AlpcRpc::DceNdr::DceAllocator> chars;
    for (size_t i = 0; i < View.BufferSize(); ++i)
    {
        status = chars.Emplace(View[i]);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    /* Then a NULL to be null terminated. */
    if (NullTerminateString)
    {
        status = chars.Emplace(L'\0');
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    /* Then we make a shared pointer. */
    auto stringPtr = xpf::MakeShared<xpf::Vector<AlpcRpc::DceNdr::DcePrimitiveType<wchar_t>, AlpcRpc::DceNdr::DceAllocator>,
                                     AlpcRpc::DceNdr::DceAllocator>(xpf::Move(chars));
    if (stringPtr.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* And now convert to NdrString. */
    NdrString = stringPtr;
    if (NdrString.Data().IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* All good. */
    return STATUS_SUCCESS;
}

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::HelperWstringToUniqueNdr(
    _In_ _Const_ const xpf::StringView<wchar_t>& View,
    _Inout_ AlpcRpc::DceNdr::DceUniquePointer<AlpcRpc::DceNdr::DceNdrWstring>& NdrUniqueString,
    _In_ bool NullTerminateString
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    AlpcRpc::DceNdr::DceNdrWstring ndrString;
    xpf::SharedPointer<AlpcRpc::DceNdr::DceNdrWstring, AlpcRpc::DceNdr::DceAllocator> uniqueNdrString;

    status = AlpcRpc::HelperWstringToNdr(View, ndrString, NullTerminateString);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    uniqueNdrString = xpf::MakeShared<AlpcRpc::DceNdr::DceNdrWstring,
                                      AlpcRpc::DceNdr::DceAllocator>(xpf::Move(ndrString));
    if (uniqueNdrString.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NdrUniqueString = uniqueNdrString;
    return STATUS_SUCCESS;
}

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::RpcAlpcClientPort::Connect(
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& ObjectIdentifier,
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& TransferSyntaxFlags,
    _Inout_ xpf::Optional<AlpcRpc::RpcAlpcClientPort>& Port
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    uint32_t transferSyntaxFlags = 0;

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
    AlpcRpc::RpcAlpcClientPort& port = (*Port);

    //
    // Map transfer syntax flags.
    //
    if (TransferSyntaxFlags.SyntaxGUID == gDceNdrTransferSyntaxIdentifier.SyntaxGUID)
    {
        transferSyntaxFlags = LRPC_TRANSFER_SYNTAX_DCE;
    }
    else if (TransferSyntaxFlags.SyntaxGUID == gNdr64TransferSyntaxIdentifier.SyntaxGUID)
    {
        transferSyntaxFlags = LRPC_TRANSFER_SYNTAX_NDR64;
    }

    //
    // Now find the endpoint to connect to.
    //
    status = AlpcRpc::DceNdr::FindEndpointAndConnect(ObjectIdentifier,
                                                     TransferSyntaxFlags,
                                                     port.m_AlpcPort,
                                                     port.m_BindingId);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // All good.
    //
    port.m_ObjectIdentifier = ObjectIdentifier;
    port.m_TransferSyntax = TransferSyntaxFlags;
    port.m_TransferSyntaxFlags = transferSyntaxFlags;
    return STATUS_SUCCESS;
}

_Must_inspect_result_
NTSTATUS XPF_API
AlpcRpc::RpcAlpcClientPort::Connect(
    _In_ _Const_ const xpf::StringView<wchar_t>& PortName,
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& ObjectIdentifier,
    _In_ _Const_ const ALPC_RPC_SYNTAX_IDENTIFIER& TransferSyntaxFlags,
    _Inout_ xpf::Optional<AlpcRpc::RpcAlpcClientPort>& Port
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    uint32_t transferSyntaxFlags = 0;

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
    AlpcRpc::RpcAlpcClientPort& port = (*Port);

    if (TransferSyntaxFlags.SyntaxGUID == gDceNdrTransferSyntaxIdentifier.SyntaxGUID)
    {
        transferSyntaxFlags = LRPC_TRANSFER_SYNTAX_DCE;
    }
    else if (TransferSyntaxFlags.SyntaxGUID == gNdr64TransferSyntaxIdentifier.SyntaxGUID)
    {
        transferSyntaxFlags = LRPC_TRANSFER_SYNTAX_NDR64;
    }

    //
    // Now find the endpoint to connect to.
    //
    status = AlpcRpc::AlpcPort::Connect(PortName, port.m_AlpcPort);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = AlpcRpc::DceNdr::BindToInterface(*port.m_AlpcPort,
                                               ObjectIdentifier,
                                               transferSyntaxFlags,
                                               port.m_BindingId);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // All good.
    //
    port.m_ObjectIdentifier = ObjectIdentifier;
    port.m_TransferSyntax = TransferSyntaxFlags;
    port.m_TransferSyntaxFlags = transferSyntaxFlags;
    return STATUS_SUCCESS;
}


_Must_inspect_result_
NTSTATUS
AlpcRpc::RpcAlpcClientPort::CallProcedure(
    _In_ uint16_t ProcNum,
    _In_ _Const_ const AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _Inout_  AlpcRpc::DceNdr::DceMarshallBuffer& UnmarshallBuffer
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    return AlpcRpc::DceNdr::CallMethod((*this->m_AlpcPort),
                                       this->m_BindingId,
                                       this->m_ObjectIdentifier.SyntaxGUID,
                                       ProcNum,
                                       MarshallBuffer,
                                       UnmarshallBuffer);
}
