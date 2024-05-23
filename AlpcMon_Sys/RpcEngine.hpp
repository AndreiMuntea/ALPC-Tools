/**
 * @file        ALPC-Tools/AlpcMon_Sys/RpcEngine.hpp
 *
 * @brief       The inspection engine capable of understanding
 *              RPC messages as they are serialized.
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
namespace RpcEngine
{
/**
 * @brief       This function is responsible for inspecting RPC messages from a buffer.
 *              It takes the required actions on each message. This usually is a log or
 *              A new event.
 *
 * @param[in]   Buffer          - Binary buffer containing the serialized rpc message.
 *                                The buffer needs to be captured - it is the caller responsibility
 *                                to ensure that access will Not trigger any SEH exceptions.
 * @param[in]   BufferSize      - Number of bytes in the Buffer.
 * @param[in]   Interface       - The interface in which the call happens.
 * @param[in]   ProcedureNumber - The procedure that is called from the given interface.
 * @param[in]   TransferSyntax  - Transfer syntax used - one of the LRPC_TRANSFER_SYNTAX* flags.
 * @param[in]   PortHandle      - Associated with the message.
 *
 * @return      This function does not return anything. It handles all cases internally and takes
 *              any required actions.
 *
 * @note        This method is not intended to be called at dispatch level.
 */
_IRQL_requires_max_(APC_LEVEL)
void XPF_API
Analyze(
    _In_ _Const_ const uint8_t* Buffer,
    _In_ size_t BufferSize,
    _In_ const uuid_t& Interface,
    _In_ const uint64_t ProcedureNumber,
    _In_ const uint64_t& TransferSyntax,
    _In_ const uint64_t& PortHandle
) noexcept(true);
};  // namespace RpcEngine
};  // namespace SysMon
