/**
 * @file        ALPC-Tools/ALPC-Demo/Source.cpp
 *
 * @brief       Entry point of the project.
 *              This is responsible for doing RPC calls manually
 *              through ALPC by connecting to the port,
 *              serializing parameters, and interpreting the result.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include <stdio.h>
#include "precomp.hpp"

#include "ITaskSchedulerInterface.hpp"
#include "IEventServiceInterface.hpp"
#include "LocalFWInterface.hpp"
#include "SvcctlInterface.hpp"
#include "SamrInterface.hpp"


/* To ease the access. */
using namespace AlpcRpc::DceNdr;        // NOLINT(*)

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       Command: Run Task                                                         |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief       This is the handler for "RunTask" command.
 *              It will wait for an input specifying the task path.
 *
 * @return      void
 */
static void XPF_API
CommandRunTask(
    void
) noexcept(true)
{
    char taskPath[MAX_PATH] = { 0 };
    xpf::String<wchar_t> wideTaskPath;

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Optional<AlpcRpc::DceNdr::ITaskSchedulerInterface> port;

    printf("[*] Handling %s.\r\n", XPF_FUNCSIG());

    /* Read the task path. */
    printf("Please input the task path to be run:\r\n");
    gets_s(taskPath, sizeof(taskPath));
    printf("[*] Will attempt to run the task from path %s.\r\n", taskPath);

    /* Convert the path to wide. */
    status = xpf::StringConversion::UTF8ToWide(taskPath, wideTaskPath);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide format. status = 0x%x.\r\n", status);
        return;
    }

    /* Connect to ITaskScheduler interface. */
    status = AlpcRpc::DceNdr::ITaskSchedulerInterface::Create(port);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to connect to the port. status = 0x%x.\r\n", status);
        return;
    }
    printf("[*] Connected to the port. Transfer syntax flags used: %d. \r\n",
           (*port).TransferSyntaxFlags());

    /* Declare parameters for SchRpcRun. */
    DceNdrWstring path;
    DcePrimitiveType<uint32_t> cArgs = 0;
    DceUniquePointer<DceConformantArray<DceNdrWstring>> pArgs;
    DcePrimitiveType<uint32_t> flags = 0x2;     // TASK_RUN_IGNORE_CONSTRAINTS
    DcePrimitiveType<uint32_t> sessionId = 0;
    DceUniquePointer<DceNdrWstring> user;
    DcePrimitiveType<uuid_t> pGuid;
    DcePrimitiveType<uint32_t> hResult;

    /* Now we convert the path to the proper format. */
    status = AlpcRpc::HelperWstringToNdr(wideTaskPath.View(), path, true);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in serializable format. status = 0x%x.\r\n", status);
        return;
    }

    /* Do the actual call. */
    status = (*port).SchRpcRun(path,
                               cArgs,
                               pArgs,
                               flags,
                               sessionId,
                               user,
                               &pGuid,
                               &hResult);
    if (!NT_SUCCESS(status))
    {
        printf("[!] SchRpcRun failed with status = 0x%x.\r\n", status);
        return;
    }

    /* Inspect the actula return value. */
    if (hResult.Data() != S_OK)
    {
        printf("[!] SchRpcRun failed with hresult = 0x%x.\r\n", hResult.Data());
        return;
    }

    /* All good! */
    printf("[*] SchRpcRun call succedeed. Ran task %S."
           "GUID = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}.\r\n",
            wideTaskPath.View().Buffer(),
            pGuid.Data().Data1,    pGuid.Data().Data2,    pGuid.Data().Data3,
            pGuid.Data().Data4[0], pGuid.Data().Data4[1], pGuid.Data().Data4[2], pGuid.Data().Data4[3],
            pGuid.Data().Data4[4], pGuid.Data().Data4[5], pGuid.Data().Data4[6], pGuid.Data().Data4[7]);
}

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       Command: Clear Event Log                                                  |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief       This is the handler for "ClearEventLog" command.
 *
 * @return      void
 */
