/**
 * @file        ALPC-Tools/Alpc-Installer/Helpers.hpp
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

#pragma once

#define WIN32_NO_STATUS
    #include <Windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;


/**
 * @brief Available architectures.
 *        To avoid ifdefs and inconsistencies we group them here.
 */
enum class OsArchitecture
{
    /**
     * @brief   Corresponds to _M_IX86
     */
    ix86 = 0,

    /**
     * @brief   Corresponds to _M_AMD64
     */
    amd64 = 1,

    /**
     * @brief A canary value so we can find how many
     *        architectures we have defined so far.
     *
     * @note  Do not add anything below this.
     */
    MAX
};  // enum class OsArchitecture

/**
 * @brief   Returns the current architecture of the OS.
 *          It is intended to avoid ifdefs in many places.
 *          Better to have them here and be consistent.
 *
 * @return  Architecture.
 */
OsArchitecture
HelperCurrentOsArchitecture(
    void
) noexcept(true);

/**
 * @brief       Uses Resource API to find and drop a specific resource.
 *
 * @param[in]   ResourceId  - The id of the resource.
 *
 * @param[in]   DropPath    - The fully qualified path where the resource should be dropped on disk.
 *
 * @return      A proper NTSTATUS error code.
 */
NTSTATUS
HelperDropResource(
    _In_ DWORD ResourceId,
    _In_ const fs::path& DropPath
) noexcept(true);

/**
 * @brief       Helper method do delete a file.
 *              If the file can not be deleted, it is renamed and scheduled
 *              to be deleted at the next reboot.
 *
 * @param[in]   FilePath - The file to be deleted.
 *
 * @return      Nothing.
 */
void
HelperDeleteFile(
    _In_ const std::wstring_view& FilePath
) noexcept(true);

/**
 * @brief       Helper method to set the value of a registry key.
 *
 * @param[in]   Key         - A handle to an open registry key.
 *                            This handle is returned by the RegCreateKeyEx or RegOpenKeyEx function,
 *                            or it can be one of the following predefined keys:
 *                            HKEY_CLASSES_ROOT
 *                            HKEY_CURRENT_CONFIG
 *                            HKEY_CURRENT_USER
 *                            HKEY_LOCAL_MACHINE HKEY_USERS
 * @param[in]   Subkey      - The name of the registry subkey to be opened.
 *                            Key names are not case sensitive.
 * @param[in]   ValueName   - The name of the value to be set.
 *                            If a value with this name is not already present in the key, the function adds it to the key.
 * @param[in]   Type        - The type of data pointed to by the lpData parameter.
 *                            For a list of the possible types, see Registry Value Types in MSDN.
 * @param[in]   Data        - The data to be stored. For string-based types, such as REG_SZ,
 *                            the string must be null-terminated. With the REG_MULTI_SZ data type,
 *                            the string must be terminated with two null characters.
 * @param[in]   CbData      - The size of the information pointed to by the Data parameter, in bytes.
 *                            If the data is of type REG_SZ, REG_EXPAND_SZ, or REG_MULTI_SZ,
 *                            CbData must include the size of the terminating null character or characters.
 *
 * @return      A proper NTSTATUS error code.
 */
NTSTATUS
HelperSetRegistryKeyValue(
    _In_ HKEY Key,
    _In_ const std::wstring_view& Subkey,
    _In_ const std::wstring_view& ValueName,
    _In_ DWORD Type,
    _In_ const BYTE* Data,
    _In_ DWORD CbData
) noexcept(true);
