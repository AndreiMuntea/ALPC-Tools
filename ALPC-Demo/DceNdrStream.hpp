/**
 * @file        ALPC-Tools/ALPC-Demo/DceNdrStream.hpp
 *
 * @brief       In this file we implement a stream class to support
 *              serialization and deserialization of data.
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


namespace AlpcRpc
{
namespace DceNdr
{
 /**
  * @brief       This is the allocator used internally by the dce-ndr serializable objects.
  *              It is defined here for convenience. So we can easily swap when we need.
  *
  * @details     ALPC-Demo executable application is compiled with LARGEADDRESSAWARE:NO linker switch.
  *              A more legit solution would maybe be to create a custom allocator which allocates
  *              in the [0, 4GB] space for x64. This is required as the protocol requires as to
  *              serialize addresses, and we are limited to 4 bytes to represent those.
  *
  * @note        Please note that this is only for x64 configuration, and it only impacts the SERIALIZATION
  *              of the messages. If you only want to DESERIALIZE, well known messages, an ordinary allocator
  *              is more than enough.
  */
#define DceAllocator      xpf::PolymorphicAllocator{ .AllocFunction = &xpf::SplitAllocator::AllocateMemory,        \
                                                      .FreeFunction = &xpf::SplitAllocator::FreeMemory }

/**
 * @brief   This class is used to store serialized data.
 *          It provides a convenience wrapper over read and
 *          write operations taking care of memory management.
 *          By using only xpf api and objects, this is cross platform.
 */
class RwStream final
{
 public:
    /**
     * @brief   Default constructor.
     */
     RwStream(void) noexcept(true) = default;

    /**
     * @brief   Default destructor.
     */
    ~RwStream(void) noexcept(true) = default;

    /**
     * @brief   Copy and move behavior are deleted.
     *          We can revisit this when the need arise.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(AlpcRpc::DceNdr::RwStream, delete);

    /**
     * @brief           This will serialize provided data into the given stream.
     *
     * @param[in]       Data            - a byte array to be serialized into the stream.
     * @param[in]       DataSize        - number of bytes provided by Data buffer.
     * @param[in]       DataAlignment   - required alignment for the data; before writing
     *                                    to the stream, this function will ensure that the
     *                                    stream cursor is aligned to this number by properly
     *                                    appending zeroes.
     *
     * @return          A proper NTSTATUS to signal the success or failure.
     *
     * @note            The operation is destructive towards the Stream.
     *                  So, if anything fails, there are no guarantees that the stream is intact, its value
     *                  must be disregarded by the caller.
     */
    _Must_inspect_result_
    inline NTSTATUS XPF_API
    SerializeRawData(
        _In_ _Const_ const void* Data,
        _In_ size_t DataSize,
        _In_ uint8_t DataAlignment
    ) noexcept(true)
    {
        XPF_ASSERT(nullptr != Data);
        XPF_ASSERT(0 != DataSize);
        XPF_ASSERT(0 != DataAlignment);

        /* First we align the stream. */
        NTSTATUS status = this->AlignForSerialization(DataAlignment);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* And then we serialize the data. */
        return this->WriteRawBufferInternal(static_cast<const uint8_t*>(Data),
                                            DataSize);
    }

    /**
     * @brief           This will align the stream for serialization adjusting the write cursor.
     *
     * @param[in]       DataAlignment   - required alignment for the data.
     *
     * @return          A proper NTSTATUS to signal the success or failure.
     *
     * @note            The operation is destructive towards the Stream.
     *                  So, if anything fails, there are no guarantees that the stream is intact, its value
     *                  must be disregarded by the caller.
     */
    _Must_inspect_result_
    inline NTSTATUS XPF_API
    AlignForSerialization(
        _In_ uint8_t DataAlignment
    ) noexcept(true)
    {
        XPF_ASSERT(0 != DataAlignment);

        while (this->m_WriteCursor % DataAlignment != 0)
        {
            uint8_t lost = 0;
            NTSTATUS status = this->WriteRawBufferInternal(&lost,
                                                           sizeof(lost));
            if (!NT_SUCCESS(status))
            {
                return status;
            }
        }
        return STATUS_SUCCESS;
    }

    /**
     * @brief           This will deserialize provided data into the given stream.
     *
     * @param[in,out]   Data            - a byte array which will contain the deserialized data.
     * @param[in]       DataSize        - number of bytes to be deserialized from the stream;
     *                                    it is the caller responsibility to ensure that Data is large
     *                                    enough to accomodate this number.
     * @param[in]       DataAlignment   - required alignment for the data; before reading
     *                                    from the stream, this function will ensure that the
     *                                    stream cursor is aligned to this number by properly
     *                                    skipping a number of characters.
     *
     * @return          A proper NTSTATUS to signal the success or failure.
     *
     * @note            The operation is destructive towards the Stream.
     *                  So, if anything fails, there are no guarantees that the stream is intact, its value
     *                  must be disregarded by the caller.
     */
    _Must_inspect_result_
    inline NTSTATUS XPF_API
    DeserializeRawData(
        _Inout_ void* Data,
        _In_ size_t DataSize,
        _In_ uint8_t DataAlignment
    ) noexcept(true)
    {
        XPF_ASSERT(nullptr != Data);
        XPF_ASSERT(0 != DataSize);
        XPF_ASSERT(0 != DataAlignment);

        /* First we align the stream. */
        NTSTATUS status = this->AlignForDeserialization(DataAlignment);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* And then we deserialize the data. */
        return this->ReadRawBufferInternal(static_cast<uint8_t*>(Data),
                                           DataSize);
        }