static void XPF_API
CommandClearEventLog(
    void
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Optional<AlpcRpc::DceNdr::IEventServiceInterface> port;

    printf("[*] Handling %s.\r\n", XPF_FUNCSIG());

    /* Connect to IEventService interface. */
    status = AlpcRpc::DceNdr::IEventServiceInterface::Create(port);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to connect to the port. status = 0x%x.\r\n", status);
        return;
    }
    printf("[*] Connected to the port. Transfer syntax flags used: %d. \r\n",
           (*port).TransferSyntaxFlags());

    /* Declare parameters. */
    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> controlHandle;
    DcePrimitiveType<uint32_t> error;

    DcePrimitiveType<uint32_t> flags = 0;
    DcePrimitiveType<uint32_t> numChannels;
    DceUniquePointer<DceConformantPointerArray<DceNdrWstring>> channelsPaths;

    DceRpcInfo rpcErrorInfo;
    DceUniquePointer<DceNdrWstring> backupPath;

    /* First we obtain a control handle. */
    status = (*port).EvtRpcRegisterControllableOperation(&controlHandle, &error);
    if (!NT_SUCCESS(status))
    {
        printf("[!] EvtRpcRegisterControllableOperation failed with status 0x%x. \r\n", status);
        return;
    }
    if (error.Data() != 0)
    {
        printf("[!] EvtRpcRegisterControllableOperation returned with error code 0x%x. \r\n", error.Data());
        return;
    }

    /* Then we retrieve all channels. */
    status = (*port).EvtRpcGetChannelList(flags, &numChannels, &channelsPaths, &error);
    if (!NT_SUCCESS(status))
    {
        printf("[!] EvtRpcGetChannelList failed with status 0x%x. \r\n", status);
        goto CleanUp;
    }
    if (error.Data() != 0)
    {
        printf("[!] EvtRpcGetChannelList returned with error code 0x%x. \r\n", error.Data());
        goto CleanUp;
    }
    printf("[*] Retrieved a number of %d channels. \r\n", numChannels.Data());

    /* Now clear all channels. */
    for (size_t i = 0; i < numChannels.Data(); ++i)
    {
        const auto& crtChannel = channelsPaths.Data()->Data()[i];
        (void) (*port).EvtRpcClearLog(controlHandle,
                                      *crtChannel.Data(),
                                      backupPath,
                                      flags,
                                      &rpcErrorInfo,
                                      &error);
    }
    printf("[*] Removed event logs! \r\n");

CleanUp:
    /* Be a good citizen and clean the resources. */
    status = (*port).EvtRpcClose(&controlHandle, &error);
    if (!NT_SUCCESS(status))
    {
        printf("[!] EvtRpcClose failed with status 0x%x. \r\n", status);
        return;
    }
    if (error.Data() != 0)
    {
        printf("[!] EvtRpcClose returned with error code 0x%x. \r\n", error.Data());
        return;
    }
}


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       Command: Delete Firewall Rules                                            |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief       This is the handler for "DeleteFwRules" command.
 *
 * @return      void
 */
static void XPF_API
CommandDeleteFwRules(
    void
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Optional<AlpcRpc::DceNdr::LocalFwInterface> port;

    printf("[*] Handling %s.\r\n", XPF_FUNCSIG());

    /* Connect to LocalFwInterface interface. */
    status = AlpcRpc::DceNdr::LocalFwInterface::Create(port);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to connect to the port. status = 0x%x.\r\n", status);
        return;
    }
    printf("[*] Connected to the port. Transfer syntax flags used: %d. \r\n",
           (*port).TransferSyntaxFlags());

    /* Declare parameters for interacting with fw. */
    DcePrimitiveType<uint16_t> binaryVersion = 0x020A;      // FW_SEVEN_BINARY_VERSION
    DceEnumerationType storeType = 0x2;                     // FW_STORE_TYPE_LOCAL
    DceEnumerationType accessRight = 0x2;                   // FW_POLICY_ACCESS_RIGHT_READ_WRITE
    DcePrimitiveType<uint32_t> dwFlags = 0x0;
    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> policyStore;
    DcePrimitiveType<uint32_t> retValue;

    /* Now we convert the path to the proper format. */
    status = (*port).FWOpenPolicyStore(binaryVersion,
                                       storeType,
                                       accessRight,
                                       dwFlags,
                                       &policyStore,
                                       &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] FWOpenPolicyStore failed with status 0x%x. \r\n", status);
        return;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] FWOpenPolicyStore returned with error code 0x%x. \r\n", retValue.Data());
        return;
    }

    /* Attempt to delete the firewall rules. */
    status = (*port).FWDeleteAllFirewallRules(policyStore,
                                              &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] FWDeleteAllFirewallRules failed with status 0x%x. \r\n", status);
        goto CleanUp;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] FWDeleteAllFirewallRules returned with error code 0x%x. \r\n", retValue.Data());
        goto CleanUp;
    }
    printf("[*] Successfully deleted firewall rules! \r\n");

