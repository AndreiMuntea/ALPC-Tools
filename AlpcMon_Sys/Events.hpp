/**
 * @file        ALPC-Tools/AlpcMon_Sys/Events.hpp
 *
 * @brief       This contains all events available throughout the driver.
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
 * @brief   This contains all available event ids.
 */
enum class EventId : xpf::EVENT_ID
{
    /**
     * @brief Sent when a process is created.
     */
    ProcessCreate = 0,

    /**
     * @brief Sent when a process is terminated.
     */
    ProcessTerminate = 1,

    /**
     * @brief Sent when an image is loaded in a process.
     */
    ImageLoad = 2,

    /**
     * @brief Sent when a thread is created.
     */
    ThreadCreate = 3,

    /**
     * @brief Sent when a thread is terminated.
     */
    ThreadTerminate = 4,

    /**
     * @brief Sent from an UM hook.
     */
     UmHookMessage = 5,

    /**
     * @brief A canary value so we can find how many
     *        events we have defined so far.
     *
     * @note  Do not add anything below this.
     */
    MAX
};  // enum class EventId

/**
 * @brief   This contains the possible architecture of a process.
 */
enum class ProcessArchitecture
{
    /**
     * @brief Process is running as x86.
     */
    x86 = 0,

    /**
     * @brief Process is running as x64.
     */
    x64 = 1,

    /**
     * @brief Process is running as WoW x86 process on x64.
     */
    WoWX86onX64 = 2,

    /**
     * @brief A canary value so we can find how many
     *        architectures we have defined so far.
     *
     * @note  Do not add anything below this.
     */
    MAX
};  // enum class ProcessArchitecture

/**
 * @brief The Process creation event.
 *        Sent when a process is created.
 */
class ProcessCreateEvent final : public xpf::IEvent
{
 protected:
    /**
     * @brief       Default constructor.
     *
     * @note        Please use Create method!
     */
     ProcessCreateEvent(void) noexcept(true) = default;

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~ProcessCreateEvent(void) noexcept(true) = default;

    /**
     * @brief   An event can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ProcessCreateEvent, delete);

    /**
     * @brief This method is used to retrieve the event id of an Event.
     *        This uniquely identifies the event.
     * 
     * @return An uniquely identifier for this type of event.
     */
    inline xpf::EVENT_ID XPF_API
    EventId (
        void
    ) const noexcept(true) override
    {
        return static_cast<xpf::EVENT_ID>(SysMon::EventId::ProcessCreate);
    }

    /**
     * @brief          This method is used to create the event
     *
     * @param[in,out]  Event                - Will contain the properly created event.
     *
     * @param[in]      ProcessPid           - The process Id.
     *
     * @param[in]      ProcessArchitecture  - The architecture of the process.
     *
     * @param[in]      ProcessPath          - The path of the process.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           This is intended to be called from process creation routine.
     *                 It inherits its irql requirements.
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
        _In_ uint32_t ProcessPid,
        _In_ const SysMon::ProcessArchitecture& ProcessArchitecture,
        _In_ _Const_ const xpf::StringView<wchar_t> ProcessPath
    ) noexcept(true);

    /**
     * @brief   Getter for the process id.
     *
     * @return  The id of the process which is created.
     */
    inline uint32_t XPF_API
    ProcessPid(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessPid;
    }

    /**
     * @brief   Getter for the process path.
     *
     * @return  The path of the process which is created.
     */
    inline const xpf::String<wchar_t>& XPF_API
    ProcessPath(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessPath;
    }

    /**
     * @brief   Getter for the process architecture.
     *
     * @return  The architecture of the process which is created.
     */
    inline const SysMon::ProcessArchitecture& XPF_API
    ProcessArchitecture(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessArchitecture;
    }

 private:
     uint32_t m_ProcessPid = 0;
     xpf::String<wchar_t> m_ProcessPath;
     SysMon::ProcessArchitecture m_ProcessArchitecture = SysMon::ProcessArchitecture::MAX;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class ProcessCreateEvent

/**
 * @brief The Process termination event.
 *        Sent when a process is terminated.
 */
class ProcessTerminateEvent final : public xpf::IEvent
{
 protected:
    /**
     * @brief       Default constructor.
     *
     * @note        Please use Create method!
     */
     ProcessTerminateEvent(void) noexcept(true) = default;

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~ProcessTerminateEvent(void) noexcept(true) = default;

    /**
     * @brief   An event can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ProcessTerminateEvent, delete);

    /**
     * @brief This method is used to retrieve the event id of an Event.
     *        This uniquely identifies the event.
     * 
     * @return An uniquely identifier for this type of event.
     */
    inline xpf::EVENT_ID XPF_API
    EventId (
        void
    ) const noexcept(true) override
    {
        return static_cast<xpf::EVENT_ID>(SysMon::EventId::ProcessTerminate);
    }

