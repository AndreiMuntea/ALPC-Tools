/**
 * @file        ALPC-Tools/AlpcMon_Sys/KmHelper.hpp
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

#pragma once

#include "precomp.hpp"

namespace KmHelper
{
/**
 * @brief       Retrieves the address of a function exported by ntoskrnl or hal.
 *              It is implemented as a convenient wrapper over MmGetSystemRoutine.
 *
 * @param[in]   SystemRoutineName   - The name of the function.
 *
 * @return      The address of the function, or NULL if the function could not be found.
 *
 * @note        This function can only be called at IRQL equal to PASSIVE_LEVEL.
 */
_Ret_maybenull_
_IRQL_requires_max_(PASSIVE_LEVEL)
void* XPF_API
WrapperMmGetSystemRoutine(
    _In_ _Const_ const xpf::StringView<wchar_t>& SystemRoutineName
) noexcept(true);

/**
 * @brief       Checks whether a process is protected process or
 *              protected process light.
 *
 * @param[in]   EProcess   - The eprocess to check whether protected or not
 *
 * @return      True if the process is protected (or light protected),
 *              false otherwise.
 *
 * @note        This function resolves the routines PsIsProtectedProcess and
 *              PsIsProtectedProcessLight on the first use.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
bool XPF_API
WrapperIsProtectedProcess(
    _In_ void* EProcess
) noexcept(true);

/**
 * @brief       Wrapper over RtlImageNtHeader / RtlImageNtHeaderEx.
 *              On windows 7 only the first variant is exported.
 *              On windows 8 and above, the second one is as well.
 *              We prefer the second one if we find it.
 *
 * @param[in]   Flags       - RTL_IMAGE_NT_HEADER_EX_FLAG_NO_RANGE_CHECK for compatibility with RtlImageNtHeader,
 *                            recommended to be zero.
 * @param[in]   Base        - Supplies the base of the image.
 * @param[in]   Size        - Supplies the size of the image.
 * @param[out]  OutHeaders  - The nt headers.
 *
 * @return      A proper NTSTATUS error code.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
WrapperRtlImageNtHeaderEx(
    _In_ uint32_t Flags,
    _In_ void* Base,
    _In_ uint64_t Size,
    _Out_ PIMAGE_NT_HEADERS* OutHeaders
) noexcept(true);

/**
 * @brief       Uses PsGetProcessWow64Process. Checks whether the process has an associated wow64
 *              eprocess and returns true or false based on that.
 *
 * @param[in]   EProcess   - Process to be checked.
 *
 * @return      true if the process is wow64, false otherwise. 
 *
 * @note        This function can only be called at IRQL equal to PASSIVE_LEVEL.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
bool XPF_API
WrapperIsWow64Process(
    _In_ void* EProcess
) noexcept(true);

/**
 * @brief           Checks whether the address is a user one.
 *                  Uses the mvi tracked MM_USER_PROBE_ADDRESS to check
 *
 * @param[in]       Address             - The address to be checked.
 *
 * @return          true if Address is lower than the MM_USER_PROBE_ADDRESS,
 *                  false otherwise.
 */
_IRQL_requires_max_(APC_LEVEL)
bool XPF_API
HelperIsUserAddress(
    _In_opt_ _Const_ const void* Address
) noexcept(true);

