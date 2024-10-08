/**
 * @file        ALPC-Tools/AlpcMon_Sys/PdbHelper.cpp
 *
 * @brief       In this file we define helper methods to interact with
 *              pdb files so we can use them throughout the project.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright � Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#pragma once

#include "precomp.hpp"

#include "KmHelper.hpp"
#include "HashUtils.hpp"
#include "globals.hpp"

#include "PdbHelper.hpp"
#include "trace.hpp"

/**
 * @brief   The functionality related to pdb interaction is heavy!
 *          All code is paged as it is intended to use work items.
 *          And not do anything at higher irqls.
 */
XPF_SECTION_PAGED;

/**
 *  @brief      See https://www.debuginfo.com/articles/debuginfomatch.html
 *
 *  @details    "When debug information for an executable is stored in PDB file,
 *               the executable�s debug directory contains an entry of type IMAGE_DEBUG_TYPE_CODEVIEW.
 *               This entry points to a small data block, which tells the debugger where to look for the PDB file.
 *               But before we proceed to the details of the data stored in this block, a word about CodeView
 *               debug information in general should be said. If we look at CodeView format specification
 *               (available in older versions of MSDN), we can notice that several kinds of CodeView information exist.
 *               Since all of them are called �CodeView� and use the same type of debug directory entry
 *               (IMAGE_DEBUG_TYPE_CODEVIEW), debuggers must be given a way to determine which CodeView format is actually used.
 *               This is achieved with the help of a DWORD-sized signature, which is always placed at the beginning of 
 *               CodeView debug information. The most known signatures for CodeView debug information stored in the
 *               executable are "NB09" (CodeView 4.10) and "NB11" (CodeView 5.0). When CodeView information refers to a PDB file,
 *               the signature can be "NB10" (which is used with PDB 2.0 files) or "RSDS" (for PDB 7.0 files)."
 */
typedef struct _CODEVIEW_PDB_INFO
{
    /**
     * @brief   Indicates that the pdb format is 2.0.
     */
    #define CODEVIEW_PDB_NB10_SINGATURE '90BN'

    /**
     * @brief   Indicates that the pdb format is 7.0.
     */
    #define CODEVIEW_PDB_RSDS_SINGATURE 'SDSR'

    /**
     *  @brief  Equals to "NB10" for PDB 2.0 files or "RSDS" for PDB 7.0 files.
     */
    uint32_t    CodeViewSignature;

    union
    {
        struct CODEVIEW_INFO_PDB20
        {
            /**
             * @brief    CodeView offset. Set to 0, because debug information is usually stored in a separate file. 
             */
            uint32_t    Offset;

            /**
             *  @brief   The time when debug information was created (in seconds since 01.01.1970) 
             */
            uint32_t    Signature;

            /**
             * @brief   Ever-incrementing value, which is initially set to 1 and incremented every
             *          time when a part of the PDB file is updated without rewriting the whole file. 
             */
            uint32_t    Age;

            /**
             * @brief   Null-terminated name of the PDB file. It can also contain full or partial path to the file.
             */
            char        PdbFileName[1];
        } Pdb20;

        struct CODEVIEW_INFO_PDB70
        {
            /**
             * @brief   A unique identifier, which changes with every rebuild of the executable and PDB file. 
             */
            uuid_t      Signature;

            /**
             * @brief   Ever-incrementing value, which is initially set to 1 and incremented every
             *          time when a part of the PDB file is updated without rewriting the whole file.
             */
            uint32_t    Age;

            /**
             * @brief   Null-terminated name of the PDB file. It can also contain full or partial path to the file.
             */
            char        PdbFileName[1];
        } Pdb70;
    } Info;
} CODEVIEW_PDB_INFO;

