/**
 * @file        ALPC-Tools/AlpcMon_Sys/StackDecorator.hpp
 *
 * @brief       In this file we define helper api to aid with stack capture
 *              and decoration.
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

namespace SysMon
{
/**
 * @brief    An object containing the stack trace. 
 */
struct StackTrace
{
    /**
     * @brief   Addresses of the stack trace.
     */
    void*       Frames[128] = { 0 };
    /**
     * @brief   Actual number of captured frames in the Frames array.
     */
    uint32_t    CapturedFrames = 0;
    /**
     * @brief   The ID of the process in which the capture took place.
     */
    uint32_t    ProcessPid = 0;
    /**
     * @brief   The decorated frames containing module!symbol information. 
     */
    xpf::Vector<xpf::String<wchar_t>> DecoratedFrames;
};  // struct StackTrace

/**
 * @brief           Captures the current thread stack trace.
 *
 * @param[in,out]   Trace - will only capture the raw stack.
 *
 * @return          A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS XPF_API
StackTraceCapture(
    _Inout_ StackTrace* Trace
) noexcept(true);

/**
 * @brief           Decorates the current stack trace.
 *
 * @param[in,out]   Trace - a previosuly captured stack trace.
 *                  This API will populate the DecoratedFrames member.
 *
 * @return          A proper NTSTATUS error code.
 */
_Must_inspect_result_
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS XPF_API
StackTraceDecorate(
    _Inout_ StackTrace* Trace
) noexcept(true);
};  // namespace SysMon