    /**
     * @brief          This method is used to create the event
     *
     * @param[in,out]  Event        - Will contain the properly created event.
     *
     * @param[in]      ProcessPid   - The process Id.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           This is intended to be called from process notification routine.
     *                 It inherits its irql requirements.
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
        _In_ uint32_t ProcessPid
    ) noexcept(true);

    /**
     * @brief   Getter for the process id.
     *
     * @return  The id of the process which is terminated.
     */
    inline uint32_t XPF_API
    ProcessPid(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessPid;
    }

 private:
     uint32_t m_ProcessPid = 0;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class ProcessTerminateEvent

/**
 * @brief The Image Load event.
 *        Sent when an image is loaded.
 */
class ImageLoadEvent final : public xpf::IEvent
{
 protected:
    /**
     * @brief       Default constructor.
     *
     * @note        Please use Create method!
     */
     ImageLoadEvent(void) noexcept(true) = default;

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~ImageLoadEvent(void) noexcept(true) = default;

    /**
     * @brief   An event can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ImageLoadEvent, delete);

    /**
     * @brief This method is used to retrieve the event id of an Event.
     *        This uniquely identifies the event.
     * 
     * @return An uniquely identifier for this type of event.
     */
    inline xpf::EVENT_ID XPF_API
    EventId (
        void
    ) const noexcept(true) override
    {
        return static_cast<xpf::EVENT_ID>(SysMon::EventId::ImageLoad);
    }

    /**
     * @brief          This method is used to create the event
     *
     * @param[in,out]  Event          - Will contain the properly created event.
     *
     * @param[in]      ProcessPid     - The process Id in which the image is loaded.
     *
     * @param[in]      ImagePath      - The path of the image.
     *
     * @param[in]      IsKernelImage  - A boolean specifying whether the loaded image is
     *                                  a kernel one or not.
     *
     * @param[in]      ImageBase      - The base of the image.
     *
     * @param[in]      ImageSize      - The size of the image.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           This is intended to be called from image load routine.
     *                 It inherits its irql requirements.
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
        _In_ uint32_t ProcessPid,
        _In_ _Const_ const xpf::StringView<wchar_t>& ImagePath,
        _In_ bool IsKernelImage,
        _In_ void* ImageBase,
        _In_ size_t ImageSize
    ) noexcept(true);

    /**
     * @brief   Getter for the process id.
     *
     * @return  The id of the process in which the image is loaded.
     */
    inline uint32_t XPF_API
    ProcessPid(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessPid;
    }

    /**
     * @brief   Getter for the image path.
     *
     * @return  The path of the image which is loaded.
     */
    inline const xpf::String<wchar_t>& XPF_API
    ImagePath(
        void
    ) const noexcept(true)
    {
        return this->m_ImagePath;
    }

    /**
     * @brief   Checks whether the image loaded is kernel or not.
     *
     * @return  true if loaded image is kernel, false otherwise.
     */
    inline bool XPF_API
    IsKernelImage(
        void
    ) const noexcept(true)
    {
        return this->m_IsKernelImage;
    }

    /**
     * @brief   Getter for the image base.
     *
     * @return  The base of the image which is loaded.
     */
    inline void* XPF_API
    ImageBase(
        void
    ) const noexcept(true)
    {
        return this->m_ImageBase;
    }

    /**
     * @brief   Getter for the image size.
     *
     * @return  The size of the image which is loaded.
     */
    inline size_t XPF_API
    ImageSize(
        void
    ) const noexcept(true)
    {
        return this->m_ImageSize;
    }

 private:
     uint32_t m_ProcessPid = 0;
     xpf::String<wchar_t> m_ImagePath;
     bool m_IsKernelImage = false;
     void* m_ImageBase = nullptr;
     size_t m_ImageSize = 0;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class ImageLoadEvent


/**
 * @brief The Thread creation event.
 *        Sent when a thread is created.
 */
class ThreadCreateEvent final : public xpf::IEvent
{
 protected:
    /**
     * @brief       Default constructor.
     *
     * @note        Please use Create method!
     */
     ThreadCreateEvent(void) noexcept(true) = default;

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~ThreadCreateEvent(void) noexcept(true) = default;

    /**
     * @brief   An event can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ThreadCreateEvent, delete);

    /**
     * @brief This method is used to retrieve the event id of an Event.
     *        This uniquely identifies the event.
     * 
     * @return An uniquely identifier for this type of event.
     */
    inline xpf::EVENT_ID XPF_API
    EventId (
        void
    ) const noexcept(true) override
    {
        return static_cast<xpf::EVENT_ID>(SysMon::EventId::ThreadCreate);
    }