/**
 * @brief           This extracts the program database information from an already opened file.
 *                  The file must have been opened with read access. The file must be a dll or executable.
 *
 * @param[in,out]   File            - The opened module file.
 * @param[out]      PdbGuidAndAge   - Extracted PDB information required to download the pdb symbol.
 * @param[out]      PdbName         - Extracted PDB name required to download the pdb symbol.
 *
 * @return          A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS XPF_API
PdbHelperExtractPdbInformationFromFile(
    _Inout_ SysMon::File::FileObject& File,
    _Out_ xpf::String<wchar_t>* PdbGuidAndAge,
    _Out_ xpf::String<wchar_t>* PdbName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Buffer bufferFile{ SYSMON_PAGED_ALLOCATOR };

    ULONG debugEntrySize = 0;
    PIMAGE_DEBUG_DIRECTORY debugEntryDirectory = NULL;

    ULONG debugEntriesCount = 0;
    PIMAGE_DEBUG_DIRECTORY imageDebugCodeViewSection = NULL;
    CODEVIEW_PDB_INFO* rawData = NULL;

    WCHAR buffer[100] = { 0 };
    UNICODE_STRING ustrBuffer = { 0 };
    xpf::StringView<wchar_t> bufferView;
    xpf::StringView<char> pdbName;

    xpf::String<wchar_t> widePdbName{ SYSMON_PAGED_ALLOCATOR };

    /* The buffer we'll be using for printing data. */
    ::RtlInitEmptyUnicodeString(&ustrBuffer,
                                buffer,
                                sizeof(buffer));

    /* Clear output. */
    (*PdbGuidAndAge).Reset();
    (*PdbName).Reset();

    /* We limit this to size_t max bytes. */
    if (File.FileSize() > xpf::NumericLimits<size_t>::MaxValue())
    {
        status = STATUS_FILE_TOO_LARGE;
        goto CleanUp;
    }

    /* Read the file in the system addres space. */
    status = bufferFile.Resize(static_cast<size_t>(File.FileSize()));
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }
    status = File.Read(0, &bufferFile);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Go to the debug directory. */
    debugEntryDirectory = static_cast<PIMAGE_DEBUG_DIRECTORY>(::RtlImageDirectoryEntryToData(bufferFile.GetBuffer(),
                                                                                             FALSE,
                                                                                             IMAGE_DIRECTORY_ENTRY_DEBUG,
                                                                                             &debugEntrySize));
    if (NULL == debugEntryDirectory || 0 == debugEntrySize)
    {
        status = STATUS_INVALID_IMAGE_FORMAT;
        goto CleanUp;
    }

    /* Find the codeview PDBINFO directory section. */
    debugEntriesCount = debugEntrySize / sizeof(IMAGE_DEBUG_DIRECTORY);
    for (ULONG i = 0; i < debugEntriesCount; ++i)
    {
        if (debugEntryDirectory[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW)
        {
            imageDebugCodeViewSection = &debugEntryDirectory[i];
            break;
        }
    }
    if (NULL == imageDebugCodeViewSection)
    {
        status = STATUS_INVALID_IMAGE_FORMAT;
        goto CleanUp;
    }

    /* Print the signature and age to a buffer. */
    rawData = static_cast<CODEVIEW_PDB_INFO*>(xpf::AlgoAddToPointer(bufferFile.GetBuffer(),
                                                                    imageDebugCodeViewSection->PointerToRawData));

    if (rawData->CodeViewSignature == CODEVIEW_PDB_NB10_SINGATURE)
    {
        pdbName = xpf::StringView<char>(rawData->Info.Pdb20.PdbFileName);

        status = ::RtlUnicodeStringPrintf(&ustrBuffer,
                                          L"%02X%x",
                                          rawData->Info.Pdb20.Signature,
                                          rawData->Info.Pdb20.Age);
    }
    else if (rawData->CodeViewSignature == CODEVIEW_PDB_RSDS_SINGATURE)
    {
        pdbName = xpf::StringView<char>(rawData->Info.Pdb70.PdbFileName);

        status = ::RtlUnicodeStringPrintf(&ustrBuffer,
                                          L"%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x",
                                          rawData->Info.Pdb70.Signature.Data1,
                                          rawData->Info.Pdb70.Signature.Data2,
                                          rawData->Info.Pdb70.Signature.Data3,
                                          rawData->Info.Pdb70.Signature.Data4[0],
                                          rawData->Info.Pdb70.Signature.Data4[1],
                                          rawData->Info.Pdb70.Signature.Data4[2],
                                          rawData->Info.Pdb70.Signature.Data4[3],
                                          rawData->Info.Pdb70.Signature.Data4[4],
                                          rawData->Info.Pdb70.Signature.Data4[5],
                                          rawData->Info.Pdb70.Signature.Data4[6],
                                          rawData->Info.Pdb70.Signature.Data4[7],
                                          rawData->Info.Pdb70.Age);
    }
    else
    {
        status = STATUS_INVALID_IMAGE_FORMAT;
    }
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Now properly set output parameter. */
    status = KmHelper::HelperUnicodeStringToView(ustrBuffer,
                                                 bufferView);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }
    status = PdbGuidAndAge->Append(bufferView);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    status = xpf::StringConversion::UTF8ToWide(pdbName,
                                               widePdbName);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    status = PdbName->Append(widePdbName.View());
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

CleanUp:
    return status;
}

