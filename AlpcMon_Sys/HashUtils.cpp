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

#include "globals.hpp"
#include "KmHelper.hpp"

#include "HashUtils.hpp"
#include "trace.hpp"

//
// ************************************************************************************************
// *                                This contains the non-paged section code.                     *
// ************************************************************************************************
//

/**
 * @brief MmSectionObjectType will be declared in non-paged area.
 */
XPF_SECTION_DEFAULT;

/**
 * @brief MmSectionObjectType needs to be demangled.
 */
XPF_EXTERN_C_START()

/**
 * @brief   See https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/ntos/mi/section.htm
 *
 * @details "The SECTION structure is not documented. Microsoft is not known to have disclosed even
 *           its name for—let alone any internal details of—whatever kernel-mode structure supports
 *           a handle to a section object. For the handful of exported functions, e.g.,
 *           MmMapViewInSystemSpace and MmMapViewInSessionSpace, that take a pointer to a section
 *           object as an argument and for which Microsoft has published C-language declarations in
 *           headers from a Windows Driver Kit (WDK), the argument’s type is simply PVOID.
 *           Even the referencing of the object from a handle is obscure: though the
 *           MmSectionObjectType variable is a kernel export as far back as version 3.51,
 *           it never has been declared in any WDK header except an NTOSP.H that Microsoft disclosed
 *           in early editions of the WDK for Windows 10 (apparently only by oversight)."
 */
extern POBJECT_TYPE* MmSectionObjectType;

/**
 * @brief MmSectionObjectType will be demangled. So we can stop with extern c now.
 */
XPF_EXTERN_C_END()


//
// ************************************************************************************************
// *                                This contains the paged section code.                         *
// ************************************************************************************************
//


XPF_SECTION_PAGED;


//
// ************************************************************************************************
// *                                FILE-RELATED FUNCTIONALITY.                                   *
// ************************************************************************************************
//

_Use_decl_annotations_
NTSTATUS
KmHelper::File::OpenFile(
    _In_ _Const_ const xpf::StringView<wchar_t>& Path,
    _In_ _Const_ const FileAccessType& AccessType,
    _Out_ PHANDLE Handle
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != Handle);

    UNICODE_STRING fileName = { 0 };
    IO_STATUS_BLOCK fileIoStatusBlock = { 0 };
    OBJECT_ATTRIBUTES fileAttributes = { 0 };
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    ACCESS_MASK desiredAccess = { 0 };
    DWORD shareAccess = 0;
    DWORD createDisposition = 0;


    /* First map the access type. */
    switch (AccessType)
    {
        case KmHelper::File::FileAccessType::kRead:
        {
            desiredAccess = FILE_GENERIC_READ;
            shareAccess = FILE_SHARE_READ;
            createDisposition = FILE_OPEN;
            break;
        }
        default:
        {
            XPF_ASSERT(false);
            return STATUS_INVALID_PARAMETER;
        }
    }

    /* Then convert the path to a proper unicode string. */
    status = KmHelper::HelperViewToUnicodeString(Path,
                                                 fileName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* And finally open the file. */
    InitializeObjectAttributes(&fileAttributes,
                               &fileName,
                               OBJ_KERNEL_HANDLE | OBJ_FORCE_ACCESS_CHECK,
                               NULL,
                               NULL);

    status = ::ZwCreateFile(Handle,
                            desiredAccess,
                            &fileAttributes,
                            &fileIoStatusBlock,
                            NULL,
                            FILE_ATTRIBUTE_NORMAL,
                            shareAccess,
                            createDisposition,
                            FILE_SYNCHRONOUS_IO_NONALERT,
                            NULL,
                            0);
    if (!NT_SUCCESS(status))
    {
        *Handle = NULL;
    }
    return status;
}

_Use_decl_annotations_
VOID
KmHelper::File::CloseFile(
    _Inout_ PHANDLE Handle
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (NULL == Handle)
    {
        return;
    }
    if (NULL != *Handle)
    {
        status = ::ZwClose(*Handle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));

        *Handle = NULL;
    }
}

