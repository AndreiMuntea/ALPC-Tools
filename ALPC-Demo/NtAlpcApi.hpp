#pragma once

//
// This file does not have doxygen.
//
#ifndef DOXYGEN_SHOULD_SKIP_THIS


// *****************************************************************************************************************************
// * Shout out to open source project system informer https://github.com/winsiderss/systeminformer where we found APIs defs.   *
// * There are however some mistakes for x64 -- the BufferLength attribute is marked as "ULONG" (4 bytes)                      *
// * However, on x64, this is used as 8 bytes. For example:                                                                    *
// *      ntdll!AlpcInitializeMessageAttribute:                                                                                *
// *      00007ffc`98b4e740 48895c2408       mov     qword ptr [rsp+8], rbx                                                    *
// *      00007ffc`98b4e745 48896c2410       mov     qword ptr [rsp+10h], rbp                                                  *
// *      00007ffc`98b4e74a 4889742418       mov     qword ptr [rsp+18h], rsi                                                  *
// *      00007ffc`98b4e74f 57               push    rdi                                                                       *
// *      00007ffc`98b4e750 4883ec20         sub     rsp, 20h                                                                  *
// *      00007ffc`98b4e754 498bd9           mov     rbx, r9               <- RequiredBufferSize (r9 -> rbx)                   *
// * Which leads to a buffer overflow corruption. The fix was to re-define as SIZE_T instead.                                  *
// * The definitions here have been tested on both x86 and x64. And they seem to work.                                         *
// *                                                                                                                           *
// * Another good resource we used was https://recon.cx/2008/a/thomas_garnier/LPC-ALPC-paper.pdf                               *
// * Some of the definitions are taken from there.                                                                             *
// *                                                                                                                           *
// * Update: Once the terminal project got published on M$ github, we managed to cross-check some of these:                    *
// * https://github.com/microsoft/terminal/blob/6b29ef51e304fb1af8df7c798f87d3dbb0408c92/dep/Console/ntlpcapi.h#L26            *
// *****************************************************************************************************************************


//
// We want to avoid name mangling. So all these will be
// encapsulated in extern C definitions. This is the start marker.
//
XPF_EXTERN_C_START();



//
// Link with ntdll.lib for the below definitions.
//
#if defined XPF_COMPILER_MSVC
    #if defined XPF_PLATFORM_WIN_UM
        #pragma comment(lib, "ntdll.lib")
    #endif  // XPF_PLATFORM_WIN_UM
#endif  // XPF_COMPILER_MSVC


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       STRUCTURES DEFINITIONS                                                    |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


///
/// x86:
/// 0: kd> dt nt!_ALPC_PORT_ATTRIBUTES
///             +0x000 Flags            : Uint4B
///             +0x004 SecurityQos      : _SECURITY_QUALITY_OF_SERVICE
///             +0x010 MaxMessageLength : Uint4B
///             +0x014 MemoryBandwidth  : Uint4B
///             +0x018 MaxPoolUsage     : Uint4B
///             +0x01c MaxSectionSize   : Uint4B
///             +0x020 MaxViewSize      : Uint4B
///             +0x024 MaxTotalSectionSize : Uint4B
///             +0x028 DupObjectTypes   : Uint4B
///
/// x64:
/// 1: kd> dt nt!_ALPC_PORT_ATTRIBUTES
///             +0x000 Flags            : Uint4B
///             +0x004 SecurityQos      : _SECURITY_QUALITY_OF_SERVICE
///             +0x010 MaxMessageLength : Uint8B
///             +0x018 MemoryBandwidth  : Uint8B
///             +0x020 MaxPoolUsage     : Uint8B
///             +0x028 MaxSectionSize   : Uint8B
///             +0x030 MaxViewSize      : Uint8B
///             +0x038 MaxTotalSectionSize : Uint8B
///             +0x040 DupObjectTypes   : Uint4B
///             +0x044 Reserved         : Uint4B
///

typedef struct _ALPC_PORT_ATTRIBUTES
{
    UINT32                          Flags;
    SECURITY_QUALITY_OF_SERVICE     SecurityQos;
    SIZE_T                          MaxMessageLength;
    SIZE_T                          MemoryBandwidth;
    SIZE_T                          MaxPoolUsage;
    SIZE_T                          MaxSectionSize;
    SIZE_T                          MaxViewSize;
    SIZE_T                          MaxTotalSectionSize;
    UINT32                          DupObjectTypes;

#if defined _M_AMD64
    UINT32                          Reserved;
#endif  // _M_AMD64
} ALPC_PORT_ATTRIBUTES;

