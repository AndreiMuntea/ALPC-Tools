/**
 * @file        ALPC-Tools/AlpcMon_Sys/HashUtils.hpp
 *
 * @brief       In this file we define helper methods about hashing
 *              so we can use them throughout the project.
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

namespace KmHelper
{
namespace File
{

//
// ************************************************************************************************
// *                                FILE-RELATED FUNCTIONALITY.                                   *
// ************************************************************************************************
//

/**
 * @brief   This describes the file access mode.
 *          We're not defining all of them here.
 *          More will be added as required.
 */
enum class FileAccessType
{
    /**
     * @brief   Requires read access for the file.
     *          Equivalent to FILE_GENERIC_READ
     */
    kRead = 1,

    /**
     * @brief   Requires write access to the file.
     *          Equivalent to FILE_GENERIC_WRITE.
     */
    kWrite = 2,
};  // enum class FileAccessType

/**
 * @brief       This is used to open a file by its path.
 *
 * @param[in]   Path        - The file path to be opened.
 * @param[in]   AccessType  - Access requested for open.
 *                          - when kRead is required, we'll open with FILE_GENERIC_READ
 *                            and we'll allow share read access.
 * @param[out]  Handle      - A handle to the opened file, null on failure.
 *
 * @return      A proper NTSTATUS error code.
 *
 * @note    It is recommended to open files from a separated system thread to avoid potential deadlocks.
 *          Leverage Work-Queue mechanism. Use this routine with care!
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
OpenFile(
    _In_ _Const_ const xpf::StringView<wchar_t>& Path,
    _In_ _Const_ const FileAccessType& AccessType,
    _Out_ PHANDLE Handle
) noexcept(true);

/**
 * @brief          Closes a previously opened file handle.
 *
 * @param[in,out]  Handle - A handle to the opened file.
 *
 * @return         Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
CloseFile(
    _Inout_ PHANDLE Handle
) noexcept(true);


/**
 * @brief       This is used to write a buffer to a file.
 *
 * @param[in]   FileHandle  - A handle to the opened file.
 * @param[in]   Buffer      - A buffer to be written.
 * @param[in]   BufferSize  - Number of bytes in Buffer.
 *
 * @return      A proper NTSTATUS error code.
 *
 * @note        It is recommended to open files from a separated system thread to avoid potential deadlocks.
 *              Leverage Work-Queue mechanism. Use this routine with care!
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
WriteFile(
    _In_ HANDLE FileHandle,
    _In_ const uint8_t* Buffer,
    _In_ size_t BufferSize
) noexcept(true);

/**
 * @brief       This is used to query the file size for an already opened file handle.
 *
 * @param[in]   FileHandle  - A handle to the opened file, null on failure.
 * @param[out]  FileSize    - The size of the file.
 *
 * @return      A proper NTSTATUS error code.
 *
 * @note        It is recommended to open files from a separated system thread to avoid potential deadlocks.
 *              Leverage Work-Queue mechanism. Use this routine with care!
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
QueryFileSize(
    _In_ HANDLE FileHandle,
    _Out_ uint64_t* FileSize
) noexcept(true);


/**
 * @brief           This will first attempt to query the name of the
 *                  object by using only the cache. If that fails,
 *                  it falls back to querying the file name from
 *                  a dedicated worker thread.
 *
 * @param[in]       FileObject  - The object we want to query.
 * @param[in,out]   FileName    - Retrieved file name.
 *
 * @return          A proper ntstatus error code.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
QueryFileNameFromObject(
    _In_ PVOID FileObject,
    _Inout_ xpf::String<wchar_t, xpf::SplitAllocator>& FileName
) noexcept(true);


//
// ************************************************************************************************
// *                                SECTION-RELATED FUNCTIONALITY.                                *
// ************************************************************************************************
//


/**
 * @brief       This will map a file in the system process context.
 *
 * @param[in]   FileHandle - A handle to the opened file, null on failure.
 * @param[out]  ViewBase   - Contains a kernel address where the file is mapped.
 *                           Reading-writing from this base requires proper SEH guards!
 * @param[out]  ViewSize   - The size of the view - rounded down to file size.
 *
 * @return      A proper NTSTATUS error code.
 *
 * @note        It is recommended to open files from a separated system thread to avoid potential deadlocks.
 *              Leverage Work-Queue mechanism. Use this routine with care!
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
MapFileInSystem(
    _In_ HANDLE FileHandle,
    _Outptr_result_bytebuffer_(*ViewSize) PVOID* ViewBase,
    _Out_ size_t* ViewSize
) noexcept(true);

/**
 * @brief          Unmaps a previously mapped system view.
 *
 * @param[in,out]  ViewBase - Contains a kernel address where the file is mapped.
 *
 * @return         Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
UnMapFileInSystem(
    _Inout_ PVOID* ViewBase
) noexcept(true);


//
// ************************************************************************************************
// *                                HASH-RELATED FUNCTIONALITY.                                   *
// ************************************************************************************************
//


/**
 * @brief   This describes the hash methods supported.
 */
enum class HashType
{
    /**
     * @brief   MD5 can be used as a checksum to verify data integrity against unintentional corruption.
     *          Historically it was widely used as a cryptographic hash function; however it has been
     *          found to suffer from extensive vulnerabilities. It remains suitable for other non-cryptographic purposes.
     */
    kMd5 = 1
};  // enum class HashType

/**
 * @brief          This will hash a file in the system process context.
 *                 It will first map the file in the system process.
 *
 * @param[in]      FileHandle  - A handle to the opened file.
 * @param[in]      HashType    - One of the HashType values.
 *
 * @return         A proper NTSTATUS error code.
 *
 * @note           It is recommended to open files from a separated system thread to avoid potential deadlocks.
 *                 Leverage Work-Queue mechanism. Use this routine with care!
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
HashFile(
    _In_ HANDLE FileHandle,
    _In_ _Const_ const HashType& HashType,
    _Inout_ xpf::Buffer<xpf::SplitAllocator>& Hash
) noexcept(true);
};  // namespace File
};  // namespace KmHelper