    /**
     * @brief           This will align the stream for deserialization adjusting the read cursor.
     *
     * @param[in]       DataAlignment   - required alignment for the data.
     *
     * @return          A proper NTSTATUS to signal the success or failure.
     *
     * @note            The operation is destructive towards the Stream.
     *                  So, if anything fails, there are no guarantees that the stream is intact, its value
     *                  must be disregarded by the caller.
     */
    _Must_inspect_result_
    inline NTSTATUS XPF_API
    AlignForDeserialization(
        _In_ uint8_t DataAlignment
    ) noexcept(true)
    {
        XPF_ASSERT(0 != DataAlignment);

        while (this->m_ReadCursor % DataAlignment != 0)
        {
            uint8_t lost = 0;
            NTSTATUS status = this->ReadRawBufferInternal(&lost,
                                                          sizeof(lost));
            if (!NT_SUCCESS(status))
            {
                return status;
            }
        }

        return STATUS_SUCCESS;
    }

    /**
     * @brief           Getter for underlying buffer.
     *
     * @return          Const reference to the underlying buffer.
     */
    inline const xpf::Buffer& XPF_API
    Buffer(
        void
    ) const noexcept(true)
    {
        return this->m_Buffer;
    }

 private:
    /**
     * @brief           This will serialize provided data into the given stream.
     *
     * @param[in]       Data            - a byte array to be serialized into the stream.
     * @param[in]       DataSize        - number of bytes provided by Data buffer.
     *
     * @return          A proper NTSTATUS to signal the success or failure.
     *
     * @note            The operation is destructive towards the Stream.
     *                  So, if anything fails, there are no guarantees that the stream is intact, its value
     *                  must be disregarded by the caller.
     */
    _Must_inspect_result_
    inline NTSTATUS XPF_API
    WriteRawBufferInternal(
        _In_ const uint8_t* Data,
        _In_ size_t DataSize
    ) noexcept(true)
    {
        XPF_ASSERT(nullptr != Data);
        XPF_ASSERT(0 != DataSize);

        size_t finalWriteCursor = 0;
        bool success = xpf::ApiNumbersSafeAdd(this->m_WriteCursor,
                                              DataSize,
                                              &finalWriteCursor);
        if (!success)
        {
            return STATUS_INTEGER_OVERFLOW;
        }

        NTSTATUS status = this->m_Buffer.Resize(finalWriteCursor);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        xpf::ApiCopyMemory(static_cast<uint8_t*>(this->m_Buffer.GetBuffer()) + this->m_WriteCursor,
                           Data,
                           DataSize);
        this->m_WriteCursor = finalWriteCursor;
        return STATUS_SUCCESS;
    }

    /**
     * @brief           This will deserialize provided data into the given stream.
     *
     * @param[in,out]   Data            - a byte array which will contain the deserialized data.
     * @param[in]       DataSize        - number of bytes to be deserialized from the stream;
     *                                    it is the caller responsibility to ensure that Data is large
     *                                    enough to accomodate this number.
     *
     * @return          A proper NTSTATUS to signal the success or failure.
     *
     * @note            The operation is destructive towards the Stream.
     *                  So, if anything fails, there are no guarantees that the stream is intact, its value
     *                  must be disregarded by the caller.
     */
    _Must_inspect_result_
    inline NTSTATUS XPF_API
    ReadRawBufferInternal(
        _Inout_ uint8_t* Data,
        _In_ size_t DataSize
    ) noexcept(true)
    {
        XPF_ASSERT(nullptr != Data);
        XPF_ASSERT(0 != DataSize);

        size_t finalReadCursor = 0;
        bool success = xpf::ApiNumbersSafeAdd(this->m_ReadCursor,
                                              DataSize,
                                              &finalReadCursor);
        if (!success)
        {
            return STATUS_INTEGER_OVERFLOW;
        }

        if (finalReadCursor > this->m_Buffer.GetSize())
        {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        xpf::ApiCopyMemory(Data,
                           static_cast<const uint8_t*>(this->m_Buffer.GetBuffer()) + this->m_ReadCursor,
                           DataSize);
        this->m_ReadCursor = finalReadCursor;
        return STATUS_SUCCESS;
    }

 private:
     xpf::Buffer m_Buffer{ DceAllocator };
     size_t m_ReadCursor = 0;
     size_t m_WriteCursor = 0;
};  // class Stream
};  // namespace DceNdr
};  // namespace AlpcRpc
