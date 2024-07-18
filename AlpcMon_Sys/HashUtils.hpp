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
#include "FileObject.hpp"

namespace KmHelper
{
namespace File
{

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
 * @param[in]      MappedFile  - The file mapped in memory.
 * @param[in]      HashType    - One of the HashType values.
 * @param[in,out]  Hash        - Will contain the resulted hash.
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
    _Inout_ SysMon::File::FileObject& MappedFile,
    _In_ _Const_ const HashType& HashType,
    _Inout_ xpf::Buffer& Hash
) noexcept(true);
};  // namespace File
};  // namespace KmHelper
