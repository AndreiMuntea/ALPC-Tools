/**
 * @file        ALPC-Tools/AlpcMon_Sys/ProcessFilter.hpp
 *
 * @brief       In this file we define the functionality related to
 *              process filtering.
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

/**
 * @brief       Registers the create process notify routine.
 *              See the below documentation for details.
 *              https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutine
 *
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ProcessFilterStart(
    void
) noexcept(true);

/**
 * @brief       Unregisters the previously registered create process notify routine.
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessFilterStop(
    void
) noexcept(true);

/**
 * @brief   This routine is used to gather information about the already running processes.
 *          Caller should ensure he blocks the creation of new ones until this is finished.
 *          This must be called after registering the notification.
 *
 * @return  VOID.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessFilterGatherPreexistingProcesses(
    void
) noexcept(true);
