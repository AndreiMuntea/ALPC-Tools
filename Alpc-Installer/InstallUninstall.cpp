/**
 * @file        ALPC-Tools/Alpc-Installer/InstallUninstall.cpp
 *
 * @brief       In this file we define the methods for aiding us in
 *              installation and uninstalation.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "InstallUninstall.hpp"
#include "resource.h"

//
// These are globals as they needs to be the same for install - uninstall.
//
static constexpr std::wstring_view gInstallDir      = L"\\\\?\\C:\\SysMon\\";
static constexpr std::wstring_view gServiceKey      = L"System\\CurrentControlSet\\Services\\AlpcMon_Sys";

static constexpr std::wstring_view gServiceName     = L"AlpcMon_Sys";
static constexpr std::wstring_view gServicePath     = L"\\\\?\\C:\\SysMon\\AlpcMon_Sys.sys";

static constexpr std::wstring_view gUmDllWin32Path  = L"\\\\?\\C:\\SysMon\\AlpcMon_DllWin32.dll";
static constexpr std::wstring_view gUmDllx64Path    = L"\\\\?\\C:\\SysMon\\AlpcMon_Dllx64.dll";

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper method to drop the required files.                                 |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static NTSTATUS
MakeInstallFolder(
    void
) noexcept(true)
{
    try
    {
        /* First step is to create the instalation directory. */
        printf("[*] Creating installation directory: %S \r\n",
               gInstallDir.data());
        if (!fs::exists(gInstallDir))
        {
            fs::create_directories(gInstallDir);
        }

        /* Macro to help us drop resources. */
        #define DROP_RESOURCE(path, resourceId)                             \
        {                                                                   \
            NTSTATUS status = HelperDropResource(resourceId,                \
                                                 path);                     \
            if (status != STATUS_SUCCESS)                                   \
            {                                                               \
                printf("[!] Failed to drop resource %d status 0x%x r\n",    \
                       resourceId,                                          \
                       status);                                             \
                return status;                                              \
            }                                                               \
        };

        /* Then we drop all required files - this is dependant on architecture */
        OsArchitecture architecture = HelperCurrentOsArchitecture();

        /* Injection dlls are dropped regardless. */
        DROP_RESOURCE(gUmDllWin32Path,   IDR_INSTALLFILE2);
        DROP_RESOURCE(gUmDllx64Path,     IDR_INSTALLFILE4);

        /* Then the os-specific data. */
        if (architecture == OsArchitecture::ix86)
        {
            DROP_RESOURCE(gServicePath, IDR_INSTALLFILE1);

            return STATUS_SUCCESS;
        }
        else if (architecture == OsArchitecture::amd64)
        {
            DROP_RESOURCE(gServicePath, IDR_INSTALLFILE3);

            return STATUS_SUCCESS;
        }
        else
        {
            return STATUS_UNKNOWN_REVISION;
        }

        #undef DROP_RESOURCE
    }
    catch (...)
    {
        return STATUS_UNHANDLED_EXCEPTION;
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper method to delete the dropped folder.                               |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void
DeleteInstallFolder(
    void
) noexcept(true)
{
    try
    {
        for (const auto& path : fs::recursive_directory_iterator(gInstallDir))
        {
            HelperDeleteFile(path.path().c_str());
        }
        printf("[*] Cleared installation directory: %S \r\n",
               gInstallDir.data());
    }
    catch (...)
    {
        printf("[!] An exception was encountered while deleting the install dir! Manual removal required %S \r\n",
               gInstallDir.data());
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper method to create the service.                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static NTSTATUS
CreateSysmonService(
    void
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Get machine name. */
    WCHAR machineNameBuff[MAX_PATH + 1] = { 0 };
    DWORD machineNameBuffSize = MAX_PATH;

    if (FALSE == GetComputerNameW(machineNameBuff, &machineNameBuffSize))
    {
        printf("[!] Failed to retrieve computer name. gle = 0x%x.\r\n",
               ::GetLastError());
        return STATUS_INVALID_SERVER_STATE;
    }

    /* Open sc manager */
    SC_HANDLE scManagerHandle = ::OpenSCManagerW(machineNameBuff,
                                                 NULL,
                                                 SC_MANAGER_ALL_ACCESS);
    if (NULL == scManagerHandle)
    {
        printf("[!] Failed to OpenSCManagerW. gle = 0x%x.\r\n",
               ::GetLastError());
        return STATUS_INVALID_SERVER_STATE;
    }

    /* Create the service. */
    SC_HANDLE serviceHandle = ::CreateServiceW(scManagerHandle,
                                               gServiceName.data(),
                                               gServiceName.data(),
                                               SERVICE_ALL_ACCESS,
                                               SERVICE_KERNEL_DRIVER,
                                               SERVICE_DEMAND_START,
                                               SERVICE_ERROR_NORMAL,
                                               gServicePath.data(),
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL);
    if (NULL == serviceHandle)
    {
        printf("[!] Failed to CreateServiceW. gle = 0x%x.\r\n",
               ::GetLastError());

        status =  STATUS_INVALID_SERVER_STATE;
        goto CleanUp;
    }

    /* Add the registry key specifying the instalation directory. */
    status = HelperSetRegistryKeyValue(HKEY_LOCAL_MACHINE,
                                       gServiceKey,
                                       L"InstallDirectory",
                                       REG_SZ,
                                       reinterpret_cast<const BYTE*>(gInstallDir.data()),
                                       (gInstallDir.size() + 1) * sizeof(wchar_t));
    if (status != STATUS_SUCCESS)
    {
        printf("[!] HelperSetRegistryKeyValue failed with status 0x%x \r\n",
                status);
        goto CleanUp;
    }

    /* Start the service. */
    if (FALSE == ::StartServiceW(serviceHandle, 0, NULL))
    {
        printf("[!] Failed to StartServiceW. gle = 0x%x.\r\n",
               ::GetLastError());

        status =  STATUS_INVALID_SERVER_STATE;
        goto CleanUp;
    }

    /* All good. */
    status = STATUS_SUCCESS;

CleanUp:
    if (NULL != serviceHandle)
    {
        ::CloseServiceHandle(serviceHandle);
        serviceHandle = NULL;
    }
    if (NULL != scManagerHandle)
    {
        ::CloseServiceHandle(scManagerHandle);
        scManagerHandle = NULL;
    }
    return status;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper method to delete the service.                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

static void
DeleteSysmonService(
    void
) noexcept(true)
{
    /* Get machine name. */
    WCHAR machineNameBuff[MAX_PATH + 1] = { 0 };
    DWORD machineNameBuffSize = MAX_PATH;

    if (FALSE == GetComputerNameW(machineNameBuff, &machineNameBuffSize))
    {
        printf("[!] Failed to retrieve computer name. gle = 0x%x.\r\n", GetLastError());
        return;
    }

    /* Open sc manager */
    SC_HANDLE scManagerHandle = ::OpenSCManagerW(machineNameBuff,
                                                 NULL,
                                                 SC_MANAGER_ALL_ACCESS);
    if (NULL == scManagerHandle)
    {
        printf("[!] Failed to OpenSCManagerW. gle = 0x%x.\r\n", GetLastError());
        return;
    }

    /* Open the service. */
    SC_HANDLE serviceHandle = ::OpenServiceW(scManagerHandle,
                                             gServiceName.data(),
                                             SERVICE_ALL_ACCESS);
    if (NULL != serviceHandle)
    {
        /* Success - first we try to stop it. */
        SERVICE_STATUS serviceStatus = { 0 };
        if (FALSE == ::ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus))
        {
            printf("[!] Failed to ControlService. gle = 0x%x.\r\n", GetLastError());
        }
        
        /* Then we delete it. */
        if (FALSE == ::DeleteService(serviceHandle))
        {
            printf("[!] Failed to DeleteService. gle = 0x%x.\r\n", GetLastError());
        }

        /* Then we close the handle. */
        ::CloseServiceHandle(serviceHandle);
    }
    ::CloseServiceHandle(scManagerHandle);

    printf("[*] Finished deleting the service! \r\n");
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       DoInstall()                                                               |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

void
DoInstall(
    void
) noexcept(true)
{
    NTSTATUS status = MakeInstallFolder();
    if (status != STATUS_SUCCESS)
    {
        printf("[!] MakeInstallFolder failed with status 0x%x \r\n",
                status);
        goto CleanUp;
    }

    status = CreateSysmonService();
    if (status != STATUS_SUCCESS)
    {
        printf("[!] CreateSysmonService failed with status 0x%x \r\n",
                status);
        goto CleanUp;
    }

    printf("[*] Successfully installed the sensor solution! \r\n");

CleanUp:
    if (status != STATUS_SUCCESS)
    {
        DoUninstall();
    }
}

void
DoUninstall(
    void
) noexcept(true)
{
    DeleteSysmonService();
    DeleteInstallFolder();

    printf("[*] Finished uninstalling the sensor solution! \r\n");
}
