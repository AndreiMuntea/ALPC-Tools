/**
 * @file        ALPC-Tools/AlpcMon_Sys/ModuleCollector.hpp
 *
 * @brief       A structure containing data about modules.
 *              Such as hash values and exports.
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
#include "KmHelper.hpp"
#include "HashUtils.hpp"

/**
 * @brief       Creates the module collector.
 *
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver entry.
 *
 * @note        Must be called before registering the image filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ModuleCollectorCreate(
    void
) noexcept(true);

/**
 * @brief       Destroys the previously created module collector
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver unload.
 *
 * @note        Must be called after unregistering image filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ModuleCollectorDestroy(
    void
) noexcept(true);

/**
 * @brief       This API handles the creation of a new module.
 *              It first looks up in the module collector cache.
 *              If the module is not found, it enqueues an async
 *              work item to compute the details about this module.
 *
 * @param[in]   ModulePath      - the path of the new module.
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID XPF_API
ModuleCollectorHandleNewModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true);