/**
 * @brief       Helper method to compute the full pdb path of a file.
 *
 * @param[in]   FileName         - Name of the file for which the pdb is requested.
 *                                 For example "ntdll"
 * @param[in]   PdbGuidAndAge    - As there can be multiple ntdll versions, the specific
 *                                 version is identified by its guid and age.
 * @param[in]   PdbDirectoryPath - The directory where to save the pdb on disk.
 *                                 This must exist.
 * @param[out]  PdbFullFilePath  - Will store the full pdb file path. Its form will be:
 *                                 PdbDirectoryPath/PdbGuidAndAge_FileName.
 *
 * @return      A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS XPF_API
PdbHelperComputePdbFullFilePath(
    _In_ _Const_ const xpf::StringView<wchar_t>& FileName,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbGuidAndAge,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbDirectoryPath,
    _Out_ xpf::String<wchar_t>* PdbFullFilePath
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();
    XPF_DEATH_ON_FAILURE(nullptr != PdbFullFilePath);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Preinit output. */
    PdbFullFilePath->Reset();

    /* Helper macro to help build the full file path */
    #ifndef DOXYGEN_SHOULD_SKIP_THIS
        #define HELPER_APPEND_DATA_TO_STRING(string, data)          \
        {                                                           \
            status = string->Append(data);                          \
            if (!NT_SUCCESS(status))                                \
            {                                                       \
                return status;                                      \
            }                                                       \
        }
    #endif  // DOXYGEN_SHOULD_SKIP_THIS

    /* Construct path. */
    HELPER_APPEND_DATA_TO_STRING(PdbFullFilePath, PdbDirectoryPath);
    if (!PdbDirectoryPath.EndsWith(L"\\", false))
    {
        HELPER_APPEND_DATA_TO_STRING(PdbFullFilePath, L"\\");
    }
    HELPER_APPEND_DATA_TO_STRING(PdbFullFilePath, PdbGuidAndAge);
    HELPER_APPEND_DATA_TO_STRING(PdbFullFilePath, L"_");
    HELPER_APPEND_DATA_TO_STRING(PdbFullFilePath, FileName);

    /* Macro no longer needed */
    #undef HELPER_APPEND_DATA_TO_STRING

    /* All good. */
    return STATUS_SUCCESS;
}

/**
 * @brief       Checks if pdb is available locally, otherwise it downloads the pdb for the given file name.
 *              The pdb file is saved on disk.
 *
 * @param[in]   FileName         - Name of the file for which the pdb is requested.
 *                                 For example "ntdll"
 * @param[in]   PdbGuidAndAge    - As there can be multiple ntdll versions, the specific
 *                                 version is identified by its guid and age.
 * @param[in]   PdbFullFilePath  - The location where to save the pdb on disk.
 *
 * @return      A proper NTSTATUS error code.
 *              On success, the PdbDirectoryPath/PdbGuidAndAge_FileName.pdb file is created.
 *
 * @note        An HTTP request to http://msdl.microsoft.com/download/symbols is performed.
 * @note        It is recommended to use a system thread for this functionality.
 *              Leverage work queue or threadpool mechanisms.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS XPF_API
PdbHelperResolvePdb(
    _In_ _Const_ const xpf::StringView<wchar_t>& FileName,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbGuidAndAge,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbFullFilePath
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::String<char> url{ SYSMON_PAGED_ALLOCATOR };
    xpf::SharedPointer<xpf::IClient> client{ SYSMON_PAGED_ALLOCATOR };
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    xpf::String<char> ansiFileName{ SYSMON_PAGED_ALLOCATOR };
    xpf::String<char> ansiGuidAndAge{ SYSMON_PAGED_ALLOCATOR };
    xpf::Optional<SysMon::File::FileObject> pdbFile;

    xpf::http::HttpResponse response;

    response.ResponseBuffer = xpf::SharedPointer<xpf::Buffer>(SYSMON_PAGED_ALLOCATOR);
    response.Headers = xpf::Move(xpf::Vector<xpf::http::HeaderItem>(SYSMON_PAGED_ALLOCATOR));

    /* Get the ansi name for request. */
    status = xpf::StringConversion::WideToUTF8(FileName, ansiFileName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = xpf::StringConversion::WideToUTF8(PdbGuidAndAge, ansiGuidAndAge);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Header items to be used. */
    const xpf::http::HeaderItem headerItems[] =
    {
        { "Accept",             "application/octet-stream" },
        { "Accept-Encoding",    "gzip, deflate, br" },
        { "User-Agent",         "Microsoft-Symbol-Server/10.0.10036.206" },
        { "Connection",         "close" },
    };

    /* Helper macro to help build the request path and the full file path */
    #ifndef DOXYGEN_SHOULD_SKIP_THIS
        #define HELPER_APPEND_DATA_TO_STRING(string, data)          \
        {                                                           \
            status = string.Append(data);                           \
            if (!NT_SUCCESS(status))                                \
            {                                                       \
                return status;                                      \
            }                                                       \
        }
    #endif  // DOXYGEN_SHOULD_SKIP_THIS

    HELPER_APPEND_DATA_TO_STRING(url, "http://msdl.microsoft.com/download/symbols/");
    HELPER_APPEND_DATA_TO_STRING(url, ansiFileName.View());
    HELPER_APPEND_DATA_TO_STRING(url, "/");
    HELPER_APPEND_DATA_TO_STRING(url, ansiGuidAndAge.View());
    HELPER_APPEND_DATA_TO_STRING(url, "/");
    HELPER_APPEND_DATA_TO_STRING(url, ansiFileName.View());

    #undef HELPER_APPEND_DATA_TO_STRING

    /* Check if the pdb is there. */
    status = SysMon::File::FileObject::Create(PdbFullFilePath,
                                              XPF_FILE_ACCESS_READ,
                                              &pdbFile);
    if (NT_SUCCESS(status))
    {
        return STATUS_SUCCESS;
    }

    /* Connect to msdl to grab the .pdb file. This will also grab the first chunk. */
    status = xpf::http::InitiateHttpDownload(url.View(),
                                             headerItems,
                                             XPF_ARRAYSIZE(headerItems),
                                             &response,
                                             client);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Open the file and write the first chunk. */
    status = SysMon::File::FileObject::Create(PdbFullFilePath,
                                              XPF_FILE_ACCESS_WRITE,
                                              &pdbFile);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = (*pdbFile).Write(response.Body.Buffer(),
                              response.Body.BufferSize());
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* No grab the rest of .pdb */
    bool hasMoreData = true;
    while (hasMoreData)
    {
        /* And keep downloading. */
        status = xpf::http::HttpContinueDownload(client,
                                                 &response,
                                                 &hasMoreData);
        if (!NT_SUCCESS(status))
        {
            break;
        }

        if (!response.Body.IsEmpty())
        {
            status = (*pdbFile).Write(response.Body.Buffer(),
                                      response.Body.BufferSize());
            if (!NT_SUCCESS(status))
            {
                break;
            }
        }
    }
    return status;
}

