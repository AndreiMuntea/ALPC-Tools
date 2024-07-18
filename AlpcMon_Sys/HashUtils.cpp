/**
 * @file        ALPC-Tools/AlpcMon_Sys/HashUtils.cpp
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

#include "precomp.hpp"

#include "HashUtils.hpp"
#include "trace.hpp"


/**
 * @brief   All hashing code is paged.
 */
XPF_SECTION_PAGED;

//
// ************************************************************************************************
// *                                HASH-RELATED FUNCTIONALITY.                                   *
// ************************************************************************************************
//

_Use_decl_annotations_
NTSTATUS
KmHelper::File::HashFile(
    _Inout_ SysMon::File::FileObject& MappedFile,
    _In_ _Const_ const HashType& HashType,
    _Inout_ xpf::Buffer& Hash
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    BCRYPT_ALG_HANDLE algorithmHandle = { 0 };
    BOOLEAN isAlgorithmProviderOpened = FALSE;
    LPCWSTR algorithmId = NULL;

    BCRYPT_HASH_HANDLE hashHandle = { 0 };
    BOOLEAN isHashHandleCreated = FALSE;

    uint32_t hashLength = 0;
    ULONG cbResultPropertyLength = 0;

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::Buffer chunkBuffer{ SYSMON_PAGED_ALLOCATOR };

    /* First map the hash type to the bcrypt algorithm type. */
    switch (HashType)
    {
        case KmHelper::File::HashType::kMd5:
        {
            algorithmId = BCRYPT_MD5_ALGORITHM;
            break;
        }
        default:
        {
            XPF_ASSERT(false);
            return STATUS_INVALID_PARAMETER;
        }
    }

    /* Now open the algorithm provider. */
    status = ::BCryptOpenAlgorithmProvider(&algorithmHandle,
                                           algorithmId,
                                           MS_PRIMITIVE_PROVIDER,
                                           0);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }
    isAlgorithmProviderOpened = TRUE;

    /* Now create the hashing object. */
    status = ::BCryptCreateHash(algorithmHandle,
                                &hashHandle,
                                NULL,
                                0,
                                NULL,
                                0,
                                0);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }
    isHashHandleCreated = TRUE;

    /* We'll read the file in PAGE_SIZE chunks. */
    status = chunkBuffer.Resize(PAGE_SIZE);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* And now start hashing. */
    for (size_t i = 0; i < MappedFile.FileSize(); i += chunkBuffer.GetSize())
    {
        /* Read next chunk. */
        status = MappedFile.Read(i, &chunkBuffer);
        if (!NT_SUCCESS(status))
        {
            goto CleanUp;
        }
        if (chunkBuffer.GetSize() == 0)
        {
            status = STATUS_INVALID_BUFFER_SIZE;
            goto CleanUp;
        }

        /* Update the hash. */
        status = ::BCryptHashData(hashHandle,
                                  static_cast<PUCHAR>(chunkBuffer.GetBuffer()),
                                  static_cast<uint32_t>(chunkBuffer.GetSize()),
                                  0);
        if (!NT_SUCCESS(status))
        {
            goto CleanUp;
        }
    }

    /* Now find how many bytes we need for the hash result. */
    status = ::BCryptGetProperty(hashHandle,
                                 BCRYPT_HASH_LENGTH,
                                 reinterpret_cast<PUCHAR>(&hashLength),
                                 sizeof(hashLength),
                                 &cbResultPropertyLength,
                                 0);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }
    if (cbResultPropertyLength != sizeof(hashLength))
    {
        status = STATUS_INFO_LENGTH_MISMATCH;
        goto CleanUp;
    }

    /* Allocate buffer. */
    status = Hash.Resize(hashLength);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* And finalize the hashing. */
    status = ::BCryptFinishHash(hashHandle,
                                reinterpret_cast<PUCHAR>(Hash.GetBuffer()),
                                hashLength,
                                0);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

CleanUp:
    /* Cleanup the hash object. */
    if (FALSE != isHashHandleCreated)
    {
        NTSTATUS cleanupStatus = ::BCryptDestroyHash(hashHandle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(cleanupStatus));

        isHashHandleCreated = FALSE;
    }

    /* Cleanup the algorithm provider. */
    if (FALSE != isAlgorithmProviderOpened)
    {
        NTSTATUS cleanupStatus = ::BCryptCloseAlgorithmProvider(algorithmHandle,
                                                                0);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(cleanupStatus));

        isAlgorithmProviderOpened = FALSE;
    }

    return status;
}