CleanUp:
    /* Be a good citizen and clean the handle. */
    status = (*port).FWClosePolicyStore(&policyStore,
                                        &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] FWClosePolicyStore failed with status 0x%x. \r\n", status);
        return;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] FWClosePolicyStore returned with error code 0x%x. \r\n", retValue.Data());
        return;
    }
}

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       Command: Create Service                                                   |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief       This is the handler for "CreateService" command.
 *              It will wait for an input specifying the service path and name.
 *
 * @return      void
 */
static void XPF_API
CommandCreateService(
    void
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Optional<AlpcRpc::DceNdr::SvcCtlInterface> port;

    printf("[*] Handling %s.\r\n", XPF_FUNCSIG());

    /* Connect to SvcCtl interface. */
    status = AlpcRpc::DceNdr::SvcCtlInterface::Create(port);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to connect to the port. status = 0x%x.\r\n", status);
        return;
    }
    printf("[*] Connected to the port. Transfer syntax flags used: %d. \r\n",
           (*port).TransferSyntaxFlags());

    /* Declare parameters for interacting with svcctl. */
    DceUniquePointer<DceNdrWstring> machineName;
    DceUniquePointer<DceNdrWstring> databaseName;
    DcePrimitiveType<uint32_t> desiredAccess = SC_MANAGER_ALL_ACCESS;
    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> scManagerHandle;
    DcePrimitiveType<uint32_t> retValue;

    /* Get machine name. */
    WCHAR machineNameBuff[MAX_PATH + 1] = { 0 };
    DWORD machineNameBuffSize = MAX_PATH;

    if (FALSE == GetComputerNameW(machineNameBuff, &machineNameBuffSize))
    {
        printf("[!] Failed to retrieve computer name. gle = 0x%x.\r\n", GetLastError());
        return;
    }
    status = AlpcRpc::HelperWstringToUniqueNdr(machineNameBuff, machineName, true);
    if (!NT_SUCCESS(status))
    {
        printf("[!] HelperWstringToUniqueNdr failed with status 0x%x. \r\n", status);
        return;
    }

    /* Open scm database. */
    status = (*port).ROpenSCManagerW(machineName,
                                     databaseName,
                                     desiredAccess,
                                     &scManagerHandle,
                                     &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] ROpenSCManagerW failed with status 0x%x. \r\n", status);
        return;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] ROpenSCManagerW returned with error code 0x%x. \r\n", retValue.Data());
        return;
    }

    /* Now create the service. */
    DceNdrWstring lpServiceName;
    DceUniquePointer<DceNdrWstring> lpDisplayName;
    DcePrimitiveType<uint32_t> dwDesiredAccess = SERVICE_ALL_ACCESS;
    DcePrimitiveType<uint32_t> dwServiceType = SERVICE_KERNEL_DRIVER;
    DcePrimitiveType<uint32_t> dwStartType = SERVICE_DEMAND_START;
    DcePrimitiveType<uint32_t> dwErrorControl = SERVICE_ERROR_NORMAL;
    DceNdrWstring lpBinaryPathName;
    DceUniquePointer<DceNdrWstring> lpLoadOrderGroup;
    DceUniquePointer<DcePrimitiveType<uint32_t>> lpdwTagId;
    DceUniquePointer<DceConformantArray<DcePrimitiveType<uint8_t>>> lpDependencies;
    DcePrimitiveType<uint32_t> dwDependSize = 0;
    DceUniquePointer<DceNdrWstring> lpServiceStartName;
    DceUniquePointer<DceConformantArray<DcePrimitiveType<uint8_t>>> lpPassword;
    DcePrimitiveType<uint32_t> dwPwSize = 0;
    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> lpServiceHandle;

    char serviceName[MAX_PATH] = { 0 };
    xpf::String<wchar_t> wideServiceName;

    char servicePath[MAX_PATH] = { 0 };
    xpf::String<wchar_t> wideServicePath;

    bool hasServiceHandle = false;

    /* Read the service data. */
    printf("Please input the service path to be create:\r\n");
    gets_s(servicePath, sizeof(servicePath));
    printf("[*] Will attempt to create the service from path %s.\r\n", servicePath);
    status = xpf::StringConversion::UTF8ToWide(servicePath, wideServicePath);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide format. status = 0x%x.\r\n", status);
        goto CleanUp;
    }
    status = AlpcRpc::HelperWstringToNdr(wideServicePath.View(), lpBinaryPathName, true);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide ndr format. status = 0x%x.\r\n", status);
        goto CleanUp;
    }

    /* Read the service name. */
    printf("Please input the service name to be create:\r\n");
    gets_s(serviceName, sizeof(serviceName));
    printf("[*] Will attempt to create the service with name %s.\r\n", servicePath);
    status = xpf::StringConversion::UTF8ToWide(serviceName, wideServiceName);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide format. status = 0x%x.\r\n", status);
        goto CleanUp;
    }
    status = AlpcRpc::HelperWstringToNdr(wideServiceName.View(), lpServiceName, true);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide ndr format. status = 0x%x.\r\n", status);
        goto CleanUp;
    }
    /* Use the same display name. */
    status = AlpcRpc::HelperWstringToUniqueNdr(wideServiceName.View(), lpDisplayName, true);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide ndr format. status = 0x%x.\r\n", status);
        goto CleanUp;
    }

    status = (*port).RCreateServiceW(scManagerHandle,
                                     lpServiceName,
                                     lpDisplayName,
                                     dwDesiredAccess,
                                     dwServiceType,
                                     dwStartType,
                                     dwErrorControl,
                                     lpBinaryPathName,
                                     lpLoadOrderGroup,
                                     &lpdwTagId,
                                     lpDependencies,
                                     dwDependSize,
                                     lpServiceStartName,
                                     lpPassword,
                                     dwPwSize,
                                     &lpServiceHandle,
                                     &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] RCreateServiceW failed with status 0x%x. \r\n", status);
        goto CleanUp;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] RCreateServiceW returned with error code 0x%x. \r\n", retValue.Data());
        goto CleanUp;
    }
    /* The service handle must be freed. */
    hasServiceHandle = true;
    printf("[*] Successfully created the service! \r\n");

