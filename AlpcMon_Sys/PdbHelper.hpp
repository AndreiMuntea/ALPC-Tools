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
 * @brief       This extracts the program database information for an already opened file.
 *              The file must be a .exe or .dll.
 *              An HTTP request to http://msdl.microsoft.com/download/symbols is performed
 *              to retrieve the required .pdb file.
 *
 * @param[in]   FileHandle       - The handle to the opened module.
 * @param[in]   PdbDirectoryPath - The directory where to save the pdb on disk.
 *                                 This must exist.
 * @param[out]  Symbols          - Extracted symbols from the .pdb files.
 *
 * @return      A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
ExtractPdbSymbolInformation(
    _In_ HANDLE FileHandle,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbDirectoryPath,
    _Out_ xpf::Vector<xpf::pdb::SymbolInformation>* Symbols
) noexcept(true);

};  // namespace PdbHelper
