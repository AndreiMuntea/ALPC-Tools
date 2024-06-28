/**
 * @file        ALPC-Tools/AlpcMon_Sys/ProcessCollector.hpp
 *
 * @brief       A structure containing data about processes.
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

/**
 * @brief       Creates the process collector.
 *
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver entry.
 *
 * @note        Must be called before registering the process filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ProcessCollectorCreate(
    void
) noexcept(true);

/**
 * @brief       Destroys the previously created process collector
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver unload.
 *
 * @note        Must be called after unregistering process filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorDestroy(
    void
) noexcept(true);

/**
 * @brief       This API handles the creation of a new process.
 *
 * @param[in]   ProcessId   - the id of the process which is created.
 * @param[in]   ProcessPath - the path of the process
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorHandleCreateProcess(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ProcessPath
) noexcept(true);

/**
 * @brief       This API handles the termination of an existing process.
 *
 * @param[in]   ProcessId - the id of the process which is terminated.
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorHandleTerminateProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true);

/**
 * @brief       This API handles the creation of a new module
 *              associated with a given process.
 *
 * @param[in]   ProcessId   - the id of the process in which the module is loaded.
 * @param[in]   ModulePath  - the path of the new module.
 * @param[in]   ModuleBase  - the base address in the process where the module is loaded.
 * @param[in]   ModuleSize  - the size of the module.
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorHandleLoadModule(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
    _In_ _Const_ const void* ModuleBase,
    _In_ _Const_ const size_t& ModuleSize
) noexcept(true);
