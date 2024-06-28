/**
 * @file        ALPC-Tools/AlpcMon_Sys/ImageFilter.cpp
 *
 * @brief       In this file we define the functionality related to
 *              image filtering.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"
#include "KmHelper.hpp"
#include "Events.hpp"
#include "HashUtils.hpp"
#include "globals.hpp"

#include "ModuleCollector.hpp"
#include "ProcessCollector.hpp"

#include "ImageFilter.hpp"
#include "trace.hpp"

 /**
  * @brief   Put everything below here in paged section.
  */
XPF_SECTION_PAGED;

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Structure definition area                                                          |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 *  @brief      Supported from Windows 10, version 1709. The PsSetLoadImageNotifyRoutineEx routine
 *              registers a driver-supplied callback that is subsequently notified whenever an image
 *              (for example, a DLL or EXE) is loaded (or mapped into memory)..
 *
 *  @param[in]  NotifyRoutine   - A pointer to the caller-implemented PLOAD_IMAGE_NOTIFY_ROUTINE
 *                                callback routine for load-image notifications.
 *
 *  @param[in]  Flags           - Supplies a bitmask of flags that control the callback function.
 *                                PS_IMAGE_NOTIFY_CONFLICTING_ARCHITECTURE indicates that the callback
 *                                routine should be invoked for all potentially executable images,
 *                                including images that have a different architecture from the native
 *                                architecture of the operating system.
 *
 * @return      An ntstatus error code.
 *
 * @note        https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetloadimagenotifyroutineex
 */
typedef NTSTATUS(NTAPI* PFUNC_PsSetLoadImageNotifyRoutineEx)(
    _In_ PLOAD_IMAGE_NOTIFY_ROUTINE NotifyRoutine,
    _In_ ULONG_PTR Flags
) noexcept(true);

/**
 * @brief   Callback routine should be invoked for all potentially executable images.
 *          Including images that have a different architecture from the native architecture of the operating system.
 */
#define FLAGS_PS_IMAGE_NOTIFY_CONFLICTING_ARCHITECTURE            0x1

/**
 * @brief   A global variable that will store the address of PsSetLoadImageNotifyRoutineEx
 *          or NULL if we fail to find it. The resolution is attempted during ImageFilterStart.
 *          After we unregister the routine (ImageFilterStop), this is set back to NULL.
 */
static volatile PFUNC_PsSetLoadImageNotifyRoutineEx gApiPsSetLoadImageNotifyRoutineEx = nullptr;

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Forwrd definition of methods private to this module only.                          |
/// |                              Skipped by doxygen as they are annotated below.                                    |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

#ifndef DOXYGEN_SHOULD_SKIP_THIS

_Function_class_(PLOAD_IMAGE_NOTIFY_ROUTINE)
static void NTAPI
ImageFilterImageLoadNotifyRoutineCallback(
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_ HANDLE ProcessId,
    _In_ PIMAGE_INFO ImageInfo
) noexcept(true);

#endif  // DOXYGEN_SHOULD_SKIP_THIS

///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                              Helper methods private to this module only.                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///

/**
 *  @brief      Called by the operating system to notify the driver when a driver image or a user image
 *              (for example, a DLL or EXE) is mapped into virtual memory. The operating system invokes
 *              this routine after an image has been mapped to memory, but before its entrypoint is called.
 *
 *  @param[in]  FullImageName       - A pointer to a buffered Unicode string that identifies the executable image file.
 *                                    (The FullImageName parameter can be NULL in cases in which the operating system
 *                                    is unable to obtain the full name of the image at process creation time.).
 *
 *  @param[in]  ProcessId           - The process ID of the process in which the image has been mapped,
 *                                    but this handle is zero if the newly loaded image is a driver.
 *
 *  @param[in]  ImageInfo           - If the ExtendedInfoPresent flag is set in the IMAGE_INFO structure,
 *                                    the information is part of a larger, extended version of the image
 *                                    information structure, IMAGE_INFO_EX.
 *
 * @return      void.
 *
 * @note        https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pload_image_notify_routine
 */