CleanUp:
    /* Be a good citizen and close all resources. */
    if (hasServiceHandle)
    {
        status = (*port).RCloseServiceHandle(&lpServiceHandle, &retValue);
        if (!NT_SUCCESS(status))
        {
            printf("[!] RCloseServiceHandle failed with status 0x%x. \r\n", status);
        }
        if (retValue.Data() != 0)
        {
            printf("[!] RCloseServiceHandle returned with error code 0x%x. \r\n", retValue.Data());
        }
        hasServiceHandle = false;
    }
    status = (*port).RCloseServiceHandle(&scManagerHandle, &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] RCloseServiceHandle failed with status 0x%x. \r\n", status);
        return;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] RCloseServiceHandle returned with error code 0x%x. \r\n", retValue.Data());
        return;
    }
}


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       Command: Create User                                                      |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief       This is the handler for "CreateUser" command.
 *
 * @return      void
 */
static void XPF_API
CommandCreateUser(
    void
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Optional<AlpcRpc::DceNdr::SamrInterface> port;

    printf("[*] Handling %s.\r\n", XPF_FUNCSIG());

    /* Connect to Samr interface. */
    status = AlpcRpc::DceNdr::SamrInterface::Create(port);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to connect to the port. status = 0x%x.\r\n", status);
        return;
    }
    printf("[*] Connected to the port. Transfer syntax flags used: %d. \r\n",
           (*port).TransferSyntaxFlags());

    /* Get the user name. */
    char userName[MAX_PATH] = { 0 };
    xpf::String<wchar_t> wideUserName;
    DceUniquePointer<DceNdrWstring> dceUserName;

    printf("Please input the user name to be create:\r\n");
    gets_s(userName, sizeof(userName));
    printf("[*] Will attempt to create the user with name %s.\r\n", userName);
    status = xpf::StringConversion::UTF8ToWide(userName, wideUserName);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the username in wide format. status = 0x%x.\r\n", status);
        return;
    }
    status = AlpcRpc::HelperWstringToUniqueNdr(wideUserName.View(), dceUserName, false);
    if (!NT_SUCCESS(status))
    {
        printf("[!] Failed to convert the path in wide ndr format. status = 0x%x.\r\n", status);
        return;
    }

    /* Get machine name. */
    WCHAR machineNameBuff[MAX_PATH + 1] = { 0 };
    DWORD machineNameBuffSize = MAX_PATH;
    DceUniquePointer<DceNdrWstring> localdomain;

    if (FALSE == GetComputerNameW(machineNameBuff, &machineNameBuffSize))
    {
        printf("[!] Failed to retrieve computer name. gle = 0x%x.\r\n", GetLastError());
        return;
    }
    status = AlpcRpc::HelperWstringToUniqueNdr(machineNameBuff, localdomain, false);
    if (!NT_SUCCESS(status))
    {
        printf("[!] HelperWstringToUniqueNdr failed with status 0x%x. \r\n", status);
        return;
    }
    printf("[*] Retrieved local domain name: %S. \r\n", machineNameBuff);

    /* Parameters for dce-ndr calls. */
    DceUniquePointer<DceNdrWstring> serverName;
    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> serverHandle;
    DcePrimitiveType<uint32_t> desiredAccess = MAXIMUM_ALLOWED;
    DcePrimitiveType<uint32_t> retValue;

    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> domainHandle;
    DceRpcUnicodeString domainName{ localdomain };
    DceUniquePointer<DceRpcSid> domainSid;

    DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE> userHandle;
    DceRpcUnicodeString username{ dceUserName };
    DcePrimitiveType<uint32_t> userAccountType = 0x00000010;    // USER_NORMAL_ACCOUNT
    DcePrimitiveType<uint32_t> userGrantedAccess;
    DcePrimitiveType<uint32_t> userRid;

    /* For cleaning up. */
    bool hasDomainHandle = false;
    bool hasUserHandle = false;

    /* Connect to SAM database. */
    status = (*port).SamrConnect(serverName,
                                 &serverHandle,
                                 desiredAccess,
                                 &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] SamrConnect failed with status 0x%x. \r\n", status);
        return;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] SamrConnect returned with error code 0x%x. \r\n", retValue.Data());
        return;
    }

    /* Find the SID of the domain name. */
    status = (*port).SamrLookupDomainInSamServer(serverHandle,
                                                 domainName,
                                                 &domainSid,
                                                 &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] SamrLookupDomainInSamServer failed with status 0x%x. \r\n", status);
        goto CleanUp;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] SamrLookupDomainInSamServer returned with error code 0x%x. \r\n", retValue.Data());
        goto CleanUp;
    }

    /* Open the domain. */
    status = (*port).SamrOpenDomain(serverHandle,
                                    desiredAccess,
                                    *(domainSid.Data()),
                                    &domainHandle,
                                    &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] SamrOpenDomain failed with status 0x%x. \r\n", status);
        goto CleanUp;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] SamrOpenDomain returned with error code 0x%x. \r\n", retValue.Data());
        goto CleanUp;
    }
    hasDomainHandle = true;

    //
    // Now create the user.
    //
    desiredAccess = MAXIMUM_ALLOWED;
    status = (*port).SamrCreateUser2InDomain(domainHandle,
                                             username,
                                             userAccountType,
                                             desiredAccess,
                                             &userHandle,
                                             &userRid,
                                             &userGrantedAccess,
                                             &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] SamrCreateUser2InDomain failed with status 0x%x. \r\n", status);
        goto CleanUp;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] SamrCreateUser2InDomain returned with error code 0x%x. \r\n", retValue.Data());
        goto CleanUp;
    }
    hasUserHandle = true;

    printf("[*] Successfully created new user! \r\n");

