/**
 * @file        ALPC-Tools/AlpcMon_Sys/FirmwareTableHandlerFilter.hpp
 *
 * @brief       In this file we define the functionality related to
 *              registering a firmware table handler callback
 *              to filter requests, especially useful for our usermode
 *              hooking component. As we cannot create any ports, or open
 *              any device handlers, because the process may go into
 *              sleep, be suspended, and so on, we are using another
 *              functionality provided by the OS.
 *
 * @note        We use SystemRegisterFirmwareTableInformationHandler
 *              to register our own callback which can be used afterwards
 *              in NtQuerySystemInformation() calls. While this is not
 *              documented by M$, there are numerous open source examples 
 *              on how to use this functionality, you just have to look them up.
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
 * @brief       Registers the FirmwareTableHandler 
 *
 * @param[in]   DriverObject - The driver object to be associated with the
 *              callback for FirmwareTableHandlerFilter.
 * 
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
FirmwareTableHandlerFilterStart(
    _In_ void* DriverObject
) noexcept(true);

/**
 * @brief       Unregisters the previously registered image notify routine.
 *
 * @param[in]   DriverObject - The driver object to be associated with the
 *              callback for FirmwareTableHandlerFilter.
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
FirmwareTableHandlerFilterStop(
    _In_ void* DriverObject
) noexcept(true);