///
/// x86:
/// 0: kd> dt -r2 nt!_PORT_MESSAGE
///                 +0x000 u1               : <unnamed-tag>
///                    +0x000 s1               : <unnamed-tag>
///                       +0x000 DataLength       : Int2B
///                       +0x002 TotalLength      : Int2B
///                    +0x000 Length           : Uint4B
///                 +0x004 u2               : <unnamed-tag>
///                    +0x000 s2               : <unnamed-tag>
///                       +0x000 Type             : Int2B
///                       +0x002 DataInfoOffset   : Int2B
///                    +0x000 ZeroInit         : Uint4B
///                 +0x008 ClientId         : _CLIENT_ID
///                    +0x000 UniqueProcess    : Ptr32 Void
///                    +0x004 UniqueThread     : Ptr32 Void
///                 +0x008 DoNotUseThisField : Float
///                 +0x010 MessageId        : Uint4B
///                 +0x014 ClientViewSize   : Uint4B
///                 +0x014 CallbackId       : Uint4B
///
/// x64:
/// 1: kd> dt -r2 nt!_PORT_MESSAGE
///                 +0x000 u1               : <anonymous-tag>
///                    +0x000 s1               : <anonymous-tag>
///                       +0x000 DataLength       : Int2B
///                       +0x002 TotalLength      : Int2B
///                    +0x000 Length           : Uint4B
///                 +0x004 u2               : <anonymous-tag>
///                    +0x000 s2               : <anonymous-tag>
///                       +0x000 Type             : Int2B
///                       +0x002 DataInfoOffset   : Int2B
///                    +0x000 ZeroInit         : Uint4B
///                 +0x008 ClientId         : _CLIENT_ID
///                    +0x000 UniqueProcess    : Ptr64 Void
///                    +0x008 UniqueThread     : Ptr64 Void
///                 +0x008 DoNotUseThisField : Float
///                 +0x018 MessageId        : Uint4B
///                 +0x020 ClientViewSize   : Uint8B
///                 +0x020 CallbackId       : Uint4B
///

typedef struct _PORT_MESSAGE
{
    union
    {
        struct
        {
            UINT16    DataLength;
            UINT16    TotalLength;
        } s1;
        UINT32        Length;
    } u1;

    union
    {
        struct
        {
            UINT16    Type;
            UINT16    DataInfoOffset;
        } s2;
        UINT32        ZeroInit;
    } u2;

    /* DoNotUseThisField is actually an union with this as it has the same offset. */
    /* We don't want to use a float type in KM. We're ok with this one only. */
    CLIENT_ID         ClientId;

    UINT32            MessageId;
    union
    {
        SIZE_T            ClientViewSize;
        UINT32            CallbackId;
    };
} PORT_MESSAGE;

///
/// x86:
/// 0: kd> dt nt!_ALPC_MESSAGE_ATTRIBUTES
///             +0x000 AllocatedAttributes : Uint4B
///             +0x004 ValidAttributes  : Uint4B
///
/// x64:
/// 1: kd> dt nt!_ALPC_MESSAGE_ATTRIBUTES
///             +0x000 AllocatedAttributes : Uint4B
///             +0x004 ValidAttributes  : Uint4B
///

typedef struct _ALPC_MESSAGE_ATTRIBUTES
{
    UINT32    AllocatedAttributes;
    UINT32    ValidAttributes;
} ALPC_MESSAGE_ATTRIBUTES;

///
/// This actually was found in combase.dll
///
/// 0:000> dt  combase!_ALPC_DATA_VIEW_ATTR
///                   +0x000 Flags            : Uint4B
///                   +0x008 SectionHandle    : Ptr64 Void
///                   +0x010 ViewBase         : Ptr64 Void
///                   +0x018 ViewSize         : Uint8B
///

typedef struct _ALPC_DATA_VIEW_ATTR
{
    UINT32  Flags;
    HANDLE  SectionHandle;
    PVOID   ViewBase;
    SIZE_T  ViewSize;
} ALPC_DATA_VIEW_ATTR;


///
/// This is actually documented. But we don't want to have a rpcrt dependency.
/// So simply redefine here. For documentation please consult:
/// https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rpcl/1831bd1c-738c-45dc-a2af-5d0b835af6f5
///

typedef struct _ALPC_RPC_VERSION
{
    UINT16 MajorVersion;
    UINT16 MinorVersion;
} ALPC_RPC_VERSION;

