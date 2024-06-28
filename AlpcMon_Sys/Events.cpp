/**
 * @file        ALPC-Tools/AlpcMon_Sys/Events.cpp
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

#include "precomp.hpp"

#include "Events.hpp"
#include "trace.hpp"

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       PAGED SECTION AREA                                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


 /**
  * @brief   Put everything below here in paged section.
  */
XPF_SECTION_PAGED;


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Process Create Event                                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::ProcessCreateEvent::Create(
    _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
    _In_ uint32_t ProcessPid,
    _In_ const SysMon::ProcessArchitecture& ProcessArchitecture,
    _In_ _Const_ const xpf::StringView<wchar_t> ProcessPath
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::UniquePointer<SysMon::ProcessCreateEvent> eventInstance;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // First we create the event instance.
    //
    eventInstance = xpf::MakeUniqueWithAllocator<SysMon::ProcessCreateEvent>(SYSMON_PAGED_ALLOCATOR);
    if (eventInstance.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::ProcessCreateEvent& eventInstanceReference = (*eventInstance);

    //
    // Now we fill the members.
    //
    status = eventInstanceReference.m_ProcessPath.Append(ProcessPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    eventInstanceReference.m_ProcessPid = ProcessPid;
    eventInstanceReference.m_ProcessArchitecture = ProcessArchitecture;

    //
    // And finally cast to generic event.
    //
    Event = xpf::DynamicUniquePointerCast<xpf::IEvent, SysMon::ProcessCreateEvent>(eventInstance);
    return (Event.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                             : STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Process Terminate Event                                                   |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::ProcessTerminateEvent::Create(
    _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
    _In_ uint32_t ProcessPid
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::UniquePointer<SysMon::ProcessTerminateEvent> eventInstance;

    //
    // First we create the event instance.
    //
    eventInstance = xpf::MakeUniqueWithAllocator<SysMon::ProcessTerminateEvent>(SYSMON_PAGED_ALLOCATOR);
    if (eventInstance.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::ProcessTerminateEvent& eventInstanceReference = (*eventInstance);

    //
    // Now we fill the members.
    //
    eventInstanceReference.m_ProcessPid = ProcessPid;

    //
    // And finally cast to generic event.
    //
    Event = xpf::DynamicUniquePointerCast<xpf::IEvent, SysMon::ProcessTerminateEvent>(eventInstance);
    return (Event.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                             : STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Image Load Event                                                          |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::ImageLoadEvent::Create(
    _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
    _In_ uint32_t ProcessPid,
    _In_ _Const_ const xpf::StringView<wchar_t>& ImagePath,
    _In_ bool IsKernelImage,
    _In_ void* ImageBase,
    _In_ size_t ImageSize
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::UniquePointer<SysMon::ImageLoadEvent> eventInstance;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // First we create the event instance.
    //
    eventInstance = xpf::MakeUniqueWithAllocator<SysMon::ImageLoadEvent>(SYSMON_PAGED_ALLOCATOR);
    if (eventInstance.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::ImageLoadEvent& eventInstanceReference = (*eventInstance);

    //
    // Now we fill the members.
    //
    status = eventInstanceReference.m_ImagePath.Append(ImagePath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    eventInstanceReference.m_ProcessPid = ProcessPid;
    eventInstanceReference.m_IsKernelImage = IsKernelImage;
    eventInstanceReference.m_ImageBase = ImageBase;
    eventInstanceReference.m_ImageSize = ImageSize;

    //
    // And finally cast to generic event.
    //
    Event = xpf::DynamicUniquePointerCast<xpf::IEvent, SysMon::ImageLoadEvent>(eventInstance);
    return (Event.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                             : STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Thread Create Event                                                       |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::ThreadCreateEvent::Create(
    _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
    _In_ uint32_t ProcessPid,
    _In_ uint32_t ThreadTid
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    xpf::UniquePointer<SysMon::ThreadCreateEvent> eventInstance;

    //
    // First we create the event instance.
    //
    eventInstance = xpf::MakeUniqueWithAllocator<SysMon::ThreadCreateEvent>(SYSMON_PAGED_ALLOCATOR);
    if (eventInstance.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::ThreadCreateEvent& eventInstanceReference = (*eventInstance);

    //
    // Now we fill the members.
    //

    eventInstanceReference.m_ProcessPid = ProcessPid;
    eventInstanceReference.m_ThreadTid = ThreadTid;

    //
    // And finally cast to generic event.
    //
    Event = xpf::DynamicUniquePointerCast<xpf::IEvent, SysMon::ThreadCreateEvent>(eventInstance);
    return (Event.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                             : STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Thread Terminate Event                                                    |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::ThreadTerminateEvent::Create(
    _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
    _In_ uint32_t ProcessPid,
    _In_ uint32_t ThreadTid
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    xpf::UniquePointer<SysMon::ThreadTerminateEvent> eventInstance;

    //
    // First we create the event instance.
    //
    eventInstance = xpf::MakeUniqueWithAllocator<SysMon::ThreadTerminateEvent>(SYSMON_PAGED_ALLOCATOR);
    if (eventInstance.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::ThreadTerminateEvent& eventInstanceReference = (*eventInstance);

    //
    // Now we fill the members.
    //

    eventInstanceReference.m_ProcessPid = ProcessPid;
    eventInstanceReference.m_ThreadTid = ThreadTid;

    //
    // And finally cast to generic event.
    //
    Event = xpf::DynamicUniquePointerCast<xpf::IEvent, SysMon::ThreadTerminateEvent>(eventInstance);
    return (Event.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                             : STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Um Hook Event                                                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::UmHookEvent::Create(
    _Inout_ xpf::UniquePointer<xpf::IEvent>& Event,
    _In_ void* UmHookMessage
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    xpf::UniquePointer<SysMon::UmHookEvent> eventInstance;

    //
    // First we create the event instance.
    //
    eventInstance = xpf::MakeUniqueWithAllocator<SysMon::UmHookEvent>(SYSMON_PAGED_ALLOCATOR);
    if (eventInstance.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    SysMon::UmHookEvent& eventInstanceReference = (*eventInstance);

    //
    // Now we fill the members.
    //

    eventInstanceReference.m_Message = UmHookMessage;

    //
    // And finally cast to generic event.
    //
    Event = xpf::DynamicUniquePointerCast<xpf::IEvent, SysMon::UmHookEvent>(eventInstance);
    return (Event.IsEmpty()) ? STATUS_INSUFFICIENT_RESOURCES
                             : STATUS_SUCCESS;
}