_Use_decl_annotations_
NTSTATUS
KmHelper::File::QueryFileSize(
    _In_ HANDLE FileHandle,
    _Out_ uint64_t* FileSize
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != FileSize);

    IO_STATUS_BLOCK ioStatusBlock = { 0 };
    FILE_STANDARD_INFORMATION fileStandardInfo = { 0 };
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Query standard information. */
    status = ::ZwQueryInformationFile(FileHandle,
                                      &ioStatusBlock,
                                      &fileStandardInfo,
                                      sizeof(fileStandardInfo),
                                      FILE_INFORMATION_CLASS::FileStandardInformation);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* If fileStandardInfo was not properly populated we bail. */
    /* The Information member of iosb receives the number of */
    /* bytes that this routine actually writes to the FileInformation buffer. */
    if (ioStatusBlock.Information != sizeof(fileStandardInfo))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    /* This is a signed return value. Check if it's less than 0 first. */
    if (fileStandardInfo.EndOfFile.QuadPart < 0)
    {
        return STATUS_FILE_INVALID;
    }

    /* All good, convert to unsigned value.*/
    *FileSize = fileStandardInfo.EndOfFile.QuadPart;
    return STATUS_SUCCESS;
}

//
// ************************************************************************************************
// *                                FILE-OBJECT FUNCTIONALITY.                                    *
// ************************************************************************************************
//

/**
 * @brief   We are using this structure as an async context when resolving the file name
 *          from a worker thread.
 */
struct FileObjectFileNameContext
{
    /**
     * @brief   The File object whose name we want to query.
     */
    /* [in]  */    PVOID FileObject = nullptr;

    /**
     * @brief   The resulted file name.
     */
    /* [out] */    xpf::String<wchar_t> FileName;

    /**
     * @breif   The status of the operation. 
     */
    /* [out] */    NTSTATUS status = STATUS_UNSUCCESSFUL;
};

/**
 * @brief           A wrapper over FltGetFileNameInformationUnsafe.
 *
 * @param[in]       FileObject  - The object we want to query.
 * @param[in]       NameOptions - Settings for the query.
 * @param[in,out]   FileName    - Retrieved file name.
 * 
 * @return          A proper ntstatus error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS XPF_API
FltGetFileNameInformationUnsafeWrapper(
    _In_ PVOID FileObject,
    _In_ FLT_FILE_NAME_OPTIONS NameOptions,
    _Inout_ xpf::String<wchar_t>& FileName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    PFLT_FILE_NAME_INFORMATION fileNameInfo = nullptr;

    xpf::StringView<wchar_t> retrievedName;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* This can only be called with a file object type. */
    if ((nullptr == FileObject) || (*IoFileObjectType != ::ObGetObjectType(FileObject)))
    {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    /* Query the name. */
    status = ::FltGetFileNameInformationUnsafe(static_cast<PFILE_OBJECT>(FileObject),
                                               nullptr,
                                               NameOptions,
                                               &fileNameInfo);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Get a view over the retrieved name. */
    status = KmHelper::HelperUnicodeStringToView(fileNameInfo->Name,
                                                 retrievedName);
    if (NT_SUCCESS(status))
    {
        /* Append it to the name. */
        FileName.Reset();
        status = FileName.Append(retrievedName);
    }

    /* Now release the information. */
    ::FltReleaseFileNameInformation(fileNameInfo);

    /* And propagate status. */
    return status;
}

/**
 * @brief           Worker which can query the filesystem for the name.
 *                  This is intended to be executed always in the context
 *                  of a system thread so, don't try to run this from   
 *                  an arbitrary context.
 *
 * @param[in]       Argument - FileObjectFileNameContext pointer data.
 *
 * @return          Nothing.
 */
static void XPF_API
QueryFileNameFromObjectWorker(
    _In_opt_ xpf::thread::CallbackArgument Argument
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    /* Don't expect this to be null. */
    FileObjectFileNameContext* data = static_cast<FileObjectFileNameContext*>(Argument);
    if (nullptr == data)
    {
        XPF_ASSERT(false);
        return;
    }

    data->status = FltGetFileNameInformationUnsafeWrapper(data->FileObject,
                                                          FLT_FILE_NAME_NORMALIZED |
                                                          FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,
                                                          data->FileName);
}

