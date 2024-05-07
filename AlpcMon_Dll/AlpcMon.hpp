/**
 * @file        ALPC-Tools/ALPCMon_Dll/AlpcMon.hpp
 *
 * @brief       This is responsible for monitoring alpc logic.
 *              Currently we have a predefined list of ports.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#pragma once

#include "HookEngine.hpp"

/**
 * @brief We monitor NtAlpcConnectPort.
 */
extern HookEngineApi gNtAlpcConnectPortHook;

/**
 * @brief We monitor NtAlpcSendWaitReceivePort.
 */
extern HookEngineApi gNtAlpcSendWaitReceivePortHook;

/**
 * @brief We monitor NtAlpcDisconnectPort.
 */
extern HookEngineApi gNtAlpcDisconnectPortHook;