typedef struct _ALPC_RPC_SYNTAX_IDENTIFIER
{
    GUID SyntaxGUID;
    ALPC_RPC_VERSION SyntaxVersion;
} ALPC_RPC_SYNTAX_IDENTIFIER;

typedef struct _ALPC_RPC_CONTEXT_HANDLE
{
    DWORD Attributes;
    GUID Guid;
} ALPC_RPC_CONTEXT_HANDLE;

///
/// From Windows 8.1 checked build.
/// Search rpcrt4 strings for "lmt"
///

typedef enum _LRPC_MESSAGE_TYPE
{
    lmtRequest = 0,
    lmtBind = 1,
    lmtFault = 2,
    lmtResponse = 3,
} LRPC_MESSAGE_TYPE;


///
/// From reverse engineering rpcrt4.dll
///

#define LRPC_TRANSFER_SYNTAX_DCE        1
#define LRPC_TRANSFER_SYNTAX_NDR64      2
#define LRPC_TRANSFER_SYNTAX_TEST       4

typedef struct _LRPC_BIND_MESSAGE
{
    /* Actually this starts with a PORT_MESSAGE header. */
    /* We won't define it here as our framework takes care of this. */


    /* Has the LRPC_MESSAGE_TYPE possible values. */
    UINT64  MessageType;

    /*
     * LRPC_ADDRESS::HandleBindRequest:
     *
     *    status = LRPC_SASSOCIATION::AddBinding((__int64)Association, BindMessage);
     *    BindMessage->BindingStatus = status;
     * Holds the return value of AddBinding();
     *
     * Must be inspected by the caller to ensure the binding was successful.
     */
    UINT32  BindingStatus;

    /* The interface where we try to bind to. */
    ALPC_RPC_SYNTAX_IDENTIFIER Interface;

    /*
     * 1 for DCE_NDR               - 8A885D04-1CEB-11C9-9FE8-08002B104860
     * 2 for NDR64                 - 71710533-BEBA-4937-8319-B5DBEF9CCC36
     * 4 for a test interface      - B4537DA9-3D03-4F6B-B594-52B2874EE9D0
     * 
     *  RtlAssert("(RpcpMemoryCompare(Message->TransferSyntax, NDR20TransferSyntax,   sizeof(RPC_SYNTAX_IDENTIFIER)) == 0) ||
     *             (RpcpMemoryCompare(Message->TransferSyntax, NDR64TransferSyntax,   sizeof(RPC_SYNTAX_IDENTIFIER)) == 0) ||
     *             (RpcpMemoryCompare(Message->TransferSyntax, NDRTestTransferSyntax, sizeof(RPC_SYNTAX_IDENTIFIER)) == 0)")
     */
    UINT32  TransferSyntaxFlags;

    /* Used when TransferSyntaxFlags is 1. Can be an atomic variable incrementing independently. */
    UINT16  DceNdrSyntaxBindIdentifier;

    /* Used when TransferSyntaxFlags is 2. Can be an atomic variable incrementing independently. */
    UINT16  Ndr64SyntaxBindIdentifier;

    /* Used when TransferSyntaxFlags is 4. Can be an atomic variable incrementing independently. */
    UINT16  TestSyntaxBindIdentifier;

    /*
     * TransferSyntaxFlags 2 and 4 can be specified at the same time. In this case, SupportsMultipleSyntaxes should be 1.
     * TransferSyntaxFlags 1 must be specified independently. In this case SupportsMultipleSyntaxes should be 0.
     */
    UINT32  SupportsMultipleSyntaxes;

    /*
     *  Name was inferred from the following call:
     *  if (BindMessage->SupportsCausalFlowId &&
     *      !LRPC_SASSOCIATION::FindOrCreateCausalFlow(this, BindMessage->CausalFlowId, ...))
     *
     * Can be safely set to 0 for ncalrpc.
     */
    UINT32  SupportsCausalFlowId;
    UINT64  CausalFlowId;

    /*
     * Unknown, but is assigned to Association. So named as "AssociationData".
     * *((_DWORD *)Association + 0xA8) = BindMessage->AssociationData;
     *
     * Can be safely set to 0 for ncalrpc.
     */
    UINT32  AssociationData;
} LRPC_BIND_MESSAGE;


///
/// From Windows 8.1 checked build. Reverse engineering.
/// Search rpcrt4 strings for LRPC_REQUEST_FLAG.
///

