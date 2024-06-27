/**
 * @file        ALPC-Tools/ALPC-Demo/DceNdr.hpp
 *
 * @brief       In this file we implement dce-ndr serialization.
 *              This is not exhaustive as we only did what we needed.
 *              More can be added later, when we encounter more complex
 *              types. These should provide only basic functionality.
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
#include "DceNdrStream.hpp"
#include "NtAlpcApi.hpp"

namespace AlpcRpc
{
namespace DceNdr
{

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       BASE DCE-NDR SERIALIZABLE OBJECT                                          |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   Base class of all supported DceNdr serializable objects.
 *          All must derived from this one.
 */
class DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceSerializableObject(void) noexcept(true) = default;

     /**
      * @brief  Default destructor.
      */
     virtual ~DceSerializableObject(void) noexcept(true) = default;

 protected:
     /**
      * @brief  Copy and Move are defaulted and protected.
      *         Each derived class must implement its own behavior.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(AlpcRpc::DceNdr::DceSerializableObject, default);

 public:
     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     virtual NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) = 0;

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     virtual NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) = 0;
};  // class SerializableObject

/**
 * @brief   Helper class which can be used to ease the serialization
 *          and deserialization on all DCE-NDR serializable objects.
 *          Wrapper over DceNdrStream and works with generic
 *          DceSerializableObject references.
 */
class DceMarshallBuffer final
{
 public:
     /**
      * @brief      Constructor. Takes the transfer syntax to be used.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      */
     DceMarshallBuffer(
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) : m_TransferSyntax{ LrpcTransferSyntax }
     {
         XPF_NOTHING();
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceMarshallBuffer(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are deleted. We can revisit this
      *         in the future if the need arise.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(AlpcRpc::DceNdr::DceMarshallBuffer, delete);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *
      * @param[in]      Object - The object to be serialized into the stream.
      *
      * @return         A reference to the marshall buffer after marshalling the object.
      *                 Can be used to chain multiple operations of marshalling and unmarshalling.
      *
      * @note           If the underlying stream is corrupted, the Object is not serialized.
      *                 It is the caller responsibility to check status.
      */
     inline DceMarshallBuffer&
     XPF_API
     Marshall(
         _In_ const AlpcRpc::DceNdr::DceSerializableObject& Object
     ) noexcept(true)
     {
         if (NT_SUCCESS(this->m_StreamStatus))
         {
             this->m_StreamStatus = Object.Marshall(this->m_RwStream,
                                                    this->m_TransferSyntax);
         }
         return *this;
     }

     /**
      * @brief          This method takes care of deserializing the object from DCE-NDR format.
      *
      * @param[in,out]  Object - The object to be deserialized from the stream.
      *
      * @return         A reference to the marshall buffer after marshalling the object.
      *                 Can be used to chain multiple operations of marshalling and unmarshalling.
      *
      * @note           If the underlying stream is corrupted, the Object is not deserialized.
      *                 It is the caller responsibility to check status.
      */
     inline DceMarshallBuffer&
     XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::DceSerializableObject& Object
     ) noexcept(true)
     {
         if (NT_SUCCESS(this->m_StreamStatus))
         {
             this->m_StreamStatus = Object.Unmarshall(this->m_RwStream,
                                                      this->m_TransferSyntax);
         }
         return *this;
     }

     /**
      * @brief          Getter for the underlying stream status.
      *
      * @return         The status of stream. If this is a failure status
      *                 it represents the first failure which was encountered during
      *                 a marshalling or unmarshalling. The stream must be considered
      *                 corrupted and its results inconsistent.
      */
     inline NTSTATUS XPF_API
     Status(
         void
     ) noexcept(true)
     {
         return this->m_StreamStatus;
     }

    /**
     * @brief           Getter for underlying buffer.
     *
     * @return          Const reference to the underlying buffer.
     */
    inline const xpf::Buffer<DceAllocator>& XPF_API
    Buffer(
        void
    ) const noexcept(true)
    {
        return this->m_RwStream.Buffer();
    }

    /**
     * @brief          Used to marshall raw data into the stream.
     *
     * @param[in]      Buffer - to be marshalled into the stream.
     *
     * @return         void.
     */
    inline void XPF_API
    MarshallRawBuffer(
        _In_ _Const_ const xpf::Buffer<DceAllocator>&Buffer
    ) noexcept(true)
    {
        if (NT_SUCCESS(this->m_StreamStatus))
        {
            this->m_StreamStatus = this->m_RwStream.SerializeRawData(Buffer.GetBuffer(),
                                                                     Buffer.GetSize(),
                                                                     1);
        }
    }

 private:
     /**
      * @brief  This controls whether we can keep serializing and deserializing.
      *         Once a failure occurs, the underlying DceNdr stream is considered corrupted.
      *         The first status is saved, so it can be inspected by the caller.
      */
     NTSTATUS m_StreamStatus = STATUS_SUCCESS;

     /**
      * @brief  This is the underlying serializing stream. The class wraps over a RwStream
      *         and provides chain operations to ease the access.
      */
     AlpcRpc::DceNdr::RwStream m_RwStream;

     /**
      * @brief  The transfer syntax to be used when serializing and deserializing.
      */
     uint32_t m_TransferSyntax = xpf::NumericLimits<uint32_t>::MaxValue();
};  // class DceMarshallBuffer


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                             PRIMITIVE TYPE DCE-NDR SERIALIZABLE OBJECT                                          |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   This is the class that takes care of serializing native types.
 *          They are just written directly into the data stream as a byte array.
 *          Ideal for structs, ints, guids, plain data in general.
 */
template <class Type>
class DcePrimitiveType final : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DcePrimitiveType(void) noexcept(true)
     {
         static_assert(noexcept(Type()),
                       "Object can not be safely constructed!");
     }

