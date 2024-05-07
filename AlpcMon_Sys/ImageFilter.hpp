/**
 * @file        ALPC-Tools/AlpcMon_Sys/ImageFilter.hpp
 *
 * @brief       In this file we define the functionality related to
 *              image filtering.
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
 * @brief       Registers the load image notify routine.
 *              See the below documentation for details.
 *              https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetloadimagenotifyroutineex
 *
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ImageFilterStart(
    void
) noexcept(true);

/**
 * @brief       Unregisters the previously registered image notify routine.
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ImageFilterStop(
    void
) noexcept(true);
