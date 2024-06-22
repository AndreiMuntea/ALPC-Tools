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


_Use_decl_annotations_
NTSTATUS XPF_API
PdbHelper::ExtractPdbInformationFromFile(
    _In_ HANDLE FileHandle,
    _Out_ xpf::String<wchar_t>* PdbGuidAndAge,
    _Out_ xpf::String<wchar_t>* PdbName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PVOID viewBase = NULL;
    size_t viewSize = 0;

    ULONG debugEntrySize = 0;
    PIMAGE_DEBUG_DIRECTORY debugEntryDirectory = NULL;

    ULONG debugEntriesCount = 0;
    PIMAGE_DEBUG_DIRECTORY imageDebugCodeViewSection = NULL;
    CODEVIEW_PDB_INFO* rawData = NULL;

    WCHAR buffer[100] = { 0 };
    UNICODE_STRING ustrBuffer = { 0 };
    xpf::StringView<wchar_t> bufferView;
    xpf::StringView<char> pdbName;

    /* The buffer we'll be using for printing data. */
    ::RtlInitEmptyUnicodeString(&ustrBuffer,
                                buffer,
                                sizeof(buffer));

    /* Clear output. */
    (*PdbGuidAndAge).Reset();
    (*PdbName).Reset();

    /* Map the file in the system addres space. */
    status = KmHelper::File::MapFileInSystem(FileHandle,
                                             &viewBase,
                                             &viewSize);
    if (!NT_SUCCESS(status))
    {
        viewBase = NULL;
        viewSize = 0;
        goto CleanUp;
    }

    /* Go to the debug directory. */
    debugEntryDirectory = static_cast<PIMAGE_DEBUG_DIRECTORY>(::RtlImageDirectoryEntryToData(viewBase,
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
    rawData = static_cast<CODEVIEW_PDB_INFO*>(xpf::AlgoAddToPointer(viewBase,
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
                                               *PdbName);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

CleanUp:
    /* Unmap the view. */
    KmHelper::File::UnMapFileInSystem(&viewBase);

    return status;
}

_Use_decl_annotations_
NTSTATUS XPF_API
PdbHelper::ResolvePdb(
    _In_ _Const_ const xpf::StringView<wchar_t>& FileName,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbGuidAndAge,
    _In_ _Const_ const xpf::StringView<wchar_t>& PdbDirectoryPath
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::String<char> url;
    xpf::http::HttpResponse response;
    xpf::SharedPointer<xpf::IClient> client;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    xpf::String<char> ansiFileName;
    xpf::String<char> ansiGuidAndAge;

    xpf::String<wchar_t> fullFilePath;
    HANDLE fileHandle = NULL;

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
    #define HELPER_APPEND_DATA_TO_STRING(string, data)          \
    {                                                           \
        status = string.Append(data);                           \
        if (!NT_SUCCESS(status))                                \
        {                                                       \
            return status;                                      \
        }                                                       \
    }

    HELPER_APPEND_DATA_TO_STRING(url, "http://msdl.microsoft.com/download/symbols/");
    HELPER_APPEND_DATA_TO_STRING(url, ansiFileName.View());
    HELPER_APPEND_DATA_TO_STRING(url, "/");
    HELPER_APPEND_DATA_TO_STRING(url, ansiGuidAndAge.View());
    HELPER_APPEND_DATA_TO_STRING(url, "/");
    HELPER_APPEND_DATA_TO_STRING(url, ansiFileName.View());

    HELPER_APPEND_DATA_TO_STRING(fullFilePath, PdbDirectoryPath);
    if (!PdbDirectoryPath.EndsWith(L"\\", false))
    {
        HELPER_APPEND_DATA_TO_STRING(fullFilePath, L"\\");
    }
    HELPER_APPEND_DATA_TO_STRING(fullFilePath, PdbGuidAndAge);
    HELPER_APPEND_DATA_TO_STRING(fullFilePath, L"_");
    HELPER_APPEND_DATA_TO_STRING(fullFilePath, FileName);

    #undef HELPER_APPEND_DATA_TO_STRING

    /* Check if the pdb is there. */
    status = KmHelper::File::OpenFile(fullFilePath.View(),
                                      KmHelper::File::FileAccessType::kRead,
                                      &fileHandle);
    if (NT_SUCCESS(status))
    {
        KmHelper::File::CloseFile(&fileHandle);
        return STATUS_SUCCESS;
    }

    /* Connect to msdl to grab the .pdb file. This will also grab the first chunk. */
    status = xpf::http::InitiateHttpDownload(url.View(),
                                             headerItems,
                                             XPF_ARRAYSIZE(headerItems),
                                             response,
                                             client);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Open the file and write the first chunk. */
    status = KmHelper::File::OpenFile(fullFilePath.View(),
                                      KmHelper::File::FileAccessType::kWrite,
                                      &fileHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    status = KmHelper::File::WriteFile(fileHandle,
                                       reinterpret_cast<const uint8_t*>(response.Body.Buffer()),
                                       response.Body.BufferSize());
    if (!NT_SUCCESS(status))
    {
        KmHelper::File::CloseFile(&fileHandle);
        return status;
    }

    /* No grab the rest of .pdb */
    bool hasMoreData = true;
    while (hasMoreData)
    {
        /* And keep downloading. */
        status = xpf::http::HttpContinueDownload(client,
                                                 response,
                                                 &hasMoreData);
        if (!NT_SUCCESS(status))
        {
            break;
        }

        if (!response.Body.IsEmpty())
        {
            status = KmHelper::File::WriteFile(fileHandle,
                                               reinterpret_cast<const uint8_t*>(response.Body.Buffer()),
                                               response.Body.BufferSize());
            if (!NT_SUCCESS(status))
            {
                break;
            }
        }
    }

    KmHelper::File::CloseFile(&fileHandle);
    return status;
}