     /**
      * @brief      Used to initialize an object using provided data.
      *
      * @param[in]  Element - the element to be stored as underlying data.
      */
     DcePrimitiveType(
         _In_ const Type& Element
     ) noexcept(true) : DceSerializableObject(),
                        m_Data{ Element }
     {
        static_assert(noexcept(Type(Element)),
                      "Object can not be safely constructed!");
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DcePrimitiveType(void) noexcept(true)
     {
        static_assert(noexcept(m_Data.~Type()),
                      "Object can not be safely destructed!");
     }

     /**
      * @brief  Copy and Move are defaulted. As this should be used
      *         only with plain data, we should be good.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DcePrimitiveType, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *                 Data is simply written directly into the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
        /* For now allow only these two values. */
        XPF_VERIFY( LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_DCE ||
                    LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64 );


        return Stream.SerializeRawData(&this->m_Data,
                                       sizeof(this->m_Data),
                                       alignof(Type));
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 Data is simply read directly from the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
        /* For now allow only these two values. */
        if ((LrpcTransferSyntax != LRPC_TRANSFER_SYNTAX_DCE) &&
            (LrpcTransferSyntax != LRPC_TRANSFER_SYNTAX_NDR64))
        {
            return STATUS_UNKNOWN_REVISION;
        }

        return Stream.DeserializeRawData(&this->m_Data,
                                         sizeof(this->m_Data),
                                         alignof(Type));
     }

     /**
      * @brief      Getter for the underlying data type.
      *
      * @return     A const reference to the data object.
      */
     inline const Type& XPF_API
     Data(
         void
     ) const noexcept(true)
     {
         return this->m_Data;
     }

 private:
     Type m_Data{};
};  // class PrimitiveType

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                             ENUMERATION TYPE DCE-NDR SERIALIZABLE OBJECT                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief   NDR64 supports all simple types defined by NDR (as specified in [C706] section 14.2)
 *          with the same alignment requirements except for enumerated types, which MUST be
 *          represented as signed long integers (4 octets) in NDR64.
 */
