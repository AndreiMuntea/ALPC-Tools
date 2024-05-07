/**
 * @file        ALPC-Tools/ALPCMon_Dll/UmKmComms.hpp
 *
 * @brief       In this file we have the common messages between um-km.
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

/**
 * @brief   Used when creating the callback table.
 *          Must be the same when we query.
 */
#define UM_KM_CALLBACK_SIGNATURE                   '#SMN'

/**
 * @brief   Coresponds to SYSTEM_FIRMWARE_TABLE_ACTION::SystemFirmwareTable_Get
 *          We only handle get requests.
 */
#define UM_KM_REQUEST_TYPE                         1


/**
 * @brief   This contains data from SYSTEM_FIRMWARE_TABLE_INFORMATION.
 *          It is documented in ntddk.h
 */
typedef struct _UM_KM_MESSAGE_HEADER
{
    /**
     * @brief   Must be set to UM_KM_CALLBACK_SIGNATURE.
     */
    uint32_t    ProviderSignature;

    /**
     * @brief   Must be set to UM_KM_REQUEST_TYPE.
     *          Corresponds to Action field in original structure.
     */
    uint32_t    RequestType;

    /**
     * @brief   Reserved - must be zero.
     *          Corresponds to TableId in the original structure.
     */
    uint32_t    Reserved;

    /**
     * @brief   Contains the length of the message, excluding the header.
     *          Corresponds to TableBufferLength in the original structure.
     */
    uint32_t    BufferLength;

    /* Comes after the message */
    /*uint8_t   Buffer[] */
} UM_KM_MESSAGE_HEADER;

/**
 * @brief   This must hold true on all platforms.
 */
static_assert(sizeof(UM_KM_MESSAGE_HEADER) == 16,
              "The size of UM_KM_MESSAGE header is not constant!");


/**
 * @brief   The process connected to an ALPC port.
 */
#define UM_KM_MESSAGE_TYPE_ALPC_PORT_CONNECTED              0
/**
 * @brief   The process sent an interesting message over one of the
 *          monitored RPC interfaces.
 */
#define UM_KM_MESSAGE_TYPE_INTERESTING_RPC_MESSAGE          1

/**
 * @brief       Getter for message type starting from the UM_KM_MESSAGE_HEADER.
 *              All messages must have an uint64_t field coming right after.
 *
 * @param[in]   Header - the message header.
 *
 * @return      The message type (UM_KM_MESSAGE_TYPE_*)
 */
inline uint64_t
UmKmMessageGetType(
    _In_ UM_KM_MESSAGE_HEADER*Header
) noexcept(true)
{
    uint64_t* type = static_cast<uint64_t*>(xpf::AlgoAddToPointer(Header,
                                                                  sizeof(UM_KM_MESSAGE_HEADER)));
    return *type;
}


/**
 * @brief   A message notification passed to the kernel
 *          saying that the process connected to a specific
 *          ALPC port.
 */
typedef struct _UM_KM_ALPC_PORT_CONNECTED
{
    /**
     * @brief   The header of the message. Contains metadata
     *          to properly distinguish between notifications.
     */
    UM_KM_MESSAGE_HEADER Header;

    /**
     * @brief   A header to identify the message type.
     *          For this particular message, this is always
     *          UM_KM_MESSAGE_TYPE_ALPC_PORT_CONNECTED.
     */
    uint64_t    MessageType;

    /**
     * @brief   Contains the name of the port that the process
     *          connected to. This is capped to 512 characters.
     */
    wchar_t     PortName[512];
} UM_KM_ALPC_PORT_CONNECTED;

/**
 * @brief   A message notification passed to the kernel
 *          saying that a message on a monitored RPC interface
 *          was found.
 */
typedef struct _UM_KM_INTERESTING_RPC_MESSAGE
{
    /**
     * @brief   The header of the message. Contains metadata
     *          to properly distinguish between notifications.
     */
    UM_KM_MESSAGE_HEADER Header;

    /**
     * @brief   A header to identify the message type.
     *          For this particular message, this is always
     *          UM_KM_MESSAGE_TYPE_INTERESTING_RPC_MESSAGE.
     */
    uint64_t    MessageType;

    /**
     * @brief   The GUID of the interface on which the message occured.
     */
    uuid_t      InterfaceGuid;

    /**
     * @brief   The procedure number inside the interface which was called.
     */
    uint64_t    ProcedureNumber;

    /**
     * @brief   The transfer syntax flag. One of the LRPC_TRANSFER_SYNTAX_* values.
     */
    uint64_t    TransferSyntaxFlag;

    /**
     * @brief   The buffer containing the request.
     */
    uint8_t     Buffer[0x1000];
} UM_KM_INTERESTING_RPC_MESSAGE;
