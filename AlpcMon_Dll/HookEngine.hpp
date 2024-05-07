/**
 * @file        ALPC-Tools/ALPCMon_Dll/HookEngine.hpp
 *
 * @brief       This is responsible for installing the hooks.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#pragma once

#include <xpf_lib/xpf.hpp>
#include "UmKmComms.hpp"

/**
 * @brief   This structure holds data for a specific hook.
 *          The OriginalApi is saved by detours and must be called
 *          by the HookApi.
 */
struct HookEngineApi
{
    /**
     * @brief   The name of the dll in which the api resides.
     */
    const xpf::StringView<wchar_t> DllName;
    /**
     * @brief   The name of the api.
     */
    const xpf::StringView<char> ApiName;
    /**
     * @brief   This is saved by detours mechanism.
     *          Pointer to the target pointer to which the detour will be attached.
     */
    void* OriginalApi;
    /**
     * @brief   Pointer to the detour function.
     *          Must call the OriginalApi internally.
     */
    void* HookApi;
};

/**
 * @brief       Responsible for installing the hooks.
 *
 * @return      A proper NTSTATUS error code.
 */
_Must_inspect_result_
NTSTATUS XPF_API
HookEngineInitialize(
    void
) noexcept(true);

/**
 * @brief       Responsible for deinitializing the hooks.
 *
 * @return      A proper NTSTATUS error code.
 */
void XPF_API
HookEngineDeinitialize(
    void
) noexcept(true);

/**
 * @brief           Responsible for notifying the KM with a specific message.
 *
 * @param[in,out]   Message - The actual message buffer to be sent.
 *                            On return, this may be altered with response.
 *
 * @return          A proper NTSTATUS error code.
 */
NTSTATUS XPF_API
HookEngineNotifyKernel(
    _Inout_  UM_KM_MESSAGE_HEADER* Message
) noexcept(true);