_Use_decl_annotations_
static void NTAPI
ImageFilterImageLoadNotifyRoutineCallback(
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_ HANDLE ProcessId,
    _In_ PIMAGE_INFO ImageInfo
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nc-ntddk-pload_image_notify_routine
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(NULL != ImageInfo);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    bool isKernelImage = false;
    xpf::String<wchar_t> fullImagePath{ SYSMON_PAGED_ALLOCATOR };
    xpf::UniquePointer<xpf::IEvent> broadcastEvent{ SYSMON_PAGED_ALLOCATOR };

    PIMAGE_INFO_EX imageInfoExtended = nullptr;

    //
    // Extended info flag must always be present from Vista+.
    // So assert here and bail early.
    //
    if (0 == ImageInfo->ExtendedInfoPresent)
    {
        XPF_ASSERT(false);
        return;
    }
    imageInfoExtended = CONTAINING_RECORD(ImageInfo, IMAGE_INFO_EX, ImageInfo);

    //
    // Use the file object to retrieve the image path. FullImageName migh be partial.
    //
    status = KmHelper::File::QueryFileNameFromObject(imageInfoExtended->FileObject,
                                                     fullImagePath);
    if (!NT_SUCCESS(status))
    {
        SysMonLogWarning("QueryFileNameFromObject failed with %!STATUS!",
                         status);
        return;
    }

    //
    // Log for trace.
    //
    SysMonLogInfo("Image loaded in pid %d - %wZ (%S)",
                  HandleToUlong(ProcessId),
                  FullImageName,
                  fullImagePath.View().Buffer());

    //
    // If process id is 0, this is a kernel image.
    //
    isKernelImage = (NULL == ProcessId) ? true
                                        : false;
    if (isKernelImage)
    {
        //
        // System process has PID 4 - always. So we'll dispatch
        // the event with pid = 4 as it belongs in system process.
        //
        ProcessId = UlongToHandle(4);
    }

    //
    // Cache the new module.
    //
    // Q - should process collector and module collector register to image load event instead?
    //     this way we'll decouple the filter from the collector.
    //
    ModuleCollectorHandleNewModule(fullImagePath.View());
    ProcessCollectorHandleLoadModule(HandleToUlong(ProcessId),
                                     fullImagePath.View(),
                                     ImageInfo->ImageBase,
                                     ImageInfo->ImageSize);
    //
    // Create the event.
    //
    status = SysMon::ImageLoadEvent::Create(broadcastEvent,
                                            HandleToUlong(ProcessId),
                                            fullImagePath.View(),
                                            isKernelImage,
                                            ImageInfo->ImageBase,
                                            ImageInfo->ImageSize);
    if (!NT_SUCCESS(status))
    {
        SysMonLogWarning("Could not allocate an image load event %!STATUS!",
                         status);
        return;
    }

    //
    // Dispatch it.
    //
    status = GlobalDataGetBusInstance()->Dispatch(broadcastEvent.Get());
    if (!NT_SUCCESS(status))
    {
        SysMonLogWarning("Could not dispatch image loaded event %!STATUS!",
                         status);
        return;
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       ImageFilterStart                                                          |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ImageFilterStart(
    void
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetloadimagenotifyroutine
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Registering image load notification routine...");

    //
    // First we check if we can use the newer method.
    //
    gApiPsSetLoadImageNotifyRoutineEx = static_cast<PFUNC_PsSetLoadImageNotifyRoutineEx>(
                                        KmHelper::WrapperMmGetSystemRoutine(L"PsSetLoadImageNotifyRoutineEx"));
    if (gApiPsSetLoadImageNotifyRoutineEx)
    {
        SysMonLogInfo("PsSetLoadImageNotifyRoutineEx found at %p.",
                      gApiPsSetLoadImageNotifyRoutineEx);

        status = gApiPsSetLoadImageNotifyRoutineEx((PLOAD_IMAGE_NOTIFY_ROUTINE)ImageFilterImageLoadNotifyRoutineCallback,
                                                   FLAGS_PS_IMAGE_NOTIFY_CONFLICTING_ARCHITECTURE);
    }
    else
    {
        SysMonLogInfo("PsSetLoadImageNotifyRoutineEx not found! Will use the older variant.");

        status = ::PsSetLoadImageNotifyRoutine((PLOAD_IMAGE_NOTIFY_ROUTINE)ImageFilterImageLoadNotifyRoutineCallback);
    }

    //
    // Let's inspect the result.
    //
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Registering image load notify routine failed with status = %!STATUS!",
                       status);
        return status;
    }

    //
    // All good.
    //
    SysMonLogInfo("Successfully registered image load notification routine!");
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       ImageFilterStop                                                           |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ImageFilterStop(
    void
) noexcept(true)
{
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetloadimagenotifyroutine
    // The routine can be called only at PASSIVE_LEVEL.
    //
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Unregistering image load notification routine...");

    status = ::PsRemoveLoadImageNotifyRoutine((PLOAD_IMAGE_NOTIFY_ROUTINE)ImageFilterImageLoadNotifyRoutineCallback);
    gApiPsSetLoadImageNotifyRoutineEx = nullptr;

    //
    // We don't expect a failure. So assert here and investigate what happened.
    //
    if (!NT_SUCCESS(status))
    {
        XPF_ASSERT(false);

        SysMonLogCritical("Unregistering image load notification routine failed with status = %!STATUS!",
                          status);
        return;
    }

    //
    // All good.
    //
    SysMonLogInfo("Successfully unregistered image load notification routine!");
}
