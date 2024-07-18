/**
 * @file        ALPC-Tools/AlpcMon_Sys/FileObject.cpp
 *
 * @brief       Source file which contains a wrapper file object.
 *              Can be used safely to interact with a file.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "KmHelper.hpp"

#include "FileObject.hpp"
#include "trace.hpp"

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


/**
 * @brief   We are using this structure as an async context when resolving the file name
 *          from a worker thread.
 */
struct SysMonFileObjectNameContext
{
    /**
     * @brief   The File object whose name we want to query.
     */
    /* [in]  */    PVOID FileObject = nullptr;

    /**
     * @brief   The options to be used when using FltGetFileNameInformationUnsafe. 
     */
    /* [in]  */    FLT_FILE_NAME_OPTIONS NameOptions = 0;

    /**
     * @brief   The resulted file name.
     */
    /* [out] */    xpf::String<wchar_t>* FileName = nullptr;

    /**
     * @breif   The status of the operation.
     */
    /* [out] */    NTSTATUS Status = STATUS_UNSUCCESSFUL;
};

/**
 * @brief   All code here goes in paged section.
 *          Can not do I/O at dispatch level anyway.
 */
XPF_SECTION_PAGED;

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS
SysmonFileOpen(
    _In_ _Const_ const xpf::StringView<wchar_t>& FilePath,
    _In_ _Const_ const uint32_t& DesiredAccess,
    _Out_ HANDLE* FileHandle
) noexcept(true)
{
    /* Can not do I/O at higher IRQLs */
    XPF_MAX_PASSIVE_LEVEL();

    UNICODE_STRING filePath = { 0 };
    IO_STATUS_BLOCK fileIoStatusBlock = { 0 };
    OBJECT_ATTRIBUTES fileAttributes = { 0 };

    ACCESS_MASK desiredAccess = { 0 };
    DWORD shareAccess = 0;
    DWORD createDisposition = 0;

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Preinit output. */
    *FileHandle = NULL;

    /* Map the desired access. */
    if (DesiredAccess & XPF_FILE_ACCESS_READ)
    {
        desiredAccess = FILE_GENERIC_READ;
        shareAccess = FILE_SHARE_READ;
        createDisposition = FILE_OPEN;
    }
    if (DesiredAccess & XPF_FILE_ACCESS_WRITE)
    {
        desiredAccess = desiredAccess | FILE_GENERIC_WRITE;
        createDisposition = FILE_OPEN_IF;
    }

    /* Get an unicode string to open the file. */
    status = KmHelper::HelperViewToUnicodeString(FilePath,
                                                 filePath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Open the file. */
    InitializeObjectAttributes(&fileAttributes,
                               &filePath,
                               OBJ_KERNEL_HANDLE | OBJ_FORCE_ACCESS_CHECK | OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    status = ::ZwCreateFile(FileHandle,
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
        *FileHandle = NULL;
        return status;
    }
    if (!NT_SUCCESS(fileIoStatusBlock.Status))
    {
        *FileHandle = NULL;
        return fileIoStatusBlock.Status;
    }

    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static void
SysmonFileClose(
    _Inout_ PHANDLE Handle
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    if (NULL == Handle)
    {
        return;
    }
    if (NULL != *Handle)
    {
        const NTSTATUS status = ::ZwClose(*Handle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));

        *Handle = NULL;
    }
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS
SysmonFileQuerySize(
    _In_ HANDLE FileHandle,
    _Out_ uint64_t* FileSize
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    IO_STATUS_BLOCK ioStatusBlock = { 0 };
    FILE_STANDARD_INFORMATION fileStandardInfo = { 0 };
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Preinit output. */
    *FileSize = 0;

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
    if (!NT_SUCCESS(ioStatusBlock.Status))
    {
        return ioStatusBlock.Status;
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

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS
SysmonFileRead(
    _In_ HANDLE FileHandle,
    _In_ const uint64_t& Offset,
    _Inout_ size_t* NumberOfBytes,
    _Out_ void* Bytes
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    IO_STATUS_BLOCK ioStatusBlock = { 0 };
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    LARGE_INTEGER offset = { 0 };

    /* We are limited by the API. So we cap here. */
    if (*NumberOfBytes > xpf::NumericLimits<uint32_t>::MaxValue())
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    /* Set the offset where to begin reading. */
    offset.QuadPart = Offset;

    /* Do the actual read. */
    status = ::ZwReadFile(FileHandle,
                          NULL,
                          NULL,
                          NULL,
                          &ioStatusBlock,
                          Bytes,
                          static_cast<ULONG>(*NumberOfBytes),
                          &offset,
                          NULL);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if (!NT_SUCCESS(ioStatusBlock.Status))
    {
        return ioStatusBlock.Status;
    }

    *NumberOfBytes = static_cast<ULONG>(ioStatusBlock.Information);
    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static NTSTATUS
SysmonFileWrite(
    _In_ HANDLE FileHandle,
    _Inout_ size_t* NumberOfBytes,
    _In_ const void* Bytes
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    IO_STATUS_BLOCK ioStatusBlock = { 0 };
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* We are limited by the API. So we cap here. */
    if (*NumberOfBytes > xpf::NumericLimits<uint32_t>::MaxValue())
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    /* ZwWriteFile is poorly annotatted as buffer is non-const. */
    /* However, the buffer is not modified, so const cast is ok. */
    status = ::ZwWriteFile(FileHandle,
                           NULL,
                           NULL,
                           NULL,
                           &ioStatusBlock,
                           const_cast<void*>(Bytes),
                           static_cast<ULONG>(*NumberOfBytes),
                           NULL,
                           NULL);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if (!NT_SUCCESS(ioStatusBlock.Status))
    {
        return ioStatusBlock.Status;
    }

    *NumberOfBytes = static_cast<ULONG>(ioStatusBlock.Information);
    return STATUS_SUCCESS;
}


static void XPF_API
SysmonQueryFileNameFromObject(
    _In_opt_ xpf::thread::CallbackArgument Argument
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    PFLT_FILE_NAME_INFORMATION fileNameInfo = nullptr;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::StringView<wchar_t> retrievedName;

    /* Don't expect this to be null. */
    SysMonFileObjectNameContext* data = static_cast<SysMonFileObjectNameContext*>(Argument);
    if (nullptr == data)
    {
        XPF_ASSERT(false);
        return;
    }

    /* This can only be called with a file object type. */
    if ((nullptr == data->FileObject) || (*IoFileObjectType != ::ObGetObjectType(data->FileObject)))
    {
        status = STATUS_OBJECT_TYPE_MISMATCH;
        goto CleanUp;
    }

    /* Query the name. */
    status = ::FltGetFileNameInformationUnsafe(static_cast<PFILE_OBJECT>(data->FileObject),
                                               nullptr,
                                               data->NameOptions,
                                               &fileNameInfo);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Get a view over the retrieved name. */
    status = KmHelper::HelperUnicodeStringToView(fileNameInfo->Name,
                                                 retrievedName);
    if (NT_SUCCESS(status))
    {
        /* Append it to the name. */
        data->FileName->Reset();
        status = data->FileName->Append(retrievedName);
    }

    /* Now release the information. */
    ::FltReleaseFileNameInformation(fileNameInfo);

CleanUp:
    /* Save the status. */
    data->Status = status;
}


_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::File::FileObject::Create(
    _In_ _Const_ const xpf::StringView<wchar_t>& FilePath,
    _In_ _Const_ const uint32_t& DesiredAccess,
    _Out_ xpf::Optional<SysMon::File::FileObject>* FileObject
) noexcept(true)
{
    /* Can not do I/O at higher IRQLs */
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != FileObject);

    HANDLE fileHandle = NULL;
    uint64_t fileSize = 0;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Open the file. */
    status = SysmonFileOpen(FilePath,
                            DesiredAccess,
                            &fileHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Query the size. */
    status = SysmonFileQuerySize(fileHandle,
                                 &fileSize);
    if (!NT_SUCCESS(status))
    {
        SysmonFileClose(&fileHandle);
        return status;
    }

    /* Proper construct the object. */
    FileObject->Emplace();
    SysMon::File::FileObject& file = (*(*FileObject));

    /* Take ownership ovef handle and size. */
    file.m_FileHandle = fileHandle;
    file.m_FileSize = fileSize;

    fileHandle = NULL;
    fileSize = 0;

    return status;
}

SysMon::File::FileObject::~FileObject(
    void
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    SysmonFileClose(&this->m_FileHandle);

    this->m_FileHandle = NULL;
    this->m_FileSize = 0;
}

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::File::FileObject::Read(
    _In_ const uint64_t& Offset,
    _Inout_ xpf::Buffer* Buffer
) noexcept(true)
{
    /* Can not do I/O at higher IRQLs */
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != Buffer);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    size_t bytes = Buffer->GetSize();

    /* Read the file. */
    status = SysmonFileRead(this->m_FileHandle,
                            Offset,
                            &bytes,
                            Buffer->GetBuffer());
    if (!NT_SUCCESS(status))
    {
        Buffer->Clear();
        return status;
    }

    /* If we read fewer bytes, we resize. */
    if (bytes != Buffer->GetSize())
    {
        status = Buffer->Resize(bytes);
    }
    return status;
}

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::File::FileObject::Write(
    _In_ const void* Buffer,
    _In_ const size_t& BufferSize
) noexcept(true)
{
    /* Can not do I/O at higher IRQLs */
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    size_t bytes = BufferSize;

    /* Write the file. */
    status = SysmonFileWrite(this->m_FileHandle,
                             &bytes,
                             Buffer);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* If we wrote fewer bytes, we signal this mismatch. */
    if (bytes != BufferSize)
    {
        status = STATUS_INVALID_BUFFER_SIZE;
    }
    return status;
}

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::File::QueryFileNameFromRawFileObject(
    _In_ PVOID FileObject,
    _Out_ xpf::String<wchar_t>* FileName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    FLT_FILE_NAME_OPTIONS nameOptions = { 0 };
    BOOLEAN queriedUsingOnlyCache = FALSE;
    SysMonFileObjectNameContext fileObjectNameContext;

    /* Preinit output. */
    FileName->Reset();

    /* Always ask for normalized file name. */
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

    /* Prepare the context. */
    fileObjectNameContext.FileObject = FileObject;
    fileObjectNameContext.NameOptions = nameOptions;
    fileObjectNameContext.FileName = FileName;

    /* Initial query. */
    SysmonQueryFileNameFromObject(&fileObjectNameContext);

    /* If we succeeded or we asked the filesystem as well, we are done. */
    if (NT_SUCCESS(fileObjectNameContext.Status) || (!queriedUsingOnlyCache))
    {
        return fileObjectNameContext.Status;
    }

    /* Use an async thread on fail and query filesystem as well. */
    xpf::thread::Thread asyncThread;
    fileObjectNameContext.NameOptions = FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP;

    status = asyncThread.Run(SysmonQueryFileNameFromObject,
                             &fileObjectNameContext);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Wait for operation. */
    asyncThread.Join();

    /* Propagate the status. */
    return fileObjectNameContext.Status;
}