/**
 * @brief           This will schedule the QueryFileNameFromObjectWorker.
 *                  By doing this, we can safely query the file name
 *                  as we can also query the file system itself.
 *
 * @param[in]       FileObject  - The object we want to query.
 * @param[in,out]   FileName    - Retrieved file name.
 * 
 * @return          A proper ntstatus error code.
 */
static NTSTATUS XPF_API
QueryFileNameFromObjectFallback(
    _In_ PVOID FileObject,
    _Inout_ xpf::String<wchar_t>& FileName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    FileObjectFileNameContext context;
    context.FileObject = FileObject;

    GlobalDataGetWorkQueueInstance()->EnqueueWork(QueryFileNameFromObjectWorker,
                                                  &context,
                                                  true);
    if (!NT_SUCCESS(context.status))
    {
        return context.status;
    }
    return FileName.Append(context.FileName.View());
}


_Use_decl_annotations_
NTSTATUS
KmHelper::File::QueryFileNameFromObject(
    _In_ PVOID FileObject,
    _Inout_ xpf::String<wchar_t>& FileName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    FLT_FILE_NAME_OPTIONS nameOptions = { 0 };
    BOOLEAN queriedUsingOnlyCache = FALSE;

    nameOptions = FLT_FILE_NAME_NORMALIZED;

    /* This can cause deadlocks. So only use cache to lookup name.*/
    if (::IoGetTopLevelIrp() || ::KeAreAllApcsDisabled())
    {
        nameOptions |= FLT_FILE_NAME_QUERY_CACHE_ONLY;
        queriedUsingOnlyCache = TRUE;
    }
    else
    {
        /* We can safely interogate the filesystem as well. */
        nameOptions |= FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP;
        queriedUsingOnlyCache = FALSE;
    }

    status = FltGetFileNameInformationUnsafeWrapper(FileObject,
                                                    nameOptions,
                                                    FileName);
    if (!NT_SUCCESS(status))
    {
        /* Retry by attempting to query the filesystem as well. */
        /* This will also cache the file name, so it will be faster next time. */
        if (queriedUsingOnlyCache)
        {
            status = QueryFileNameFromObjectFallback(FileObject,
                                                     FileName);
        }
    }
    return status;
}


//
// ************************************************************************************************
// *                                SECTION-RELATED FUNCTIONALITY.                                *
// ************************************************************************************************
//


_Use_decl_annotations_
NTSTATUS
KmHelper::File::MapFileInSystem(
    _In_ HANDLE FileHandle,
    _Outptr_result_bytebuffer_(*ViewSize) PVOID* ViewBase,
    _Out_ size_t* ViewSize
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != ViewBase);
    XPF_DEATH_ON_FAILURE(nullptr != ViewSize);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    uint64_t fileSize = 0;

    HANDLE sectionHandle = NULL;
    OBJECT_ATTRIBUTES sectionAttributes = { 0 };
    PVOID sectionObject = NULL;
    LARGE_INTEGER sectionOffset = { 0 };

    /* Preinit output variables. */
    *ViewBase = nullptr;
    *ViewSize = 0;

    /* Get the file size - required to map it into memory. */
    status = KmHelper::File::QueryFileSize(FileHandle,
                                           &fileSize);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* We can map at most max size_t bytes. So check here. */
    if (fileSize > xpf::NumericLimits<size_t>::MaxValue())
    {
        status = STATUS_FILE_TOO_LARGE;
        goto CleanUp;
    }

    /* Now create the section. */
    InitializeObjectAttributes(&sectionAttributes,
                               NULL,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    status = ::ZwCreateSection(&sectionHandle,
                               SECTION_MAP_READ,
                               &sectionAttributes,
                               NULL,
                               PAGE_READONLY,
                               SEC_COMMIT,
                               FileHandle);
    if (!NT_SUCCESS(status))
    {
        sectionHandle = NULL;
        goto CleanUp;
    }

    /* Now that we have our section, we need to map it into memory. */
    /* We want to map it in system space so MmMapViewInSystemSpace. */
    /* So we need to get the section object referenced by this handle. */
    status = ::ObReferenceObjectByHandle(sectionHandle,
                                         SECTION_MAP_READ,
                                         *MmSectionObjectType,
                                         KernelMode,
                                         &sectionObject,
                                         NULL);
    if (!NT_SUCCESS(status))
    {
        sectionObject = NULL;
        goto CleanUp;
    }

    status = ::MmMapViewInSystemSpace(sectionObject,
                                      ViewBase,
                                      reinterpret_cast<PSIZE_T>(ViewSize));
    if (!NT_SUCCESS(status))
    {
        *ViewBase = nullptr;
        *ViewSize = 0;
        goto CleanUp;
    }

    /* The view size may exceeds the file size (due to rounding) */
    if (*ViewSize > fileSize)
    {
        *ViewSize = static_cast<size_t>(fileSize);
    }
    status = STATUS_SUCCESS;

CleanUp:
    if (NULL != sectionObject)
    {
        ObDereferenceObject(sectionObject);
        sectionObject = NULL;
    }
    if (NULL != sectionHandle)
    {
        NTSTATUS closeStatus = ::ZwClose(sectionHandle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(closeStatus));

        sectionHandle = NULL;
    }
    return status;
}

