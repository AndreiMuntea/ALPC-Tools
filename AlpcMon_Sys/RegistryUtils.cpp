/**
 * @file        ALPC-Tools/AlpcMon_Sys/RegistryUtils.cpp
 *
 * @brief       In this file we define helper methods specific to registry
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

#include "KmHelper.hpp"

#include "RegistryUtils.hpp"
#include "trace.hpp"



///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       PAGED SECTION AREA                                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   Put everything below here in paged section.
 */
XPF_SECTION_PAGED;


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       WrapperRegistryQueryValueKey.                                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS
KmHelper::WrapperRegistryQueryValueKey(
    _In_ _Const_ const xpf::StringView<wchar_t>& KeyName,
    _In_ _Const_ const xpf::StringView<wchar_t>& ValueName,
    _In_ uint32_t Type,
    _Out_ xpf::Buffer<xpf::SplitAllocatorCritical>* OutBuffer
) noexcept(true)
{
    //
    // Registry related Zw* API requires max passive level.
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwopenkey
    //
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != OutBuffer);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    OBJECT_ATTRIBUTES objAttributes = { 0 };

    UNICODE_STRING regKeyName = { 0 };
    UNICODE_STRING regKeyValueName = { 0 };
    HANDLE regKeyHandle = NULL;

    ULONG retBufferLength = 0;
    PKEY_VALUE_FULL_INFORMATION buffer = nullptr;

    //
    // Open the key.
    //
    status = KmHelper::HelperViewToUnicodeString(KeyName,
                                                 regKeyName);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperViewToUnicodeString keyname %S failed with %!STATUS!",
                       KeyName.Buffer(),
                       status);
        goto CleanUp;
    }

    InitializeObjectAttributes(&objAttributes, &regKeyName, OBJ_KERNEL_HANDLE, NULL, NULL);
    status = ::ZwOpenKey(&regKeyHandle,
                         KEY_READ,
                         &objAttributes);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("ZwOpenKey %S failed with %!STATUS!",
                       KeyName.Buffer(),
                       status);
        regKeyHandle = NULL;
        goto CleanUp;
    }

    //
    // Now query the value for the size.
    //
    status = KmHelper::HelperViewToUnicodeString(ValueName,
                                                 regKeyValueName);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperViewToUnicodeString valuename %S failed with %!STATUS!",
                       ValueName.Buffer(),
                       status);
        goto CleanUp;
    }
    status = ::ZwQueryValueKey(regKeyHandle,
                               &regKeyValueName,
                               KEY_VALUE_INFORMATION_CLASS::KeyValueFullInformation,
                               NULL,
                               0,
                               &retBufferLength);
    if ((status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL) || (retBufferLength == 0))
    {
        SysMonLogError("ZwQueryValueKey %S - %S failed with %!STATUS! and retLength = %d",
                       KeyName.Buffer(),
                       ValueName.Buffer(),
                       status,
                       retBufferLength);

        status = STATUS_REGISTRY_CORRUPT;
        goto CleanUp;
    }

    //
    // Query again for the actual data.
    //
    buffer = static_cast<PKEY_VALUE_FULL_INFORMATION>(xpf::MemoryAllocator::AllocateMemory(retBufferLength));
    if (nullptr == buffer)
    {
        SysMonLogError("Insufficient resources to allocate for KEY_VALUE_FULL_INFORMATION. Required %d for %S - %S",
                        retBufferLength,
                        KeyName.Buffer(),
                        ValueName.Buffer());

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CleanUp;
    }

    status = ::ZwQueryValueKey(regKeyHandle,
                               &regKeyValueName,
                               KEY_VALUE_INFORMATION_CLASS::KeyValueFullInformation,
                               buffer,
                               retBufferLength,
                               &retBufferLength);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("ZwQueryValueKey %S - %S failed with %!STATUS!",
                       KeyName.Buffer(),
                       ValueName.Buffer(),
                       status);
        goto CleanUp;
    }

    //
    // Check that we got what we expected.
    //
    if (Type != buffer->Type)
    {
        SysMonLogError("ZwQueryValueKey %S - %S found type mismatch. Expected %d Actual %d",
                       KeyName.Buffer(),
                       ValueName.Buffer(),
                       Type,
                       buffer->Type);

        status = STATUS_NOT_FOUND;
        goto CleanUp;
    }

    //
    // Now save the structure.
    //
    status = OutBuffer->Resize(buffer->DataLength);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Resize for %S - %S failed with %!STATUS!. Required %d",
                       KeyName.Buffer(),
                       ValueName.Buffer(),
                       buffer->DataLength,
                       status);
        goto CleanUp;
    }
    status = KmHelper::HelperSafeWriteBuffer(OutBuffer->GetBuffer(),
                                             xpf::AlgoAddToPointer(buffer, buffer->DataOffset),
                                             buffer->DataLength);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperSafeWriteBuffer %S - %S failed with %!STATUS!",
                       KeyName.Buffer(),
                       ValueName.Buffer(),
                       status);
        goto CleanUp;
    }

    //
    // All good.
    //
    SysMonLogTrace("Retrieved key %S : value %S - data size %I64d type %d",
                   KeyName.Buffer(),
                   ValueName.Buffer(),
                   uint64_t{OutBuffer->GetSize()},
                   Type);
    status = STATUS_SUCCESS;

CleanUp:
    if (nullptr != buffer)
    {
        xpf::MemoryAllocator::FreeMemory(buffer);
        buffer = nullptr;
    }
    if (NULL != regKeyHandle)
    {
        NTSTATUS closeStatus = ::ZwClose(regKeyHandle);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(closeStatus));

        regKeyHandle = NULL;
    }
    return status;
}
