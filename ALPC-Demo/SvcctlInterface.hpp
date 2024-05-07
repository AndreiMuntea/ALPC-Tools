/**
 * @file        ALPC-Tools/ALPC-Demo/SvcCtlInterface.hpp
 *
 * @brief       In this file we have basic functionality for SvcCtl
 *              interface. It is fully documented here:
 *              https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-scmr/19168537-40b5-4d7a-99e0-d77f0f5e0241
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
 * @brief   {367ABB81-9844-35F1-AD32-98F038001003}.
 */
static constexpr ALPC_RPC_SYNTAX_IDENTIFIER gSvcCtlInterface =
{
    .SyntaxGUID = { 0x367ABB81, 0x9844, 0x35F1, { 0xAD, 0x32, 0x98, 0xF0, 0x38, 0x00, 0x10, 0x03 } },
    .SyntaxVersion = { 2, 0 }
};
/**
 * @brief   The port name for svcctl is well known, not resolved via epmapper.
 */
static constexpr xpf::StringView<wchar_t> gNtsvcsPortName = { L"\\RPC Control\\ntsvcs" };

namespace AlpcRpc
{
namespace DceNdr
{

/**
 * @brief   This class contains the minimalistic functionality to make RPC calls manually via ALPC.
 *          It is not fully featured. It only contains some methods. More can be added later.
 */
class SvcCtlInterface final
{
 private:
    /**
     * @brief  Default constructor. It is protected
     *         as the static API Connect should be used instead.
     */
     SvcCtlInterface(void) noexcept(true) = default;
 public:
    /**
     * @brief  Default destructor.
     *         Will disconnect the port.
     */
     ~SvcCtlInterface(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are deleted.
     */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(SvcCtlInterface, delete);

    /**
     * @brief          This method is used to create a SvcCtlInterface port.
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
        _Inout_ xpf::Optional<SvcCtlInterface>& Port
    ) noexcept(true)
    {
        /* Create the port. */
        Port.Emplace();

        /* And now connect and bind to the actual port. First we try NDR64. */
        (*Port).m_Port.Reset();
        NTSTATUS status = AlpcRpc::RpcAlpcClientPort::Connect(gNtsvcsPortName,
                                                              gSvcCtlInterface,
                                                              gNdr64TransferSyntaxIdentifier,
                                                              (*Port).m_Port);
        if (NT_SUCCESS(status))
        {
            return status;
        }

        /* On fail, we retry with DCE-NDR. */
        (*Port).m_Port.Reset();
        return AlpcRpc::RpcAlpcClientPort::Connect(gNtsvcsPortName,
                                                   gSvcCtlInterface,
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
     * @brief          ROpenSCManagerW   (Opnum 15)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-scmr/dc84adb3-d51d-48eb-820d-ba1c6ca5faf2
     *
     * @param[in]      lpMachineName   - null-terminated UNICODE string that specifies the server's machine name.
     * @param[in]      lpDatabaseName  - A pointer to a null-terminated UNICODE string that specifies the name of the SCM database to open.
     *                                   The parameter MUST be set to NULL, "ServicesActive", or "ServicesFailed".
     * @param[in]      dwDesiredAccess - A value that specifies the access to the database.
     * @param[out]     lpScHandle      - An LPSC_RPC_HANDLE data type that defines the handle to the newly opened SCM database.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    ROpenSCManagerW(
        _In_ const DceUniquePointer<DceNdrWstring>& lpMachineName,
        _In_ const DceUniquePointer<DceNdrWstring>& lpDatabaseName,
        _In_ DcePrimitiveType<uint32_t> dwDesiredAccess,
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* lpScHandle,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //   DWORD ROpenSCManagerW(
        //    [in, string, unique, range(0, SC_MAX_COMPUTER_NAME_LENGTH)] SVCCTL_HANDLEW lpMachineName,
        //    [in, string, unique, range(0, SC_MAX_NAME_LENGTH)] wchar_t* lpDatabaseName,
        //    [in] DWORD dwDesiredAccess,
        //    [out] LPSC_RPC_HANDLE lpScHandle);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *lpScHandle = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(lpMachineName)
               .Marshall(lpDatabaseName)
               .Marshall(dwDesiredAccess);
        if (!NT_SUCCESS(iBuffer.Status()))
        {
            return iBuffer.Status();
        }

        /* Do the actual call. */
        NTSTATUS status = (*m_Port).CallProcedure(15, iBuffer, oBuffer);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Deserialize output parameters. */
        oBuffer.Unmarshall(*lpScHandle)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          RCloseServiceHandle    (Opnum 0)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-scmr/a2a4e174-09fb-4e55-bad3-f77c4b13245c
     *
     * @param[in,out]  hSCObject       - This parameter contains an opened handle.
     * @param[out]     retValue        - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    RCloseServiceHandle(
        _Inout_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* hSCObject,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        //   DWORD RCloseServiceHandle([in, out] LPSC_RPC_HANDLE hSCObject);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(*hSCObject);
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
        oBuffer.Unmarshall(*hSCObject)
               .Unmarshall(*retValue);
        return oBuffer.Status();
    }

    /**
     * @brief          RCreateServiceW   (Opnum 12)
     *
     * @details        https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-scmr/6a8ca926-9477-4dd4-b766-692fab07227e
     *
     * @param[in]      hSCManager         - handle to the SCM database.
     * @param[in]      lpServiceName      - a null-terminated UNICODE string that specifies the name of the service to install.
     *                                      This MUST not be NULL.
     * @param[in]      lpDisplayName      - a pointer to a null-terminated UNICODE string that contains the display name by which
     *                                      user interface programs identify the service.
     * @param[in]      dwDesiredAccess    - A value that specifies the access to the service.
     * @param[in]      dwServiceType      - A value that specifies the type of service.
     * @param[in]      dwStartType        - A value that specifies when to start the service.
     * @param[in]      dwErrorControl     - A value that specifies the severity of the error if the service
     *                                      fails to start and determines the action that the SCM takes.
     * @param[in]      lpBinaryPathName   - A pointer to a null-terminated UNICODE string that contains the
     *                                      fully qualified path to the service binary file.
     * @param[in]      lpLoadOrderGroup   - A pointer to a null-terminated UNICODE string that names the
     *                                      load-ordering group of which this service is a member.
     *                                      Specify NULL or an empty string if the service does not belong to a load-ordering group.
     * @param[in,out]  lpdwTagId          - A pointer to a variable that receives a tag value.
     * @param[in]      lpDependencies     - A pointer to an array of null-separated names of services or load ordering groups
     *                                      that MUST start before this service. The array is doubly null-terminated.
     *                                      If the pointer is NULL or if it points to an empty string, the service has no dependencies.
     * @param[in]      dwDependSize       - The size, in bytes, of the string specified by the lpDependencies parameter.
     * @param[in]      lpServiceStartName - A pointer to a null-terminated UNICODE string that specifies
     *                                      the name of the account under which the service SHOULD run.
     * @param[in]      lpPassword         - A pointer to a null-terminated UNICODE string that contains the password
     *                                      of the account whose name was specified by the lpServiceStartName parameter.
     * @param[in]      dwPwSize           - The size, in bytes, of the password specified by the lpPassword parameter.
     * @param[out]     lpServiceHandle    - An LPSC_RPC_HANDLE data type that defines the handle to the newly created service record.
     * @param[out]     retValue           - This parameter contains the returned value of the operation.
     * 
     * @return         A proper NTSTATUS error code which represents only that the call to ALPC port was successful.
     *                 To check whether the operation was successfull, inspect the output parameter containing the
     *                 result returned by the server.
     */
    _Must_inspect_result_
    NTSTATUS XPF_API
    RCreateServiceW(
        _In_ const DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>& hSCManager,
        _In_ const DceNdrWstring& lpServiceName,
        _In_ const DceUniquePointer<DceNdrWstring>& lpDisplayName,
        _In_ DcePrimitiveType<uint32_t> dwDesiredAccess,
        _In_ DcePrimitiveType<uint32_t> dwServiceType,
        _In_ DcePrimitiveType<uint32_t> dwStartType,
        _In_ DcePrimitiveType<uint32_t> dwErrorControl,
        _In_ const DceNdrWstring& lpBinaryPathName,
        _In_ const DceUniquePointer<DceNdrWstring>& lpLoadOrderGroup,
        _Inout_ DceUniquePointer<DcePrimitiveType<uint32_t>>* lpdwTagId,
        _In_ const DceUniquePointer<DceConformantArray<DcePrimitiveType<uint8_t>>>& lpDependencies,
        _In_ DcePrimitiveType<uint32_t> dwDependSize,
        _In_ const DceUniquePointer<DceNdrWstring>& lpServiceStartName,
        _In_ const DceUniquePointer<DceConformantArray<DcePrimitiveType<uint8_t>>>& lpPassword,
        _In_ DcePrimitiveType<uint32_t> dwPwSize,
        _Out_ DcePrimitiveType<ALPC_RPC_CONTEXT_HANDLE>* lpServiceHandle,
        _Out_ DcePrimitiveType<uint32_t>* retValue
    ) noexcept(true)
    {
        //
        // DWORD RCreateServiceW(
        //   [in] SC_RPC_HANDLE hSCManager,
        //   [in, string, range(0, SC_MAX_NAME_LENGTH)] wchar_t* lpServiceName,
        //   [in, string, unique, range(0, SC_MAX_NAME_LENGTH)] wchar_t* lpDisplayName,
        //   [in] DWORD dwDesiredAccess,
        //   [in] DWORD dwServiceType,
        //   [in] DWORD dwStartType,
        //   [in] DWORD dwErrorControl,
        //   [in, string, range(0, SC_MAX_PATH_LENGTH)] wchar_t* lpBinaryPathName,
        //   [in, string, unique, range(0, SC_MAX_NAME_LENGTH)] wchar_t* lpLoadOrderGroup,
        //   [in, out, unique] LPDWORD lpdwTagId,
        //   [in, unique, size_is(dwDependSize)] LPBYTE lpDependencies,
        //   [in, range(0, SC_MAX_DEPEND_SIZE)] DWORD dwDependSize,
        //   [in, string, unique, range(0, SC_MAX_ACCOUNT_NAME_LENGTH)] wchar_t* lpServiceStartName,
        //   [in, unique, size_is(dwPwSize)] LPBYTE lpPassword,
        //   [in, range(0, SC_MAX_PWD_SIZE)] DWORD dwPwSize,
        //   [out] LPSC_RPC_HANDLE lpServiceHandle);
        //

        AlpcRpc::DceNdr::DceMarshallBuffer iBuffer{ (*this->m_Port).TransferSyntaxFlags() };
        AlpcRpc::DceNdr::DceMarshallBuffer oBuffer{ (*this->m_Port).TransferSyntaxFlags() };

        /* Preinit output parameters to neutral values. */
        *lpServiceHandle = {};
        *retValue = {};

        /* Serialize input. */
        iBuffer.Marshall(hSCManager)
               .Marshall(lpServiceName)
               .Marshall(lpDisplayName)
               .Marshall(dwDesiredAccess)
               .Marshall(dwServiceType)
               .Marshall(dwStartType)
               .Marshall(dwErrorControl)
               .Marshall(lpBinaryPathName)
               .Marshall(lpLoadOrderGroup)
               .Marshall(*lpdwTagId)
               .Marshall(lpDependencies)
               .Marshall(dwDependSize)
               .Marshall(lpServiceStartName)
               .Marshall(lpPassword)
               .Marshall(dwPwSize);
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
        oBuffer.Unmarshall(*lpdwTagId)
               .Unmarshall(*lpServiceHandle)
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
};  // class SvcCtlInterface
};  // namespace DceNdr
};  // namespace AlpcRpc