_Use_decl_annotations_
NTSTATUS XPF_API
PdbHelper::ExtractPdbSymbolInformation(
    _Inout_ SysMon::File::FileObject& File,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbDirectoryPath,
    _Out_ xpf::Vector<xpf::pdb::SymbolInformation>* Symbols
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();
    XPF_DEATH_ON_FAILURE(nullptr != Symbols);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    xpf::Optional<SysMon::File::FileObject> pdbFile;
    xpf::Buffer pdbFileBuffer{ SYSMON_PAGED_ALLOCATOR };

    xpf::String<wchar_t> pdbGuidAndAge{ SYSMON_PAGED_ALLOCATOR };
    xpf::String<wchar_t> pdbName{ SYSMON_PAGED_ALLOCATOR };
    xpf::String<wchar_t> pdbFullFilePath{ SYSMON_PAGED_ALLOCATOR };

    /* Preinit output. */
    Symbols->Clear();

    /* Grab details about pdb. */
    status = PdbHelperExtractPdbInformationFromFile(File,
                                                    &pdbGuidAndAge,
                                                    &pdbName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = PdbHelperComputePdbFullFilePath(pdbName.View(),
                                             pdbGuidAndAge.View(),
                                             PdbDirectoryPath,
                                             &pdbFullFilePath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Ensure the pdb exists. */
    status = PdbHelperResolvePdb(pdbName.View(),
                                 pdbGuidAndAge.View(),
                                 pdbFullFilePath.View());
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Map it in memory. */
    status = SysMon::File::FileObject::Create(pdbFullFilePath.View(),
                                              XPF_FILE_ACCESS_READ,
                                              &pdbFile);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Now extract information. */
    if ((*pdbFile).FileSize() > xpf::NumericLimits<size_t>::MaxValue())
    {
        return STATUS_FILE_TOO_LARGE;
    }
    status = pdbFileBuffer.Resize(static_cast<size_t>((*pdbFile).FileSize()));
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = (*pdbFile).Read(0, &pdbFileBuffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* This assumes an aligned size - so fill with 0. */
    const size_t alignedSize = xpf::AlgoAlignValueUp(pdbFileBuffer.GetSize(),
                                                     size_t{PAGE_SIZE});
    if (alignedSize < pdbFileBuffer.GetSize())
    {
        return STATUS_FILE_TOO_LARGE;
    }
    status = pdbFileBuffer.Resize(alignedSize);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* And finally extract the symbols. */
    return xpf::pdb::ExtractSymbols(pdbFileBuffer.GetBuffer(),
                                    pdbFileBuffer.GetSize(),
                                    Symbols);
}