CleanUp:
    /* Be a good citizen and clean resources. */
    if (hasUserHandle)
    {
        status = (*port).SamrCloseHandle(&userHandle,
                                         &retValue);
        if (!NT_SUCCESS(status))
        {
            printf("[!] SamrCloseHandle failed with status 0x%x. \r\n", status);
        }
        if (retValue.Data() != 0)
        {
            printf("[!] SamrCloseHandle returned with error code 0x%x. \r\n", retValue.Data());
        }
        hasUserHandle = false;
    }
    if (hasDomainHandle)
    {
        status = (*port).SamrCloseHandle(&domainHandle,
                                         &retValue);
        if (!NT_SUCCESS(status))
        {
            printf("[!] SamrCloseHandle failed with status 0x%x. \r\n", status);
        }
        if (retValue.Data() != 0)
        {
            printf("[!] SamrCloseHandle returned with error code 0x%x. \r\n", retValue.Data());
        }
        hasDomainHandle = false;
    }

    status = (*port).SamrCloseHandle(&serverHandle,
                                     &retValue);
    if (!NT_SUCCESS(status))
    {
        printf("[!] SamrCloseHandle failed with status 0x%x. \r\n", status);
        return;
    }
    if (retValue.Data() != 0)
    {
        printf("[!] SamrCloseHandle returned with error code 0x%x. \r\n", retValue.Data());
        return;
    }
}

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       Command: Print Help                                                       |
/// |                                Keep this last before int main()                                                 |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief       This is the handler for "PrintHelp" command.
 *
 * @return      void
 */
