/**
 * @file        ALPC-Tools/AlpcMon_Sys/RegistryUtils.hpp
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

#pragma once

#include "precomp.hpp"

namespace KmHelper
{
/**
 * @brief       Wrapper for querying a value for registry.
 *              Opens the registry key, then queries the value.
 *
 * @param[in]   KeyName     - The name of the registry key to be opened.
 *
 * @param[in]   ValueName   - The name of the value we want to query.
 *
 * @param[in]  Type         - Type of the value the buffer points to.
 *                            See the note regarding potential values.
 *
 * @param[out]  OutBuffer   - Contains the value stored in registry for
 *                            key with the given name and value with given name.
 *
 * @return      A proper NTSTATUS error code.
 *
 * @note        To find the potential values that 'Type' can have
 *              https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_key_value_basic_information
 *              Inspect the aformentioned MSDN link.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
WrapperRegistryQueryValueKey(
    _In_ _Const_ const xpf::StringView<wchar_t>& KeyName,
    _In_ _Const_ const xpf::StringView<wchar_t>& ValueName,
    _In_ uint32_t Type,
    _Out_ xpf::Buffer<xpf::SplitAllocatorCritical>* OutBuffer
) noexcept(true);
};  // namespace KmHelper
