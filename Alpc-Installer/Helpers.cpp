/**
 * @file        ALPC-Tools/Alpc-Installer/Helpers.cpp
 *
 * @brief       In this file we define helper methods so we can use them
 *              throughout the project.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "Helpers.hpp"
#include <shlwapi.h>

OsArchitecture
HelperCurrentOsArchitecture(
    void
) noexcept(true)
{
    typedef BOOL (NTAPI * PFUNC_IsWow64Process2)(_In_   HANDLE  hProcess,
                                                 _Out_  USHORT *pProcessMachine,
                                                 _Out_  USHORT *pNativeMachine);

    PFUNC_IsWow64Process2 apiIsWow64Process2 = NULL;
    HMODULE kernel32 = ::GetModuleHandleW(L"Kernel32.dll");

    //
    // Resolve the dynamic functions first.
    //
    if (NULL != kernel32)
    {
        apiIsWow64Process2 = reinterpret_cast<PFUNC_IsWow64Process2>(::GetProcAddress(kernel32,
                                                                                      "IsWow64Process2"));
    }

    //
    // First we try the IsWow64Process2
    //
    if (NULL != apiIsWow64Process2)
    {
        USHORT processMachine = 0;
        USHORT nativeMachine = 0;

        BOOL result = apiIsWow64Process2(::GetCurrentProcess(),
                                         &processMachine,
                                         &nativeMachine);
        if (FALSE != result)
        {
            if (nativeMachine == IMAGE_FILE_MACHINE_I386)
            {
                return OsArchitecture::ix86;
            }
            else if (nativeMachine == IMAGE_FILE_MACHINE_AMD64)
            {
                return OsArchitecture::amd64;
            }
            else
            {
                return OsArchitecture::MAX;
            }
        }
    }

    //
    // If we failed, we fallback.
    //
    SYSTEM_INFO info = { 0 };
    ::GetNativeSystemInfo(&info);

    if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
    {
        return OsArchitecture::ix86;
    }
    else if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
    {
        return OsArchitecture::amd64;
    }
    else
    {
        return OsArchitecture::MAX;
    }
}

NTSTATUS
HelperDropResource(
    _In_ DWORD ResourceId,
    _In_ const fs::path& DropPath
) noexcept(true)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    HRSRC resource = NULL;
    HGLOBAL resourceLoadedHandle = NULL;

    void* resourcePtr = NULL;
    BOOL isResourceLocked = FALSE;
    DWORD resourceSize = 0;

    /* First step - find the resource. */
    resource = ::FindResourceW(::GetModuleHandleW(NULL),
                                MAKEINTRESOURCE(ResourceId),
                                L"INSTALLFILE");
    if (NULL == resource)
    {
        status = STATUS_RESOURCE_DATA_NOT_FOUND;
        goto CleanUp;
    }

    /* Then we need to load it. */
    resourceLoadedHandle = ::LoadResource(::GetModuleHandleW(NULL),
                                            resource);
    if (NULL == resourceLoadedHandle)
    {
        status = STATUS_RESOURCE_DATA_NOT_FOUND;
        goto CleanUp;
    }

    /* Then we need to lock it in place. */
    resourcePtr = ::LockResource(resourceLoadedHandle);
    if (NULL == resourcePtr)
    {
        status = STATUS_RESOURCE_DATA_NOT_FOUND;
        goto CleanUp;
    }
    isResourceLocked = TRUE;

    /* Get its size. */
    resourceSize = ::SizeofResource(::GetModuleHandleW(NULL),
                                    resource);

    /* And now drop it like it's hot. */
    try
    {
        std::ofstream file(DropPath, std::ios::binary);
        file.write(static_cast<const char*>(resourcePtr), resourceSize);

        status = STATUS_SUCCESS;
    }
    catch (...)
    {
        status = STATUS_UNHANDLED_EXCEPTION;
    }

CleanUp:
    if (isResourceLocked)
    {
        UnlockResource(resourcePtr);
        isResourceLocked = FALSE;
    }
    return status;
}

void
HelperDeleteFile(
    _In_ const std::wstring_view& FilePath
) noexcept(true)
{
    DeleteFileW(FilePath.data());
    if (!PathFileExistsW(FilePath.data()))
    {
        return;
    }

    LARGE_INTEGER perfCounter = { 0 };
    QueryPerformanceCounter(&perfCounter);

    try
    {
        std::wstring newFileName = std::wstring(FilePath) + std::to_wstring(perfCounter.QuadPart);

        fs::rename(FilePath, newFileName);
        MoveFileExW(newFileName.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    catch (...)
    {
        MoveFileExW(FilePath.data(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
}


NTSTATUS
HelperSetRegistryKeyValue(
    _In_ HKEY Key,
    _In_ const std::wstring_view& Subkey,
    _In_ const std::wstring_view& ValueName,
    _In_ DWORD Type,
    _In_ const BYTE* Data,
    _In_ DWORD CbData
) noexcept(true)
{
    HKEY resultedKey = NULL;
    LSTATUS retStatus = 0;

    retStatus = ::RegOpenKeyExW(Key,
                                Subkey.data(),
                                0,
                                KEY_ALL_ACCESS,
                                &resultedKey);
    if (retStatus != ERROR_SUCCESS)
    {
        return STATUS_REGISTRY_CORRUPT;
    }

    retStatus = ::RegSetValueExW(resultedKey,
                                 ValueName.data(),
                                 0,
                                 Type,
                                 Data,
                                 CbData);
    ::RegCloseKey(resultedKey);

    return (retStatus != ERROR_SUCCESS) ? STATUS_REGISTRY_CORRUPT
                                        : STATUS_SUCCESS;
}