static void XPF_API
CommandPrintHelp(
    void
) noexcept(true)
{
    printf("Available commands: \r\n");
    printf("   * RunTask       - Uses SchRpcRun() to run a task identified by its path. \r\n");
    printf("                   - Arguments: [task_path] - the path of the task to be run. \r\n");
    printf("   * ClearEventLog - Uses EvtRpcClearLog() to clear the existing event logs. \r\n");
    printf("   * DeleteFwRules - Uses FWDeleteAllFirewallRules() to remove the firewall rules. \r\n");
    printf("   * CreateService - Uses RCreateServiceW() to create a kernel mode service. \r\n");
    printf("                   - Arguments: [path] - the full path of the .sys file. \r\n");
    printf("                                [name] - the name to be given to the service. \r\n");
    printf("   * CreateUser    - Uses SamrCreateUser2InDomain() to create a new user. \r\n");
    printf("                   - Arguments: [username] - the name of the user to be created. \r\n");
    printf("   * Exit          - Exits the current aplication. \r\n");
}

/**
 * @brief       The actual entry point of the application.
 *
 * @param[in]   ArgumentsCount - an interger specifying how many arguments
 *                               were passed to the command line when running
 *                               this program.
 * @param[in]   Arguments      - an array of wide chars arguments passed to the
 *                               command line.
 *
 * @note        Parameters passed to the command line are not actually used.
 *              We're exponsing our own interface for the user to interact with.
 *
 * @return      0 if everything went well,
 *              an error code otherwise.
 */
int
XPF_PLATFORM_CONVENTION
wmain(
    _In_ int ArgumentsCount,
    _In_ wchar_t* Arguments[]
) noexcept(true)
{
    XPF_UNREFERENCED_PARAMETER(ArgumentsCount);
    XPF_UNREFERENCED_PARAMETER(Arguments);

    CommandPrintHelp();

    while (true)
    {
        /* Read the command. */
        char command[0x1000] = { 0 };
        printf("Please input the command:\r\n");
        gets_s(command, sizeof(command));

        xpf::StringView commandView{ command };
        if (commandView.Equals("RunTask", true))
        {
            CommandRunTask();
        }
        else if (commandView.Equals("ClearEventLog", true))
        {
            CommandClearEventLog();
        }
        else if (commandView.Equals("DeleteFwRules", true))
        {
            CommandDeleteFwRules();
        }
        else if (commandView.Equals("CreateService", true))
        {
            CommandCreateService();
        }
        else if (commandView.Equals("CreateUser", true))
        {
            CommandCreateUser();
        }
        else if (commandView.Equals("Exit", true))
        {
            printf("Bye!\r\n");
            break;
        }
        else
        {
            printf("[!] Unrecognized command %s!\r\n", command);
        }
    }

    return 0;
}