#define LRPC_REQUEST_FLAG_UUID_SPECIFIED        0x0001
#define LRPC_REQUEST_FLAG_PART_OF_FLOW          0x0002
#define LRPC_REQUEST_FLAG_VIEW_PRESENT          0x0004


typedef struct _LRPC_REQUEST_MESSAGE
{
    /* Actually this starts with a PORT_MESSAGE header. */
    /* We won't define it here as our framework takes care of this. */


    /* Has the LRPC_MESSAGE_TYPE possible values. */
    UINT64  MessageType;

    /* Values from LRPC_REQUEST_FLAG_* */
    UINT32  Flags;


    /* Fields from below are basically taken from LRPC_BASE_CCALL::InitializeRequestAndAttributes. */
    /* Some reverse engineering on LRPC_BASE_CCALL was required. */

    /* lrpcReqBuffer->CallId = callId; */
    UINT32  CallId;

    /* lrpcReqBuffer->BindingId = this->GetBindingId(); */
    UINT32  BindingId;

    /* lrpcReqBuffer->ProcNum = this->RpcMessageProcNum; */
    UINT32  Procnum;

    /* This field is always set on zero. Could not figure what is used for. */
    /* lrpcBuffer->ReservedAlwaysZero = 0; */
    UINT64  ReservedAlwaysZero;

    /*
     * LRPC_BASE_CCALL::GetInPipeData
     *  if ( !*(_QWORD *)&this->InoutPipe )
     *  {
     *    RtlAssert("AdditionalCallData.InOutPipe");
     *  }
     *
     * In LRPC_BASE_CCALL::InitializeInPipeRequestAndAttributes:
     *  lrpcBuffer->PipeCallData = *(_QWORD *)(*(_QWORD *)&this->InoutPipe + 40i64);
     *
     * For ncalrpc this can be zero.
     */
    UINT64  PipeCallData;

    /*
     * LRPC_BASE_CCALL::AddToCausalFlowNotify
     *  if ( *(_QWORD *)&this->CausalFlow )
     *  {
     *    RtlAssert("this->CausalFlow == NULL");
     *  }
     *
     * LRPC_FAST_CAUSAL_FLOW::SendNextCalls:
     * lrpcRequest->CausalFlowData = this->CausalFlow;
     *
     * For ncalrpc this can be zero.
     */
    UINT64  CausalFlowData;

    /* Valid only when LRPC_REQUEST_FLAG_UUID_SPECIFIED is set. */
    GUID    Uuid;
} LRPC_REQUEST_MESSAGE;


///
/// From Windows 8.1 checked build. Reverse engineering.
/// Search rpcrt4 strings for LRPC_RESPONSE_FLAG.
///


#define LRPC_RESPONSE_FLAG_VIEW_PRESENT     0x0004

typedef struct _LRPC_RESPONSE_MESSAGE
{
    /* Actually this starts with a PORT_MESSAGE header. */
    /* We won't define it here as our framework takes care of this. */

    /* Has the LRPC_MESSAGE_TYPE possible values. */
    UINT64  MessageType;

    /* LRPC_RESPONSE_FLAG_* */
    UINT32  Flags;

    /*  LRPC_BASE_CCALL::UnpackResponse: RtlAssert("CallId == LocalResponse->CallId) */
    UINT32  CallId;

    /* Could not figure what this is. But the actual data starts after this one. */
    UINT64  Unknown;
} LRPC_RESPONSE_MESSAGE;

typedef struct _LRPC_FAULT_MESSAGE
{
    /* Has the LRPC_MESSAGE_TYPE possible values. */
    UINT64  MessageType;
    /* Failure status - if any. */
    UINT32  RpcStatus;
} LRPC_FAULT_MESSAGE;

///
/// This is documented. See the below links
/// https://pubs.opengroup.org/onlinepubs/9629399/apdxl.htm
/// https://pubs.opengroup.org/onlinepubs/9629399/apdxi.htm#tagtcjh_50
///

//
// Each tower_octet_string begins with a 2-byte floor count, encoded little-endian, followed by the tower floors as follows:
//  +-------------+---------+---------+---------+---------+---------+
//  | floor count | floor 1 | floor 2 | floor 3 |   ...   | floor n |
//  +-------------+---------+---------+---------+---------+---------+
//
// Each tower floor contains the following:
//  |<-   tower floor left hand side   ->|<-  tower floor right hand side  ->|
//  +------------+-----------------------+------------+----------------------+
//  |  LHS byte  |  protocol identifier  |  RHS byte  |  related or address  |
//  |   count    |        data           |   count    |        data          |
//  +------------+-----------------------+------------+----------------------+
//
// EPM_PROTOCOL_NCALRPC is not actually documented, but in rpcrt4!TowerExplode
//   we can see the logic called in LrpcTowerExplode (when syntax is 0xC)
//
// The structure of this tower is specific to LRPC (alpc).
//

