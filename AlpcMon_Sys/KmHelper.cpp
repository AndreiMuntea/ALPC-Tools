/**
 * @file        ALPC-Tools/AlpcMon_Sys/KmHelper.cpp
 *
 * @brief       In this file we define helper methods so we can use them
 *              throughout the project.
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
// |                                       Wrapper for MmGetSystemRoutineAddress                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Ret_maybenull_
void* XPF_API
KmHelper::WrapperMmGetSystemRoutine(
    _In_ _Const_ const wchar_t* SystemRoutineName
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-mmgetsystemroutineaddress
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UNICODE_STRING routineName = { 0 };

    //
    // First we transform the view into UNICODE_STRING format.
    //
    status = ::RtlInitUnicodeStringEx(&routineName,
                                      SystemRoutineName);
    if (!NT_SUCCESS(status))
    {
        return nullptr;
    }

    //
    // Now get the method.
    //
    return ::MmGetSystemRoutineAddress(&routineName);
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |              Wrapper for PsIsProtectedProcess and PsIsProtectedProcessLight                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
bool XPF_API
KmHelper::WrapperIsProtectedProcess(
    _In_ void* EProcess
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != EProcess);

    /* Assume unprotected. */
    bool isProtected = false;

    PFUNC_PsIsProtectedProcess gPsIsProtectedProcess = GlobalDataGetDynamicData()->ApiPsIsProtectedProcess;
    PFUNC_PsIsProtectedProcess gPsIsProtectedProcessLight = GlobalDataGetDynamicData()->ApiPsIsProtectedProcessLight;

    __try
    {
        /* First we check if this is protected. */
        if (gPsIsProtectedProcess && (FALSE != gPsIsProtectedProcess(static_cast<PEPROCESS>(EProcess))))
        {
            isProtected = true;
            __leave;
        }

        /* Then we check if this is light protected. */
        if (gPsIsProtectedProcessLight && (FALSE != gPsIsProtectedProcessLight(static_cast<PEPROCESS>(EProcess))))
        {
            isProtected = true;
            __leave;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* On exception we assume unprotected. */
        isProtected = false;
    }

    return isProtected;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |              Wrapper for RtlImageNtHeaderEx                                                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
KmHelper::WrapperRtlImageNtHeaderEx(
    _In_ uint32_t Flags,
    _In_ void* Base,
    _In_ uint64_t Size,
    _Out_ PIMAGE_NT_HEADERS* OutHeaders
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != Base);
    XPF_DEATH_ON_FAILURE(0 != Size);
    XPF_DEATH_ON_FAILURE(nullptr != OutHeaders);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PFUNC_RtlImageNtHeaderEx gRtlImageNtHeaderEx = GlobalDataGetDynamicData()->ApiRtlImageNtHeaderEx;
    PFUNC_RtlImageNtHeader gRtlImageNtHeader = GlobalDataGetDynamicData()->ApiRtlImageNtHeader;

    __try
    {
        if (gRtlImageNtHeaderEx)
        {
            /* Prefer *Ex variant if available. */
            status = gRtlImageNtHeaderEx(Flags,
                                         Base,
                                         Size,
                                         OutHeaders);
        }
        else if (gRtlImageNtHeader)
        {
            /* Fallback to base variant. */
            *OutHeaders = gRtlImageNtHeader(Base);

            /* If we failed to retrieve headers, we fail the operation. */
            status = (NULL != *OutHeaders) ? STATUS_SUCCESS
                                           : STATUS_INVALID_IMAGE_FORMAT;
        }
        else
        {
            /* Should not happen. */
            status = STATUS_ILLEGAL_FUNCTION;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* Failed to parse PE. We accessed garbage. */
        status = STATUS_UNHANDLED_EXCEPTION;
    }

    return status;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |              Wrapper for PsGetProcessWow64Process                                                               |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
bool XPF_API
KmHelper::WrapperIsWow64Process(
    _In_ void* EProcess
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != EProcess);

    if constexpr (SysMon::CurrentOsArchitecture() == SysMon::OsArchitecture::ix86)
    {
        /* On x86 we don't have wow processes. */
        return false;
    }
    else if constexpr (SysMon::CurrentOsArchitecture() == SysMon::OsArchitecture::amd64)
    {
        PFUNC_PsGetProcessWow64Process gPsGetProcessWow64Process = GlobalDataGetDynamicData()->ApiPsGetProcessWow64Process;
        return (gPsGetProcessWow64Process && (NULL != gPsGetProcessWow64Process(static_cast<PEPROCESS>(EProcess))));
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to check if an address needs probing                               |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(APC_LEVEL)
bool XPF_API
KmHelper::HelperIsUserAddress(
    _In_opt_ _Const_ const void* Address
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    return xpf::AlgoPointerToValue(Address) <= MM_USER_PROBE_ADDRESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to find the VA                                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
void* XPF_API
KmHelper::HelperRvaToVa(
    _In_ void* ModuleBase,
    _In_ size_t ModuleSize,
    _In_ uint32_t RVA,
    _In_ bool ModuleMappedAsImage
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != ModuleBase);
    XPF_DEATH_ON_FAILURE(0 != ModuleSize);

    void* result = nullptr;

    PIMAGE_SECTION_HEADER ntSection = nullptr;
    PIMAGE_NT_HEADERS ntHeaders = nullptr;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    __try
    {
        /* Go to the nt headers. */
        status = KmHelper::WrapperRtlImageNtHeaderEx(0,
                                                     ModuleBase,
                                                     ModuleSize,
                                                     &ntHeaders);
        if ((!(NT_SUCCESS(status)) || (nullptr == ntHeaders)))
        {
            __leave;
        }

        /* Grab the first section. */
        ntSection = IMAGE_FIRST_SECTION(ntHeaders);
        if (nullptr == ntSection)
        {
            __leave;
        }

        if (!ModuleMappedAsImage)
        {
            for (uint16_t i = 0; ntHeaders->FileHeader.NumberOfSections; ++i)
            {
                if (RVA >= ntSection->VirtualAddress && RVA < ntSection->VirtualAddress + ntSection->SizeOfRawData)
                {
                    /* Found the section. */
                    uint32_t offsetInSection = RVA - ntSection->VirtualAddress;
                    result = static_cast<uint8_t*>(ModuleBase) + ntSection->PointerToRawData + offsetInSection;
                    break;
                }

                /* Try next section. */
                ++ntSection;
            }
        }
        else
        {
            result = static_cast<uint8_t*>(ModuleBase) + RVA;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* If an exception is encountered, we return nullptr. */
        result = nullptr;
    }

    return result;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to find an export                                                  |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//


_IRQL_requires_max_(PASSIVE_LEVEL)
void* XPF_API
KmHelper::HelperFindExport(
    _In_ void* ModuleBase,
    _In_ size_t ModuleSize,
    _In_ bool ModuleMappedAsImage,
    _In_ _Const_ const char* ExportName
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != ModuleBase);
    XPF_DEATH_ON_FAILURE(0 != ModuleSize);
    XPF_DEATH_ON_FAILURE(nullptr != ExportName);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    void* result = nullptr;

    IMAGE_EXPORT_DIRECTORY* imageExportDirectory = 0;
    ULONG imageExportDirectorySize = 0;

    uint16_t* nameOrdinals = nullptr;
    uint32_t* addressOfFunctions = nullptr;
    uint32_t* namesRva = nullptr;

    ANSI_STRING exportName = { 0 };
    ANSI_STRING foundName = { 0 };

    __try
    {
        /* If this is a user address, we need to probe it. */
        if (KmHelper::HelperIsUserAddress(ModuleBase))
        {
            ::ProbeForRead(ModuleBase, ModuleSize, 1);
        }

        /* Get the image export directory. */
        imageExportDirectory = static_cast<IMAGE_EXPORT_DIRECTORY*>(
                               ::RtlImageDirectoryEntryToData(ModuleBase,
                                                              ModuleMappedAsImage ? TRUE : FALSE,
                                                              IMAGE_DIRECTORY_ENTRY_EXPORT,
                                                              &imageExportDirectorySize));
        if ((NULL == imageExportDirectory) || (0 == imageExportDirectorySize))
        {
            __leave;
        }

        /* Now grab the details of export data. */
        nameOrdinals = static_cast<uint16_t*>(KmHelper::HelperRvaToVa(ModuleBase,
                                                                      ModuleSize,
                                                                      imageExportDirectory->AddressOfNameOrdinals,
                                                                      ModuleMappedAsImage));
        addressOfFunctions = static_cast<uint32_t*>(KmHelper::HelperRvaToVa(ModuleBase,
                                                                            ModuleSize,
                                                                            imageExportDirectory->AddressOfFunctions,
                                                                            ModuleMappedAsImage));
        namesRva = static_cast<uint32_t*>(KmHelper::HelperRvaToVa(ModuleBase,
                                                                  ModuleSize,
                                                                  imageExportDirectory->AddressOfNames,
                                                                  ModuleMappedAsImage));
        if (nullptr == nameOrdinals || nullptr == addressOfFunctions || nullptr == namesRva)
        {
            __leave;
        }

        /* Prepare to find the name - get an ansi string. */
        status = ::RtlInitAnsiStringEx(&exportName, ExportName);
        if (!NT_SUCCESS(status))
        {
            __leave;
        }

        /* Now walk the names. */
        for (uint32_t i = 0; i < imageExportDirectory->NumberOfNames; ++i)
        {
            uint16_t nameOrdinal = 0;
            uint32_t exportRva = 0;
            const char* name = static_cast<const char*>(KmHelper::HelperRvaToVa(ModuleBase,
                                                                                ModuleSize,
                                                                                namesRva[i],
                                                                                ModuleMappedAsImage));
            if (nullptr == name)
            {
                /* Module is malformed, stop here. */
                break;
            }

            /* Grab an ansi string - so we can compare. */
            status = ::RtlInitAnsiStringEx(&foundName, name);
            if (!NT_SUCCESS(status))
            {
                break;
            }

            /* Compare the found name with the searched one. */
            if (::RtlEqualString(&foundName, &exportName, TRUE) == FALSE)
            {
                continue;
            }

            /* Found our export. */
            nameOrdinal = nameOrdinals[i];
            exportRva = addressOfFunctions[nameOrdinal];

            result = KmHelper::HelperRvaToVa(ModuleBase,
                                             ModuleSize,
                                             exportRva,
                                             ModuleMappedAsImage);

            /* Touch it to ensure it is ok. */
            XPF_VERIFY((*static_cast<volatile uint8_t*>(result)) == (*static_cast<volatile uint8_t*>(result)));
            break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* If an exception is encountered, we return nullptr. */
        result = nullptr;
    }

    return result;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to hash an unicode string.                                         |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
KmHelper::HelperHashUnicodeString(
    _In_ _Const_ const xpf::StringView<wchar_t>& String,
    _Out_ uint32_t* Hash
) noexcept(true)
{
    /* RtlHashUnicodeString can be called at max passive level see annotation. */
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != Hash);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UNICODE_STRING ustr = { 0 };

    /* Preinit output. */
    *Hash = 0;

    /* Get the view in ustr format. */
    status = KmHelper::HelperViewToUnicodeString(String,
                                                 ustr);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Do hashing. */
    status = ::RtlHashUnicodeString(&ustr,
                                    TRUE,
                                    HASH_STRING_ALGORITHM_DEFAULT,
                                    reinterpret_cast<PULONG>(Hash));
    if (!NT_SUCCESS(status))
    {
        *Hash = 0;
        return status;
    }

    /* All good. */
    return STATUS_SUCCESS;
}


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to write a buffer safe                                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS XPF_API
KmHelper::HelperSafeWriteBuffer(
    _Inout_ void* Destination,
    _In_reads_bytes_(Size) const void* Source,
    _In_ size_t Size
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != Destination);
    XPF_DEATH_ON_FAILURE(nullptr != Source);
    XPF_DEATH_ON_FAILURE(0 != Size);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    __try
    {
        /* If this is a user address, we need to probe it. */
        if (KmHelper::HelperIsUserAddress(Source))
        {
            auto src = xpf::AlgoPointerToValue(Source);
            ::ProbeForRead(reinterpret_cast<volatile void*>(src), Size, 1);
        }
        /* If this is a user address, we need to probe it. */
        if (KmHelper::HelperIsUserAddress(Destination))
        {
            auto dst = xpf::AlgoPointerToValue(Destination);
            ::ProbeForWrite(reinterpret_cast<volatile void*>(dst), Size, 1);
        }

        /* The actual copy. */
        xpf::ApiCopyMemory(Destination,
                           Source,
                           Size);

        /* If we are here, all good. */
        status = STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        status = STATUS_UNHANDLED_EXCEPTION;
    }

    return status;
}

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       NON-PAGED SECTION AREA                                                    |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   Put everything below here in default section.
 */
XPF_SECTION_DEFAULT;


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to convert an UNICODE_STRING to a view                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS XPF_API
KmHelper::HelperUnicodeStringToView(
    _In_ _Const_ const UNICODE_STRING& UnicodeString,
    _Inout_ xpf::StringView<wchar_t>& UnicodeStringView
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // We do not allow invalid strings to be converted.
    //
    status = ::RtlValidateUnicodeString(0,
                                        &UnicodeString);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Get a view from the string.
    //
    UnicodeStringView = xpf::StringView<wchar_t>(UnicodeString.Buffer,
                                                 UnicodeString.Length / sizeof(wchar_t));
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Helper to convert a view to an UNICODE_STRING                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS XPF_API
KmHelper::HelperViewToUnicodeString(
    _In_ _Const_ const xpf::StringView<wchar_t>& UnicodeStringView,
    _Inout_  UNICODE_STRING& UnicodeString
) noexcept(true)
{
    XPF_MAX_DISPATCH_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // An UNICODE_STRING can hold at most uint16_t characters.
    // If we can not safely represent all, we bail.
    //
    if (UnicodeStringView.BufferSize() > xpf::NumericLimits<uint16_t>::MaxValue() / sizeof(wchar_t))
    {
        return STATUS_INVALID_PARAMETER;
    }

    status = ::RtlInitUnicodeStringEx(&UnicodeString,
                                      UnicodeStringView.Buffer());
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    return ::RtlValidateUnicodeString(0,
                                      &UnicodeString);
}