/**
 * @brief           Hashes the unicode string provided
 *
 * @param[in]       String - The string to be hashed.
 * @param[out]      Hash   - The hash of the unicode string
 *
 * @return          A proper ntstatus error code.
 *
 * @note            This uses the RtlHashUnicodeString with the
 *                  HASH_STRING_ALGORITHM_DEFAULT hash type.
 *                  Currently, the default algorithm is the x65599 hashing algorithm.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
HelperHashUnicodeString(
    _In_ _Const_ const xpf::StringView<wchar_t>& String,
    _Out_ uint32_t* Hash
) noexcept(true);

/**
 * @brief           Implements functionality of finding an export
 *                  by walking the export table.
 *
 * @param[in]       ModuleBase          - The base of the module.
 * @param[in]       ModuleSize          - The size of the module.
 * @param[in]       ModuleMappedAsImage - If true, the module is loaded as an image,
 *                                        If false, the module is mapped (as it is from disk).
 * @param[in]       ExportName          - A null terminated string containing the export name.
 *
 * @return          NULL if the export could not be retrieved or was not found,
 *                  the address of the export otherwise.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void* XPF_API
HelperFindExport(
    _In_ void* ModuleBase,
    _In_ size_t ModuleSize,
    _In_ bool ModuleMappedAsImage,
    _In_ _Const_ const char* ExportName
) noexcept(true);

/**
 * @brief           Implements functionality of finding the VA from a RVA.
 *
 * @param[in]       ModuleBase          - The base of the module.
 * @param[in]       ModuleSize          - The size of the module.
 * @param[in]       RVA                 - The RVA.
 * @param[in]       ModuleMappedAsImage - If true, the module is loaded as an image,
 *                                        If false, the module is mapped (as it is from disk).
 *
 * @return          The value of RVA to VA.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void* XPF_API
HelperRvaToVa(
    _In_ void* ModuleBase,
    _In_ size_t ModuleSize,
    _In_ uint32_t RVA,
    _In_ bool ModuleMappedAsImage
) noexcept(true);

/**
 * @brief       Copies the contents of a source memory block to a destination memory block.
 *              It supports overlapping source and destination memory blocks.
 *              It is safe as it does probe for read and guards against violations.
 *
 * @param[out]  Destination - A pointer to the destination memory block to copy the bytes to.
 *
 * @param[in]   Source - A pointer to the source memory block to copy the bytes from.
 *
 * @param[in]   Size - The number of bytes to copy from the source to the destination.
 *
 * @return      A proper NTSTATUS error code.
 */
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS XPF_API
HelperSafeWriteBuffer(
    _Inout_ void* Destination,
    _In_reads_bytes_(Size) const void* Source,
    _In_ size_t Size
) noexcept(true);

/**
 * @brief           Obtains a view over the Unicode String.
 *                  The UnicodeString must remain valid as long as the view is valid.
 *                  No memory copy is performed.
 *
 * @param[in]       UnicodeString       - The String to get a view over.
 * @param[in,out]   UnicodeStringView   - A view over the passed string
 *
 * @return          A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS XPF_API
HelperUnicodeStringToView(
    _In_ _Const_ const UNICODE_STRING& UnicodeString,
    _Inout_ xpf::StringView<wchar_t>& UnicodeStringView
) noexcept(true);

/**
 * @brief           Obtains an unicode string over from the passed String view.
 *                  The UnicodeString must remain valid as long as the view is valid.
 *                  No memory copy is performed.
 *
 * @param[in]       UnicodeStringView   - An unicode string view.
 * @param[in,out]   UnicodeString       - An unicode string created from the view.
 *
 * @return          A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS XPF_API
HelperViewToUnicodeString(
    _In_ _Const_ const xpf::StringView<wchar_t>& UnicodeStringView,
    _Inout_  UNICODE_STRING& UnicodeString
) noexcept(true);

};  // namespace KmHelper

namespace SysMon
{
/**
 * @brief Available architectures for sysmon sensor.
 *        To avoid ifdefs and inconsistencies we group them here.
 */
enum class OsArchitecture
{
    /**
     * @brief   Corresponds to _M_IX86
     */
    ix86 = 0,

    /**
     * @brief   Corresponds to _M_AMD64
     */
    amd64 = 1,

    /**
     * @brief A canary value so we can find how many
     *        architectures we have defined so far.
     *
     * @note  Do not add anything below this.
     */
    MAX
};  // enum class OsArchitecture

/**
 * @brief   Returns the current architecture of the OS.
 *          It is intended to avoid ifdefs in many places.
 *          Better to have them here and be consistent.
 *
 * @return  Architecture.
 */
inline constexpr
SysMon::OsArchitecture
CurrentOsArchitecture(
    void
) noexcept(true)
{
    #if defined _M_IX86
        return SysMon::OsArchitecture::ix86;
    #elif defined _M_AMD64
        return SysMon::OsArchitecture::amd64;
    #else
        #error Unsupported Architecture
    #endif  // Architecture
}
};  // namespace SysMon