class DceEnumerationType final : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceEnumerationType(void) noexcept(true) = default;

     /**
      * @brief      Used to initialize an object using provided data.
      *
      * @param[in]  Element - the element to be stored as underlying data.
      */
     DceEnumerationType(
         _In_ uint16_t Element
     ) noexcept(true) : DceSerializableObject(),
                        m_Data{ Element }
     {
         XPF_NOTHING();
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceEnumerationType(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted. As this should be used
      *         only with plain data, we should be good.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceEnumerationType, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *                 Data is simply written directly into the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         if (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64)
         {
             AlpcRpc::DceNdr::DcePrimitiveType<uint32_t> u32Data = this->m_Data.Data();
             return u32Data.Marshall(Stream,
                                     LrpcTransferSyntax);
         }
         else
         {
            return this->m_Data.Marshall(Stream,
                                         LrpcTransferSyntax);
         }
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 Data is simply read directly from the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         if (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64)
         {
             AlpcRpc::DceNdr::DcePrimitiveType<uint32_t> u32Data = this->m_Data.Data();
             status = u32Data.Unmarshall(Stream,
                                         LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
             if (u32Data.Data() > xpf::NumericLimits<uint16_t>::MaxValue())
             {
                 return STATUS_INTEGER_OVERFLOW;
             }
             this->m_Data = static_cast<uint16_t>(u32Data.Data());
             return STATUS_SUCCESS;
         }
         else
         {
            return this->m_Data.Unmarshall(Stream,
                                           LrpcTransferSyntax);
         }
     }

     /**
      * @brief      Getter for the underlying data type.
      *
      * @return     A const reference to the data object.
      */
     inline const uint16_t& XPF_API
     Data(
         void
     ) const noexcept(true)
     {
         return this->m_Data.Data();
     }

 private:
     AlpcRpc::DceNdr::DcePrimitiveType<uint16_t> m_Data;
};  // class DceEnumerationType

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                             A "size_t" like object for DCE-NDR and NDR64                                        |
/// |                             is 32 bits on DCE-NDR and 64 on NDR64.                                              |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 * @brief   We need to diverge for offset sizes and alignment between the two
 *          syntaxes. This is a dummy type introduce to take care of it.
 *          It is 32 bits on DCE-NDR and 64 on NDR64.
 */
class DceSizeT final : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceSizeT(void) noexcept(true) = default;

     /**
      * @brief      Used to initialize an object using provided data.
      *
      * @param[in]  Element - the element to be stored as underlying data.
      */
     DceSizeT(
         _In_ uint64_t Element
     ) noexcept(true) : DceSerializableObject(),
                        m_Data{ Element }
     {
         XPF_NOTHING();
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceSizeT(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted. As this should be used
      *         only with plain data, we should be good.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceSizeT, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *                 Data is simply written directly into the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         if (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64)
         {
            return this->m_Data.Marshall(Stream,
                                         LrpcTransferSyntax);
         }
         else
         {
             AlpcRpc::DceNdr::DcePrimitiveType<uint32_t> u32Data;
             if (this->m_Data.Data() > xpf::NumericLimits<uint32_t>::MaxValue())
             {
                 return STATUS_INTEGER_OVERFLOW;
             }

             u32Data = static_cast<uint32_t>(this->m_Data.Data());
             return u32Data.Marshall(Stream,
                                     LrpcTransferSyntax);
         }
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 Data is simply read directly from the stream.
      *                 By default this is not aligned. Caller can align as needed.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         if (LrpcTransferSyntax == LRPC_TRANSFER_SYNTAX_NDR64)
         {
            return this->m_Data.Unmarshall(Stream,
                                           LrpcTransferSyntax);
         }
         else
         {
             AlpcRpc::DceNdr::DcePrimitiveType<uint32_t> u32Data;
             NTSTATUS status = u32Data.Unmarshall(Stream,
                                                  LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }

             this->m_Data = u32Data.Data();
             return STATUS_SUCCESS;
         }
     }

     /**
      * @brief      Getter for the underlying data type.
      *
      * @return     A const reference to the data object.
      */
     inline const uint64_t& XPF_API
     Data(
         void
     ) const noexcept(true)
     {
         return this->m_Data.Data();
     }

 private:
     AlpcRpc::DceNdr::DcePrimitiveType<uint64_t> m_Data;
};  // class DceSizeT

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                             UNIQUE POINTER TYPE DCE-NDR SERIALIZABLE OBJECT                                     |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   This is useful when we need to serialize an address in the stream.
 *          It is used by UniquePointer below.
 */
class DceRawPointer final : public DceSerializableObject
{
 public:
     /**
      * @brief  Default constructor.
      */
     DceRawPointer(void) noexcept(true) = default;

     /**
      * @brief          Used to initialize an object using provided data.
      *
      * @param[in]      Address - The address to be stored as data.
      */
     DceRawPointer(
         _In_opt_ _Const_ const void* Address
     ) noexcept(true) : DceSerializableObject(),
                        m_Data{ Address }
     {
         XPF_NOTHING();
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceRawPointer(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceRawPointer, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         AlpcRpc::DceNdr::DceSizeT address = xpf::AlgoPointerToValue(this->m_Data);
         return address.Marshall(Stream, LrpcTransferSyntax);
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 First the referent is read. If it is null, we are done.
      *                 Otherwise, we also read the data.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         AlpcRpc::DceNdr::DceSizeT address = 0;
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         status = address.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }
         if (address.Data() > xpf::NumericLimits<size_t>::MaxValue())
         {
             return STATUS_INTEGER_OVERFLOW;
         }

         size_t ptrSizeAddress = static_cast<size_t>(address.Data());

         this->m_Data = reinterpret_cast<void*>(ptrSizeAddress);
         return STATUS_SUCCESS;
     }

     /**
      * @brief      Getter for the underlying data address.
      *
      * @return     A const pointer to underlying address.
      */
     inline const void*
     Data(
         void
     ) const noexcept(true)
     {
         return this->m_Data;
     }

 private:
     const void* m_Data = nullptr;
};

/**
 * @brief   This is the class that takes care of serializing Unique Pointers as Top Level Pointers.
 *          See https://pubs.opengroup.org/onlinepubs/9629399/chap14.htm#tagcjh_19_03_11
 *          Relevant documentation snippets are extracted from there and are available below.
 *
 *          Instead of using an actual unique pointer to store data, we'll use a shared pointer
 *          The class name here only shows how it is serialized, it does not relate with
 *          data ownership model. This is useful as we might use the NdrPtr class below
 *          and we might refer to the same object, but we only need to serialize its address.
 *
 * @details Unique pointers, which can be null and cannot be aliases, and are transmitted as full pointers.
 *          NDR represents a null full pointer as an unsigned long integer with the value 0 (zero).
 *          NDR represents the first instance in a octet stream of a non-null full pointer in two parts:
 *              the first part is a non-zero unsigned long integer that identifies the referent;
 *              the second part is the representation of the referent.
 *
 * @note    NULL pointer is serialized as:
 *                                                    4 BYTES
 *                                                  ------------
 *                                                  | ZERO (0) |
 *                                                  ------------
 * @note    Non-NULL pointer is serialized as:
 *                                                     4 BYTES         Data Size
 *                                                   --------------   ------------
 *                                                  | REFERENT ID |   | REFERENT |
 *                                                  --------------    ------------
 *
 * @note    If the input and output octet streams pertaining to one remote procedure call contain several pointers
 *          that point to the same thing, the first of these pointers to be transmitted is considered primary and
 *          the others are considered aliases. As unique pointers can not be aliases, we don't need to treat this case.
 */
template <class Type>
class DceUniquePointer final : public DceSerializableObject
{
    static_assert(xpf::IsTypeBaseOf<DceSerializableObject, Type>(),
                  "Type from DceUniquePointer<Type> must derive from DceSerializableObject");
 public:
     /**
      * @brief  Default constructor.
      */
     DceUniquePointer(void) noexcept(true) = default;

     /**
      * @brief          Used to initialize an object using provided data.
      *
      * @param[in]      Pointer - the element to be stored as underlying data.
      *                           One reference will be incremented to be stored by m_Data.
      */
     DceUniquePointer(
         _In_ _Const_ const xpf::SharedPointer<Type, DceAllocator>& Pointer
     ) noexcept(true) : DceSerializableObject(),
                        m_Data{ Pointer }
     {
         XPF_NOTHING();
     }

     /**
      * @brief          Used to initialize an object using a type data.
      *                 Attempts to allocate memory for a shared pointer.
      *                 On fail, the shared pointer internally will be nullptr.
      *                 Data() can be used to inspect such failures.
      *
      * @param[in]      Data - the element to be stored as underlying data.
      */
     DceUniquePointer(
         _In_ _Const_ const Type& Data
     ) noexcept(true) : DceSerializableObject()
     {
         this->m_Data = xpf::MakeShared<Type, DceAllocator>(Data);
     }

     /**
      * @brief  Default destructor.
      */
     virtual ~DceUniquePointer(void) noexcept(true) = default;

     /**
      * @brief  Copy and Move are defaulted.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(DceUniquePointer, default);

     /**
      * @brief          This method takes care of serializing the object in DCE-NDR format.
      *                 If pointer is null, a null referent is written.
      *                 Otherwise, the referent and data are serialized one after another.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         AlpcRpc::DceNdr::DceRawPointer referent;

         /* If underlying data is empty, we'll serialize a null referent. */
         if (!this->m_Data.IsEmpty())
         {
             referent = this->Data();
         }

         /* First the referent. */
         NTSTATUS status = referent.Marshall(Stream, LrpcTransferSyntax);
         if (NT_SUCCESS(status))
         {
             /* And then the actual data if any. */
             if (!this->m_Data.IsEmpty())
             {
                 status = (*this->m_Data).Marshall(Stream, LrpcTransferSyntax);
             }
         }

         /* Send back the status. */
         return status;
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *                 First the referent is read. If it is null, we are done.
      *                 Otherwise, we also read the data.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         Type data{};
         AlpcRpc::DceNdr::DceRawPointer referent;
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         /* Clear the current pointer. */
         this->m_Data.Reset();

         /* First we deserialize the referent id. */
         status = referent.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* If referent is null, we have a null pointer: nothing left to do. */
         if (referent.Data() == nullptr)
         {
             return STATUS_SUCCESS;
         }

         /* Non-null referent. Unmarshall the data. */
         status = data.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* Now construct the unique ptr from the data. */
         this->m_Data = xpf::MakeShared<Type, DceAllocator>(xpf::Move(data));
         return (this->m_Data.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                                         : STATUS_SUCCESS;
     }

     /**
      * @brief      Getter for the underlying data type.
      *
      * @return     A const reference to the data object.
      */
     inline const Type* XPF_API
     Data(
         void
     ) const noexcept(true)
     {
         return this->m_Data.Get();
     }

 private:
     xpf::SharedPointer<Type, DceAllocator> m_Data;
};  // class DceUniquePointer


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                      UNI-DIMENSIONAL ARRAY DCE-NDR SERIALIZABLE OBJECT                                          |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


/**
 * @brief   This controls the type of uni-dimensional array.
 *          For a detailed description see below class DceUniDimensionalArray.
 */
enum class DceUniDimensionalArrayType
{
    kConformant = 0,
    kVarying = 1,
    kConformantVarying = 2,
};  // enum class DceUniDimensionalArrayType

/**
 * @brief   This is the class that takes care of serializing Conformant Arrays.
 *          https://pubs.opengroup.org/onlinepubs/9629399/chap14.htm#tagfcjh_25
 *          Relevant documentation snippets are extracted from there and are available below.
 *
 * @details We will focus on uni-dimensional arrays. Using this class we will cover 3 different cases:
 *              1. Uni-Dimensional Conformant Array which is an array in which the maximum number of elements
 *                 is not known beforehand and therefore is included in the representation of the array.
 *              2. Uni-Dimensional Varying Array which is an array in which the actual number of elements
 *                 passed in a given call varies and therefore is included in the representation of the array.
 *              3. Uni-dimensional Conformant-varying Arrays which is an array both conformant and varying.
 *
 * @note    Conformant Array:
 *                                        4 BYTES       Data Size      Data Size
 *                                      -----------   -------------- --------------
 *                                      | MaxCount |  | Element 0  | | Element 1  | ...
 *                                      -----------   -------------- --------------
 *
 * @note    Varying Array:
 *                                      4 BYTES     4 BYTES    Data Size      Data Size 
 *                                     ----------- ----------- -------------- --------------
 *                                     |  Offset ||   Count  | | Element 0  | | Element 1  | ...
 *                                     ----------- ----------- -------------- --------------
 *
 * @note    Conformant Varying Array:
 *                                        4 BYTES      4 BYTES     4 BYTES     Data Size      Data Size 
 *                                     -----------  ---------- ----------- -------------- --------------
 *                                     |  MaxCount| |  Offset ||   Count  | | Element 0  | | Element 1  | ...
 *                                     ------------ ---------- ----------- -------------- --------------
 *
 * @note    We will only support the case where MaxCount is equal to the actual number of elements in the array,
 *          and the offset is always 0. For our use case, this suffice. When the need arise, we can implement
 *          support for other cases as well, but they were not required in our tests.
 */
template <class Type, DceUniDimensionalArrayType ArrayType>
class DceUniDimensionalArray final : public DceSerializableObject
{
    static_assert(xpf::IsTypeBaseOf<DceSerializableObject, Type>(),
                  "Type from DceUniDimensionalArray<Type> must derive from DceSerializableObject");
 public:
    /**
     * @brief  Default constructor.
     */
    DceUniDimensionalArray(void) noexcept(true) = default;

    /**
     * @brief           Used to initialize an object using provided data.
     *
     * @param[in]       Elements - the elements to be stored.
     *                             Will be copied into m_Data.
     */
    DceUniDimensionalArray(
        _In_ _Const_ const xpf::SharedPointer<xpf::Vector<Type, DceAllocator>, DceAllocator>& Elements
    ) noexcept(true) : DceSerializableObject(),
                       m_Data{Elements}
    {
        XPF_NOTHING();
    }

    /**
     * @brief  Default destructor.
     */
    virtual ~DceUniDimensionalArray(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are defaulted.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(DceUniDimensionalArray, default);

     /**
      * @brief          This method takes care of serializing the array in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         /* Sanity check that the data is not empty. */
         if (this->m_Data.IsEmpty())
         {
             return STATUS_NO_DATA_DETECTED;
         }
         const auto& elements = this->Data();

         /* Sanity check for size. We can't cast to uint32 if it exceeds max value. */
         if (elements.Size() > xpf::NumericLimits<uint32_t>::MaxValue())
         {
             return STATUS_INVALID_BUFFER_SIZE;
         }

         /* Marshall the number of elements. */
         status = this->MarshallMetadata(static_cast<uint32_t>(elements.Size()),
                                         Stream,
                                         LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* Marhsall each element. */
         for (size_t i = 0; i < elements.Size(); ++i)
         {
             status = elements[i].Marshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         /* We're done. Keep stream aligned. */
         return status;
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         NTSTATUS status = STATUS_UNSUCCESSFUL;
         xpf::Vector<Type, DceAllocator> elements;
         uint32_t count = 0;

         /* First we clear the underlying data. */
         this->m_Data = xpf::MakeShared<xpf::Vector<Type, DceAllocator>, DceAllocator>();
         if (this->m_Data.IsEmpty())
         {
             return STATUS_INSUFFICIENT_RESOURCES;
         }

         /* Now we deserialize the number of elements. */
         status = this->UnmarshallMetadata(&count,
                                           Stream,
                                           LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* Then we deserialize all elements. */
         for (uint32_t i = 0; i < count; ++i)
         {
             Type element{};

             status = element.Unmarshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
             status = elements.Emplace(xpf::Move(element));
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         /* In the end, we just replace data. */
         *this->m_Data = xpf::Move(elements);
         return status;
     }

     /**
      * @brief      Getter for the underlying data type.
      *
      * @return     A const reference to the data object.
      */
     inline const
     xpf::Vector<Type, DceAllocator>& XPF_API
     Data(
         void
     ) const noexcept(true)
     {
         return *this->m_Data.Get();
     }

 private:
     /**
      * @brief          This method takes care of serializing the metadata for the array type.
      *
      * @param[in]      Count  - the number of elements that the array has to be serialized.
      * @param[in,out]  Stream - where the data will be marshalled into.
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     MarshallMetadata(
         _In_ uint32_t Count,
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true)
     {
         AlpcRpc::DceNdr::DceSizeT dceNdrMaxCount{ Count };
         AlpcRpc::DceNdr::DceSizeT dceNdrOffset{ uint32_t{0} };
         AlpcRpc::DceNdr::DceSizeT dceNdrCount{ Count };

         NTSTATUS status = STATUS_UNSUCCESSFUL;

         /* MaxCount is serialized for conformant arrays. */
         if constexpr (ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kConformant ||
                       ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kConformantVarying)
         {
                status = dceNdrMaxCount.Marshall(Stream, LrpcTransferSyntax);
                if (!NT_SUCCESS(status))
                {
                    return status;
                }
         }

         /* Offset and Count are serialized for varying arrays. */
         if constexpr (ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kVarying ||
                       ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kConformantVarying)
         {
                status = dceNdrOffset.Marshall(Stream, LrpcTransferSyntax);
                if (!NT_SUCCESS(status))
                {
                    return status;
                }

                status = dceNdrCount.Marshall(Stream, LrpcTransferSyntax);
                if (!NT_SUCCESS(status))
                {
                    return status;
                }
         }

         /* All good. */
         return status;
     }

     /**
      * @brief          This method takes care of deserializing the metadata for the array type.
      *
      * @param[out]     Count  - the number of elements that the array has serialized.
      * @param[in,out]  Stream - where the data will be unmarshalled from.
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     UnmarshallMetadata(
         _Out_ uint32_t* Count,
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true)
     {
         AlpcRpc::DceNdr::DceSizeT dceNdrMaxCount{ uint32_t{0} };
         AlpcRpc::DceNdr::DceSizeT dceNdrOffset{ uint32_t{0} };
         AlpcRpc::DceNdr::DceSizeT dceNdrCount{ uint32_t{0} };

         NTSTATUS status = STATUS_UNSUCCESSFUL;

         XPF_ASSERT(nullptr != Count);

         /* MaxCount is deserialized for conformant arrays. */
         if constexpr (ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kConformant ||
                       ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kConformantVarying)
         {
                status = dceNdrMaxCount.Unmarshall(Stream, LrpcTransferSyntax);
                if (!NT_SUCCESS(status))
                {
                    return status;
                }
                if (dceNdrMaxCount.Data() > xpf::NumericLimits<uint32_t>::MaxValue())
                {
                    return STATUS_INTEGER_OVERFLOW;
                }
                *Count = static_cast<uint32_t>(dceNdrMaxCount.Data());
         }

         /* Offset and Count are deserialized for varying arrays. Always 0. */
         if constexpr (ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kVarying ||
                       ArrayType == AlpcRpc::DceNdr::DceUniDimensionalArrayType::kConformantVarying)
         {
                status = dceNdrOffset.Unmarshall(Stream, LrpcTransferSyntax);
                if (!NT_SUCCESS(status))
                {
                    return status;
                }
                if (dceNdrOffset.Data() != 0)
                {
                    /* It is not currently supported. */
                    return STATUS_NOT_SUPPORTED;
                }
                status = dceNdrCount.Unmarshall(Stream, LrpcTransferSyntax);
                if (!NT_SUCCESS(status))
                {
                    return status;
                }
                if (dceNdrCount.Data() > xpf::NumericLimits<uint32_t>::MaxValue())
                {
                    return STATUS_INTEGER_OVERFLOW;
                }
                *Count = static_cast<uint32_t>(dceNdrCount.Data());
         }

         /* All good. */
         return status;
     }

 private:
     xpf::SharedPointer<xpf::Vector<Type, DceAllocator>,
                        DceAllocator> m_Data;
};  // class DceConformantArray

/**
 * @brief   Arrays of pointers are serialized a bit different.
 *          The pointers are embedded, and their actual serialization will be deferred.
 */
template <class Type, DceUniDimensionalArrayType ArrayType>
class DceUniDimensionalPointerArray final : public DceSerializableObject
{
    static_assert(xpf::IsTypeBaseOf<DceSerializableObject, Type>(),
                  "Type from DceUniDimensionalPointerArray<Type> must derive from DceSerializableObject");
 public:
    /**
     * @brief  Default constructor.
     */
     DceUniDimensionalPointerArray(void) noexcept(true) = default;

    /**
     * @brief           Used to initialize an object using provided data.
     *
     * @param[in]       Elements - the elements to be stored.
     *                             Will be copied into m_Data.
     */
     DceUniDimensionalPointerArray(
        _In_ _Const_ const xpf::SharedPointer<xpf::Vector<DceUniquePointer<Type>, DceAllocator>, DceAllocator>& Elements
     ) noexcept(true) : DceSerializableObject(),
                        m_Data{Elements}
     {
         XPF_NOTHING();
     }

    /**
     * @brief  Default destructor.
     */
    virtual ~DceUniDimensionalPointerArray(void) noexcept(true) = default;

    /**
     * @brief  Copy and Move are defaulted.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(DceUniDimensionalPointerArray, default);

     /**
      * @brief          This method takes care of serializing the array in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled into.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Marshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) const noexcept(true) override
     {
         xpf::SharedPointer<xpf::Vector<DceRawPointer, DceAllocator>, DceAllocator> referentArray;
         NTSTATUS status = STATUS_UNSUCCESSFUL;

         /* Sanity check that the data is not empty. */
         if (this->m_Data.IsEmpty())
         {
             return STATUS_NO_DATA_DETECTED;
         }

         /* First we'll construct a referent array. This is just the addresses. */
         referentArray = xpf::MakeShared<xpf::Vector<DceRawPointer, DceAllocator>, DceAllocator>();
         if (referentArray.IsEmpty())
         {
             return STATUS_INSUFFICIENT_RESOURCES;
         }

         const auto& elements = this->Data();
         for (size_t i = 0; i < elements.Size(); ++i)
         {
             /* We don't support null embedded pointers for serialization. */
             if (!elements[i].Data())
             {
                 return STATUS_INVALID_ADDRESS;
             }

             DceRawPointer referent = elements[i].Data();
             status = (*referentArray).Emplace(referent);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         /* Now we serialize the referent array. */
         DceUniDimensionalArray<DceRawPointer, ArrayType> dceReferentArray{ referentArray };
         if (dceReferentArray.Data().Size() != (*referentArray).Size())
         {
             return STATUS_INSUFFICIENT_RESOURCES;
         }

         status = dceReferentArray.Marshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* And now the elements. */
         for (size_t i = 0; i < elements.Size(); ++i)
         {
             status = elements[i].Marshall(Stream, LrpcTransferSyntax);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         /* All good. */
         return status;
     }

     /**
      * @brief          This method takes care of deserializing the object in DCE-NDR format.
      *
      * @param[in,out]  Stream - where the data will be marshalled from.
      *
      * @param[in]      LrpcTransferSyntax - One of the LRPC_TRANSFER_SYNTAX_* flags.
      *
      * @return         A proper NTSTATUS error code.
      *
      */
     _Must_inspect_result_
     inline NTSTATUS XPF_API
     Unmarshall(
         _Inout_ AlpcRpc::DceNdr::RwStream& Stream,
         _In_ uint32_t LrpcTransferSyntax
     ) noexcept(true) override
     {
         NTSTATUS status = STATUS_UNSUCCESSFUL;
         DceUniDimensionalArray<DceRawPointer, ArrayType> dceReferentArray;

         /* Clear the underlying data. */
         this->m_Data = xpf::MakeShared<xpf::Vector<DceUniquePointer<Type>, DceAllocator>, DceAllocator>();
         if (this->m_Data.IsEmpty())
         {
             return STATUS_INSUFFICIENT_RESOURCES;
         }

         /* First we'll deserialize the referents. */
         status = dceReferentArray.Unmarshall(Stream, LrpcTransferSyntax);
         if (!NT_SUCCESS(status))
         {
             return status;
         }

         /* Now the elements. */
         const xpf::Vector<DceRawPointer, DceAllocator>& referentArray = dceReferentArray.Data();
         for (size_t i = 0; i < referentArray.Size(); ++i)
         {
             Type element{};
             xpf::SharedPointer<Type, DceAllocator> ptrElement;

             if (referentArray[i].Data() != nullptr)
             {
                 /* Unmarshall one element. */
                 status = element.Unmarshall(Stream, LrpcTransferSyntax);
                 if (!NT_SUCCESS(status))
                 {
                     return status;
                 }

                 /* Now we'll transform into a pointer. */
                 ptrElement = xpf::MakeShared<Type, DceAllocator>(xpf::Move(element));
                 if (ptrElement.IsEmpty())
                 {
                     return STATUS_INSUFFICIENT_RESOURCES;
                 }
             }

             /* Now append the element. */
             status = this->m_Data.Get()->Emplace(ptrElement);
             if (!NT_SUCCESS(status))
             {
                 return status;
             }
         }

         /* All good. */
         return status;
     }

     /**
      * @brief      Getter for the underlying data type.
      *
      * @return     A const reference to the data object.
      */
     inline const
     xpf::Vector<DceUniquePointer<Type>, DceAllocator>& XPF_API
     Data(
         void
     ) const noexcept(true)
     {
         return (*this->m_Data);
     }


 private:
     xpf::SharedPointer<xpf::Vector<DceUniquePointer<Type>,
                                    DceAllocator>,
                        DceAllocator> m_Data;
};  // class DceUniDimensionalPointerArray

/**
 * @brief   Ease of use for uni-dimensional conformant arrays.
 */
template <class Type>
using DceConformantArray = DceUniDimensionalArray<Type,
                                                  DceUniDimensionalArrayType::kConformant>;
/**
 * @brief   Ease of use for uni-dimensional conformant pointer arrays.
 */
template <class Type>
using DceConformantPointerArray = DceUniDimensionalPointerArray<Type,
                                                                DceUniDimensionalArrayType::kConformant>;
/**
 * @brief   Ease of use for uni-dimensional varying arrays.
 */
template <class Type>
using DceVaryingArray = DceUniDimensionalArray<Type,
                                               DceUniDimensionalArrayType::kVarying>;
/**
 * @brief   Ease of use for uni-dimensional varying pointer arrays.
 */
template <class Type>
using DceVaryingPointerArray = DceUniDimensionalPointerArray<Type,
                                                             DceUniDimensionalArrayType::kVarying>;
/**
 * @brief   Ease of use for uni-dimensional conformant and varying arrays.
 */
template <class Type>
using DceConformantVaryingArray = DceUniDimensionalArray<Type,
                                                         DceUniDimensionalArrayType::kConformantVarying>;
/**
 * @brief   Ease of use for uni-dimensional conformant and varying pointer arrays.
 */
template <class Type>
using DceConformantVaryingPointerArray = DceUniDimensionalPointerArray<Type,
                                                                       DceUniDimensionalArrayType::kConformantVarying>;
/**
 * @brief   Ease of use for wchar_t* types.
 */
using DceNdrWstring = DceConformantVaryingArray<DcePrimitiveType<wchar_t>>;

};  // namespace DceNdr
};  // namespace AlpcRpc