    /**
     * @brief          This method is used to create the event
     *
     * @param[in,out]  Event        - Will contain the properly created event.
     *
     * @param[in]      ProcessPid   - The process Id.
     *
     * @param[in]      ThreadTid    - The thread Id.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           This is intended to be called from thread notification routine.
     *                 It inherits its irql requirements.
     */
    _IRQL_requires_max_(APC_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
        _In_ uint32_t ProcessPid,
        _In_ uint32_t ThreadTid
    ) noexcept(true);

    /**
     * @brief   Getter for the process id.
     *
     * @return  The id of the process in which the thread is created.
     */
    inline uint32_t XPF_API
    ProcessPid(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessPid;
    }

    /**
     * @brief   Getter for the thread id.
     *
     * @return  The id of the thread which is created.
     */
    inline uint32_t XPF_API
    ThreadTid(
        void
    ) const noexcept(true)
    {
        return this->m_ThreadTid;
    }

 private:
     uint32_t m_ProcessPid = 0;
     uint32_t m_ThreadTid = 0;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class ThreadCreateEvent

/**
 * @brief The Thread termination event.
 *        Sent when a thread is terminated.
 */
class ThreadTerminateEvent final : public xpf::IEvent
{
 protected:
    /**
     * @brief       Default constructor.
     *
     * @note        Please use Create method!
     */
     ThreadTerminateEvent(void) noexcept(true) = default;

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~ThreadTerminateEvent(void) noexcept(true) = default;

    /**
     * @brief   An event can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ThreadTerminateEvent, delete);

    /**
     * @brief This method is used to retrieve the event id of an Event.
     *        This uniquely identifies the event.
     * 
     * @return An uniquely identifier for this type of event.
     */
    inline xpf::EVENT_ID XPF_API
    EventId (
        void
    ) const noexcept(true) override
    {
        return static_cast<xpf::EVENT_ID>(SysMon::EventId::ThreadTerminate);
    }

    /**
     * @brief          This method is used to create the event
     *
     * @param[in,out]  Event        - Will contain the properly created event.
     *
     * @param[in]      ProcessPid   - The process Id.
     *
     * @param[in]      ThreadTid    - The thread Id.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           This is intended to be called from thread notification routine.
     *                 It inherits its irql requirements.
     */
    _IRQL_requires_max_(APC_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
        _In_ uint32_t ProcessPid,
        _In_ uint32_t ThreadTid
    ) noexcept(true);

    /**
     * @brief   Getter for the process id.
     *
     * @return  The id of the process in which the thread is terminated.
     */
    inline uint32_t XPF_API
    ProcessPid(
        void
    ) const noexcept(true)
    {
        return this->m_ProcessPid;
    }

    /**
     * @brief   Getter for the thread id.
     *
     * @return  The id of the thread which is terminated.
     */
    inline uint32_t XPF_API
    ThreadTid(
        void
    ) const noexcept(true)
    {
        return this->m_ThreadTid;
    }

 private:
     uint32_t m_ProcessPid = 0;
     uint32_t m_ThreadTid = 0;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class ThreadTerminateEvent

/**
 * @brief The um hook event.
 *        Sent with data from one of our UM hooks.
 */
class UmHookEvent final : public xpf::IEvent
{
 protected:
    /**
     * @brief       Default constructor.
     *
     * @note        Please use Create method!
     */
     UmHookEvent(void) noexcept(true) = default;

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~UmHookEvent(void) noexcept(true) = default;

    /**
     * @brief   An event can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::UmHookEvent, delete);

    /**
     * @brief This method is used to retrieve the event id of an Event.
     *        This uniquely identifies the event.
     * 
     * @return An uniquely identifier for this type of event.
     */
    inline xpf::EVENT_ID XPF_API
    EventId (
        void
    ) const noexcept(true) override
    {
        return static_cast<xpf::EVENT_ID>(SysMon::EventId::UmHookMessage);
    }

    /**
     * @brief          This method is used to create the event
     *
     * @param[in,out]  Event         - Will contain the properly created event.
     *
     * @param[in]      UmHookMessage - Contains the message with data.
     *                                 Can be safely casted to UM_KM_MESSAGE_HEADER.
     *                                 It is probed and captured - so there is no need
     *                                 to guard the buffer itself.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           This is intended to be called from um hooks.
     *                 It inherits its irql requirements.
     *
     * @note           This is valid only synchronously, afterwards the memory is disposed.
     *                 Caller responsibility to make any copies.
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
        _In_ void* UmHookMessage
    ) noexcept(true);

    /**
     * @brief   Getter for the um hook message.
     *
     * @return  The buffer received.
     */
    inline void* XPF_API
    Message(
        void
    ) const noexcept(true)
    {
        return this->m_Message;
    }

 private:
     void* m_Message = nullptr;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class UmHookEvent
};  // namespace SysMon
