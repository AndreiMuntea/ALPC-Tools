/**
 * @file        ALPC-Tools/AlpcMon_Sys/RpcEngine.cpp
 *
 * @brief       The inspection engine capable of understanding
 *              RPC messages as they are serialized.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "DceNdr.hpp"
#include "IEventServiceInterface.hpp"
#include "ITaskSchedulerInterface.hpp"
#include "LocalFWInterface.hpp"
#include "SamrInterface.hpp"
#include "SvcctlInterface.hpp"

#include "RpcEngine.hpp"
#include "trace.hpp"

using namespace AlpcRpc::DceNdr;    // NOLINT(*)

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       PAGED SECTION AREA                                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief   Put everything below here in paged section.
 */
XPF_SECTION_PAGED;

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                              Helper to analyze samr interface                                                   |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void XPF_API
RpcEngineAnalyzeSamrMessage(
    _In_ uint32_t ProcessPid,
    _Inout_ AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _In_ const uint64_t ProcedureNumber
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (ProcedureNumber == 50)  /* SamrCreateUser2InDomain */
    {
        DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> domainHandle;
        DceRpcUnicodeString name;
        DcePrimitiveType<uint32_t> accountType;
        DcePrimitiveType<uint32_t> desiredAccess;

        /* Unmarshall the parameters. */
        MarshallBuffer.Unmarshall(domainHandle)
                      .Unmarshall(name)
                      .Unmarshall(accountType)
                      .Unmarshall(desiredAccess);
        status = MarshallBuffer.Status();
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("SamrCreateUser2InDomain unmarshalling failed with %!STATUS!",
                           status);
            return;
        }

        /* Grab the actual name in a more convenient form. */
        xpf::String<wchar_t> strName;
        status = name.GetBuffer(strName);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("GetBuffer unmarshalling failed with %!STATUS!",
                           status);
            return;
        }

        /* Now simply log - we may want to send an event at some point. */
        SysMonLogInfo("Process with pid %d created a new user %S",
                       ProcessPid,
                       strName.View().Buffer());
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                              Helper to analyze svcctl interface                                                 |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void XPF_API
RpcEngineAnalyzeSvcCtlMessage(
    _In_ uint32_t ProcessPid,
    _Inout_ AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _In_ const uint64_t ProcedureNumber
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (ProcedureNumber == 12)  /* RCreateServiceW */
    {
        DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> hSCManager;
        DceNdrWstring lpServiceName;
        DceUniquePointer<AlpcRpc::DceNdr::DceNdrWstring> lpDisplayName;
        DcePrimitiveType<uint32_t> dwDesiredAccess;
        DcePrimitiveType<uint32_t> dwServiceType;
        DcePrimitiveType<uint32_t> dwStartType;
        DcePrimitiveType<uint32_t> dwErrorControl;
        DceNdrWstring lpBinaryPathName;
        DceUniquePointer<DceNdrWstring> lpLoadOrderGroup;
        DceUniquePointer<DcePrimitiveType<uint32_t>> lpdwTagId;
        DceUniquePointer<DceConformantArray<DcePrimitiveType<uint8_t>>> lpDependencies;
        DcePrimitiveType<uint32_t> dwDependSize;
        DceUniquePointer<DceNdrWstring> lpServiceStartName;
        DceUniquePointer<DceConformantArray<DcePrimitiveType<uint8_t>>> lpPassword;
        DcePrimitiveType<uint32_t> dwPwSize;

        /* Unmarshall the parameters. */
        MarshallBuffer.Unmarshall(hSCManager)
                      .Unmarshall(lpServiceName)
                      .Unmarshall(lpDisplayName)
                      .Unmarshall(dwDesiredAccess)
                      .Unmarshall(dwServiceType)
                      .Unmarshall(dwStartType)
                      .Unmarshall(dwErrorControl)
                      .Unmarshall(lpBinaryPathName)
                      .Unmarshall(lpLoadOrderGroup)
                      .Unmarshall(lpdwTagId)
                      .Unmarshall(lpDependencies)
                      .Unmarshall(dwDependSize)
                      .Unmarshall(lpServiceStartName)
                      .Unmarshall(lpPassword)
                      .Unmarshall(dwPwSize);
        status = MarshallBuffer.Status();
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("RCreateServiceW unmarshalling failed with %!STATUS!",
                           status);
            return;
        }

        /* Grab the strings in a more convenient form */
        xpf::String<wchar_t> strServiceName;
        xpf::String<wchar_t> strDisplayName;
        xpf::String<wchar_t> strBinaryPathName;

        status = AlpcRpc::HelperNdrWstringToWstring(lpServiceName,
                                                    strServiceName);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperNdrWstringToWstring failed with %!STATUS!",
                           status);
            return;
        }
        status = AlpcRpc::HelperUniqueNdrWstringToWstring(lpDisplayName,
                                                          strDisplayName);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperUniqueNdrWstringToWstring failed with %!STATUS!",
                           status);
            return;
        }
        status = AlpcRpc::HelperNdrWstringToWstring(lpBinaryPathName,
                                                    strBinaryPathName);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperNdrWstringToWstring failed with %!STATUS!",
                           status);
            return;
        }

        /* Now simply log - we may want to send an event at some point. */
        SysMonLogInfo("Process with pid %d created a new service name %S display %S path %S",
                       ProcessPid,
                       strServiceName.View().Buffer(),
                       strDisplayName.View().Buffer(),
                       strBinaryPathName.View().Buffer());
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                              Helper to analyze ITaskScheduler interface                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void XPF_API
RpcEngineAnalyzeITaskSchedulerMessage(
    _In_ uint32_t ProcessPid,
    _Inout_ AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _In_ const uint64_t ProcedureNumber
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (ProcedureNumber == 12)  /* SchRpcRun */
    {
        DceNdrWstring path;
        DcePrimitiveType<uint32_t> cArgs;
        DceUniquePointer<DceConformantArray<DceNdrWstring>> pArgs;
        DcePrimitiveType<uint32_t> flags;
        DcePrimitiveType<uint32_t> sessionId;
        DceUniquePointer<DceNdrWstring> user;

        /* Unmarshall the parameters. */
        MarshallBuffer.Unmarshall(path)
                      .Unmarshall(cArgs)
                      .Unmarshall(pArgs)
                      .Unmarshall(flags)
                      .Unmarshall(sessionId)
                      .Unmarshall(user);
        status = MarshallBuffer.Status();
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("SchRpcRun unmarshalling failed with %!STATUS!",
                           status);
            return;
        }

        /* Grab the strings in a more convenient form */
        xpf::String<wchar_t> strPath;

        status = AlpcRpc::HelperNdrWstringToWstring(path,
                                                    strPath);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperNdrWstringToWstring failed with %!STATUS!",
                           status);
            return;
        }

        /* Now simply log - we may want to send an event at some point. */
        SysMonLogInfo("Process with pid %d ran task from path %S",
                       ProcessPid,
                       strPath.View().Buffer());
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                              Helper to analyze IEventService interface                                          |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void XPF_API
RpcEngineAnalyzeIEventServiceMessage(
    _In_ uint32_t ProcessPid,
    _Inout_ AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _In_ const uint64_t ProcedureNumber
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (ProcedureNumber == 6)  /* EvtRpcClearLog */
    {
        DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> control;
        DceNdrWstring channelPath;
        DceUniquePointer<DceNdrWstring> backupPath;
        DcePrimitiveType<uint32_t> flags;

        /* Unmarshall the parameters. */
        MarshallBuffer.Unmarshall(control)
                      .Unmarshall(channelPath)
                      .Unmarshall(backupPath)
                      .Unmarshall(flags);
        status = MarshallBuffer.Status();
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("EvtRpcClearLog unmarshalling failed with %!STATUS!",
                           status);
            return;
        }

        /* Grab the strings in a more convenient form */
        xpf::String<wchar_t> channelPathStr;

        status = AlpcRpc::HelperNdrWstringToWstring(channelPath,
                                                    channelPathStr);
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("HelperNdrWstringToWstring failed with %!STATUS!",
                           status);
            return;
        }

        /* Now simply log - we may want to send an event at some point. */
        SysMonLogInfo("Process with pid %d is clearing event log channel %S",
                       ProcessPid,
                       channelPathStr.View().Buffer());
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                              Helper to analyze LocalFwInterface interface                                       |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void XPF_API
RpcEngineAnalyzeLocalFwInterfaceMessage(
    _In_ uint32_t ProcessPid,
    _Inout_ AlpcRpc::DceNdr::DceMarshallBuffer& MarshallBuffer,
    _In_ const uint64_t ProcedureNumber
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (ProcedureNumber == 8)  /* FWDeleteAllFirewallRules */
    {
        DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> hPolicyStore;

        /* Unmarshall the parameters. */
        MarshallBuffer.Unmarshall(hPolicyStore);
        status = MarshallBuffer.Status();
        if (!NT_SUCCESS(status))
        {
            SysMonLogError("FWDeleteAllFirewallRules unmarshalling failed with %!STATUS!",
                           status);
            return;
        }

        /* Now simply log - we may want to send an event at some point. */
        SysMonLogInfo("Process with pid %d is deleting all firewall rules!",
                       ProcessPid);
    }
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                              SysMon::RpcEngine::Analyze.                                                        |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//


_Use_decl_annotations_
void XPF_API
SysMon::RpcEngine::Analyze(
    _In_ _Const_ const uint8_t* Buffer,
    _In_ size_t BufferSize,
    _In_ const uuid_t& Interface,
    _In_ const uint64_t ProcedureNumber,
    _In_ const uint64_t& TransferSyntax
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != Buffer);
    XPF_DEATH_ON_FAILURE(0 != BufferSize);

    //
    // Grab process id.
    //
    uint32_t processId = HandleToUlong(::PsGetCurrentProcessId());

    //
    // Grab a marshall buffer.
    //
    xpf::Buffer<AlpcRpc::DceNdr::DceAllocator> rawBuffer;
    NTSTATUS status = rawBuffer.Resize(BufferSize);
    if (!NT_SUCCESS(status))
    {
        return;
    }
    xpf::ApiCopyMemory(rawBuffer.GetBuffer(),
                       Buffer,
                       rawBuffer.GetSize());

    AlpcRpc::DceNdr::DceMarshallBuffer marshallBuffer{ static_cast<uint32_t>(TransferSyntax) };
    marshallBuffer.MarshallRawBuffer(rawBuffer);

    if (Interface == gSamrInterface.SyntaxGUID)
    {
        RpcEngineAnalyzeSamrMessage(processId,
                                    marshallBuffer,
                                    ProcedureNumber);
    }
    else if (Interface == gSvcCtlInterface.SyntaxGUID)
    {
        RpcEngineAnalyzeSvcCtlMessage(processId,
                                      marshallBuffer,
                                      ProcedureNumber);
    }
    else if (Interface == gITaskSchedulerServiceIdentifier.SyntaxGUID)
    {
        RpcEngineAnalyzeITaskSchedulerMessage(processId,
                                              marshallBuffer,
                                              ProcedureNumber);
    }
    else if (Interface == gIEventServiceIdentifier.SyntaxGUID)
    {
        RpcEngineAnalyzeIEventServiceMessage(processId,
                                             marshallBuffer,
                                             ProcedureNumber);
    }
    else if (Interface == gLocalFwInterface.SyntaxGUID)
    {
        RpcEngineAnalyzeLocalFwInterfaceMessage(processId,
                                                marshallBuffer,
                                                ProcedureNumber);
    }
}
