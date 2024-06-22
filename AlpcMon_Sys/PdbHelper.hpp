/**
 * @file        ALPC-Tools/AlpcMon_Sys/PdbHelper.hpp
 *
 * @brief       In this file we define helper methods to interact with
 *              pdb files so we can use them throughout the project.
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

namespace PdbHelper
{
/**
 * @brief       This extracts the program database information from an already opened file.
 *              The file must have been opened with read access. The file must be a dll or executable.
 *
 * @param[in]   FileHandle      - The handle to the opened module.
 * @param[out]  PdbGuidAndAge   - Extracted PDB information required to download the pdb symbol.
 * @param[out]  PdbName         - Extracted PDB name required to download the pdb symbol.
 *
 * @return      A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
ExtractPdbInformationFromFile(
    _In_ HANDLE FileHandle,
    _Out_ xpf::String<wchar_t>* PdbGuidAndAge,
    _Out_ xpf::String<wchar_t>* PdbName
) noexcept(true);

/**
 * @brief       Checks if pdb is available locally, otherwise it downloads the pdb for the given file name.
 *              The pdb file is saved on disk.
 *
 * @param[in]   FileName         - Name of the file for which the pdb is requested.
 *                                 For example "ntdll"
 * @param[in]   PdbGuidAndAge    - As there can be multiple ntdll versions, the specific
 *                                 version is identified by its guid and age.
 * @param[in]   PdbDirectoryPath - The directory where to save the pdb on disk.
 *                                 This must exist.
 *
 * @return      A proper NTSTATUS error code.
 *              On success, the <PdbDirectoryPath>\\<FileName>_<MD5>.pdb file is created.
 *
 * @note        An HTTP request to http://msdl.microsoft.com/download/symbols is performed.
 * @note        It is recommended to use a system thread for this functionality.
 *              Leverage work queue or threadpool mechanisms.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
ResolvePdb(
    _In_ _Const_ const xpf::StringView<wchar_t>& FileName,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbGuidAndAge,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbDirectoryPath
) noexcept(true);
};  // namespace PdbHelper