#define EPM_PROTOCOL_NCALRPC        0x0C
#define EPM_PROTOCOL_UUID_DERIVED   0x0D
#define EPM_PROTOCOL_NAMED_PIPE     0x10

#pragma pack(push, 1)
typedef struct  _LRPC_EPM_TOWER
{
    UINT16  FloorCount;

    struct
    {
        /* The number of bytes up until RhsByteCount field. */
        UINT16  LhsByteCount;
        /* Octet 0 contains the hexadecimal value 0d. */
        /* This is a reserved protocol identifier prefix that indicates that the protocol ID is UUID derived. */
        UINT8   ProtocolId;
        /* Octets 1 to 16 inclusive contain the UUID, in little-endian format. */
        GUID    Guid;
        /* Octets 17 to 18 inclusive contain the major version number, in little-endian format. */
        UINT16  MajorVersion;
        /* The number of bytes until the end of this struct */
        UINT16  RhsByteCount;
        /* Minor version number, in little-endian format. */
        UINT16  MinorVersion;
    } Floor1;

    struct
    {
        /* The number of bytes up until RhsByteCount field. */
        UINT16  LhsByteCount;
        /* Octet 0 contains the hexadecimal value 0d. */
        /* This is a reserved protocol identifier prefix that indicates that the protocol ID is UUID derived. */
        UINT8   ProtocolId;
        /* Octets 1 to 16 inclusive contain the UUID, in little-endian format. */
        GUID    Guid;
        /* Octets 17 to 18 inclusive contain the major version number, in little-endian format. */
        UINT16  MajorVersion;
        /* The number of bytes until the end of this struct */
        UINT16  RhsByteCount;
        /* Minor version number, in little-endian format. */
        UINT16  MinorVersion;
    } Floor2;

    struct
    {
        /* The number of bytes up until RhsByteCount field. */
        UINT16  LhsByteCount;
        /* From reverse engineering this is implemented as 0xC */
        UINT8   ProtocolId;
        /* The number of bytes until the end of this struct. */
        UINT16  RhsByteCount;
        /* Must be zero. */
        UINT16  Reserved;
    } Floor3;

    struct
    {
        /* The number of bytes up until RhsByteCount field. */
        UINT16  LhsByteCount;
        /* This is interpreted as the endpoint name (pipe) floor.
           This corresponds to 0x10 https://pubs.opengroup.org/onlinepubs/9629399/apdxi.htm#tagcjh_28 */
        UINT8   ProtocolId;
        /* The number of bytes until the end of this struct. */
        UINT16  RhsByteCount;
        /* Must be 2x null terminated. */
        CHAR   EndpointName[2];
    } Floor4;
} LRPC_EPM_TOWER;
#pragma pack(pop)

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       API & DEFINITIONS                                                         |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/*
 * From win 8.1 checked build - AlpcpProcessSynchronousRequest (ntoskrnl)
 *  if ((Flags & 0x20000) == 0)
 *  {
 *      DbgPrint(assertion failed: 0 != (Flags & ALPC_MSGFLG_SYNC_REQUEST))
 *  }
 */
#define ALPC_MSGFLG_SYNC_REQUEST                        0x00020000      // Synchronous connection request.

/*
 * From win 8.1 checked build - AlpcpDispatchConnectionRequest (ntoskrnl)
 *  if ((Flags & 0x10000) != 0)
 *  {
 *      DbgPrint(assertion failed: (Flags & ALPC_MSGFLG_RELEASE_MESSAGE) == 0)
 *  }
 */
#define ALPC_MSGFLG_RELEASE_MESSAGE                     0x00010000      // Used to signal server it should free resources.

/*
 * From A. Ionescu's Syscan 2014 conference. Actual value from checked Win8.1 build.
 * Passed in PORT_MESSAGE->Type in response.
 *
 * PopUmpoProcessMessage (ntoskrnl):
 * if ((MessageType & 0x2000) == 0)
 * {
 *      DbgPrint("%s: ALPC message id=%x required continuationunexpectedly);
 * }
 *
 * LRPC_CCALLBACK::DealWithLargeResponse (rpcrt4):
 * if ((*(_WORD *)(*((_QWORD *)this + 22) + 4i64) & 0x2000) == 0 )
 * {
 *    RtlAssert("PeerResponse->PacketHeader.LpcHeader.u2.s2.Type & LPC_CONTINUATION_REQUIRED");
 * }
 */
