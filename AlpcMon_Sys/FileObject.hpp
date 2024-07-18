/**
 * @file        ALPC-Tools/AlpcMon_Sys/FileObject.hpp
 *
 * @brief       Header file which contains a wrapper file object.
 *              Can be used safely to interact with a file.
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
namespace File
{

/**
 * @brief   The requested access to the file must include read rights.
 */
#define XPF_FILE_ACCESS_READ        0x00000001

/**
 * @brief   The requested access to the file must include writi rights.
 */
#define XPF_FILE_ACCESS_WRITE       0x00000002

/**
 * @brief   This class is a wrapper to interact with files.
 */
class FileObject
{
 private:
    /**
     * @brief   Constructor. It is private as the static method
     *          Create must be used to ensure we do not end up
     *          with partly valid objects.
     */
     FileObject(void) noexcept(true) = default;
 public:
     /**
     * @brief Destructor. Cleans up all resources.
     */
    ~FileObject(void) noexcept(true);

    /**
     * @brief   The copy and move semantics is prohibited.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(FileObject, delete);

    /**
     * @brief       Properly constructs a file object.
     *
     * @param[in]   FilePath        - The file path to the file which is to be opened.
     * @param[in]   DesiredAccess   - Access desired to the mapping. See  XPF_FILE_ACCESS_* flags
     *                                for possible values to this parameter.
     * @param[out]  FileObject      - On success, it will contain a properly initialized object,
     *                                empty on failure.
     *
     * @return      A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    static NTSTATUS XPF_API
    Create(
        _In_ _Const_ const xpf::StringView<wchar_t>& FilePath,
        _In_ _Const_ const uint32_t& DesiredAccess,
        _Out_ xpf::Optional<SysMon::File::FileObject>* FileObject
    ) noexcept(true);

    /**
     * @brief           Performs a read operation on the file.
     *
     * @param[in]       Offset  - Contains the offset in the file where to start reading.
     * @param[in,out]   Buffer  - The buffer to be read. On input, this is pre-allocated with a
     *                            given size. The read operation will attempt to accomodate up to
     *                            the size specified here. On the last read, it may happen that we
     *                            do not have enough bytes left in file, in this case, the buffer
     *                            is resized to store only the read bytes.
     *
     * @return      A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS XPF_API
    Read(
        _In_ const uint64_t& Offset,
        _Inout_ xpf::Buffer* Buffer
    ) noexcept(true);

    /**
     * @brief           Performs a write operation on the mapping.
     *
     * @param[in]       Buffer      - The buffer to be written.
     * @param[in]       BufferSize  - The Number of bytes to be written.
     *
     * @return          A proper NTSTATUS error code.
     */
    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS XPF_API
    Write(
        _In_ const void* Buffer,
        _In_ const size_t& BufferSize
    ) noexcept(true);

    /**
     * @brief   Getter for the file size.
     *
     * @return  The file size in bytes.
     */
    inline uint64_t
    FileSize(
        void
    ) noexcept(true)
    {
        return this->m_FileSize;
    }

 private:
    uint64_t m_FileSize = 0;
    void* m_FileHandle = nullptr;

    /**
     * @brief   Default MemoryAllocator is our friend as it requires access to the private
     *          default constructor. It is used in the Create() method to ensure that
     *          no partially constructed objects are created but instead they will be
     *          all fully initialized.
     */
    friend class xpf::MemoryAllocator;
};  // class FileObject

/**
 * @brief       Retrieves the file name from a FILE_OBJECT*.
 *
 * @param[in]   FileObject - pointer to a FILE_OBJECT structure.
 * @param[out]  FileName   - the retrieved file name.
 *
 * @return      A proper NTSTATUS error code.
 *
 * @note        This internaly uses FltGetFileNameInformation to
 *              retrieve the file name. It does so in a safe manner
 *              so no deadlocks should occur.
 */
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS XPF_API
QueryFileNameFromRawFileObject(
    _In_ PVOID FileObject,
    _Out_ xpf::String<wchar_t>* FileName
) noexcept(true);
};  // namespace File
};  // namespace SysMon