_Use_decl_annotations_
VOID
KmHelper::File::UnMapFileInSystem(
    _Inout_ PVOID* ViewBase
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (NULL == ViewBase)
    {
        return;
    }
    if (NULL != *ViewBase)
    {
        status = ::MmUnmapViewInSystemSpace(*ViewBase);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));

        *ViewBase = NULL;
    }
}

//
// ************************************************************************************************
// *                                HASH-RELATED FUNCTIONALITY.                                   *
// ************************************************************************************************
//

_Use_decl_annotations_
NTSTATUS
KmHelper::File::HashFile(
    _In_ HANDLE FileHandle,
    _In_ _Const_ const HashType& HashType,
    _Inout_ xpf::Buffer<>& Hash
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    BCRYPT_ALG_HANDLE algorithmHandle = { 0 };
    BOOLEAN isAlgorithmProviderOpened = FALSE;

    BCRYPT_HASH_HANDLE hashHandle = { 0 };
    BOOLEAN isHashHandleCreated = FALSE;

    LPCWSTR algorithmId = NULL;
    uint8_t data[32] = { 0 };

    uint32_t hashLength = 0;
    ULONG cbResultPropertyLength = 0;

    PVOID viewBase = NULL;
    size_t viewSize = 0;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

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

    /* Now we map the file. */
    status = KmHelper::File::MapFileInSystem(FileHandle,
                                             &viewBase,
                                             &viewSize);
    if (!NT_SUCCESS(status))
    {
        viewBase = NULL;
        viewSize = 0;
        goto CleanUp;
    }

    /* And now start hashing. */
    for (size_t i = 0; i < viewSize; )
    {
        /* Read in chunks of arraysize(data) bytes. */
        const size_t sizeToRead = (viewSize - sizeof(data) > i) ? sizeof(data)
                                                                : viewSize - i;
        if (sizeToRead >= xpf::NumericLimits<uint32_t>::MaxValue())
        {
            status = STATUS_INVALID_BUFFER_SIZE;
            goto CleanUp;
        }

        /* Read-writes from views can cause seh errors. */
        status = KmHelper::HelperSafeWriteBuffer(data,
                                                 xpf::AlgoAddToPointer(viewBase, i),
                                                 sizeToRead);
        if (!NT_SUCCESS(status))
        {
            goto CleanUp;
        }

        /* Update the hash. */
        status = ::BCryptHashData(hashHandle,
                                  data,
                                  static_cast<uint32_t>(sizeToRead),
                                  0);
        if (!NT_SUCCESS(status))
        {
            goto CleanUp;
        }

        /* Move to the next chunk. */
        i += sizeToRead;
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
    /* Unmap the view. */
    KmHelper::File::UnMapFileInSystem(&viewBase);

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