#define LPC_CONTINUATION_REQUIRED                       0x00002000      // Server awaits a reply to free resources.

/*
 * From Thomas Garnier LPC ALPC paper. Cross checked with value from Win8.1build
 * Used for message attributes.
 *
 * LRPC_ADDRESS::HandleRequest (rpcrt4):
 *  if ((*((_DWORD *)MessageAttributes + 1) & 0x40000000) == 0)
 *  {
 *    RtlAssert(MsgAttributes->ValidAttributes & ALPC_FLG_MSG_DATAVIEW_ATTR) == ALPC_FLG_MSG_DATAVIEW_ATTR)
 *  }
 */
#define ALPC_FLG_MSG_DATAVIEW_ATTR                      0x40000000      // Message has a view attribute.

/*
 * From A. Ionescu's Syscan 2014 conference.
 * Used for view message attributes.
 */
#define ALPC_MSGVIEWATTR_RELEASE                        0x00010000      // Used to release the view.

/*
 * All 3 here are from Thomas Garnier LPC ALPC paper.
 * Used to initialize ALPC_PORT_ATTRIBUTES structures
 */
#define ALPC_PORTFLG_CAN_IMPERSONATE                    0x00010000      // Accept impersonation.
#define ALPC_PORTFLG_LPC_REQUESTS_ALLOWED               0x00020000      // Allow lpc messages.
#define ALPC_PORTFLG_CAN_DUPLICATE_OBJECTS              0x00080000      // Allows objects to be duplicated (eg: handles).


NTSYSCALLAPI NTSTATUS NTAPI
NtAlpcConnectPort(
    _Out_ HANDLE* PortHandle,
    _In_ UNICODE_STRING* PortName,
    _In_opt_ OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_opt_ ALPC_PORT_ATTRIBUTES* PortAttributes,
    _In_ UINT32 Flags,
    _In_opt_ SID* RequiredServerSid,
    _Inout_updates_bytes_to_opt_(*BufferLength, *BufferLength) PORT_MESSAGE* ConnectionMessage,
    _Inout_opt_ SIZE_T* BufferLength,
    _Inout_opt_ ALPC_MESSAGE_ATTRIBUTES* OutMessageAttributes,
    _Inout_opt_ ALPC_MESSAGE_ATTRIBUTES* InMessageAttributes,
    _In_opt_ LARGE_INTEGER* Timeout);

NTSYSCALLAPI NTSTATUS NTAPI
NtAlpcDisconnectPort(
    _In_ HANDLE PortHandle,
    _In_ UINT32 Flags);

NTSYSCALLAPI NTSTATUS NTAPI
NtAlpcSendWaitReceivePort(
    _In_ HANDLE PortHandle,
    _In_ UINT32 Flags,
    _In_reads_bytes_opt_(MessageToSend->u1.s1.TotalLength) PORT_MESSAGE* MessageToSend,
    _Inout_opt_ ALPC_MESSAGE_ATTRIBUTES* SendMessageAttributes,
    _Out_writes_bytes_to_opt_(*BufferLength, *BufferLength) PORT_MESSAGE* MessageToReceive,
    _Inout_opt_ SIZE_T* BufferLength,
    _Inout_opt_ ALPC_MESSAGE_ATTRIBUTES* ReceiveMessageAttributes,
    _In_opt_ LARGE_INTEGER* Timeout);

NTSYSCALLAPI NTSTATUS NTAPI
AlpcInitializeMessageAttribute(
    _In_ UINT32 AttributeFlags,
    _Out_writes_bytes_to_opt_(BufferSize, BufferSize) ALPC_MESSAGE_ATTRIBUTES* Buffer,
    _In_ SIZE_T BufferSize,
    _Out_ SIZE_T* RequiredBufferSize);

NTSYSCALLAPI PVOID NTAPI
AlpcGetMessageAttribute(
    _In_ ALPC_MESSAGE_ATTRIBUTES* Buffer,
    _In_ UINT32 AttributeFlag);

//
// We want to avoid name mangling. So all these will be
// encapsulated in extern C definitions. This is the end marker.
//
XPF_EXTERN_C_END();

//
// This file does not have doxygen.
//
#endif  // DOXYGEN_SHOULD_SKIP_THIS
