/**
 * @file        ALPC-Tools/AlpcMon_Sys/main.cpp
 *
 * @brief       Entry point of the monitoring solution.
 *              It will inject a DLL which will hook related Alpc* APIs.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"
#include "CppSupport.hpp"
#include "globals.hpp"
#include "ProcessFilter.hpp"
#include "ThreadFilter.hpp"
#include "ImageFilter.hpp"
#include "FirmwareTableHandlerFilter.hpp"
#include "ModuleCollector.hpp"

#include "trace.hpp"


/**
 * @brief   All the code from below will go into the paged section.
 */
XPF_SECTION_PAGED;

/**
 * @brief   Needs to be C-like code to avoid name mangling.
 */
XPF_EXTERN_C_START();

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                Forward Declarations Section                                                     |
// |                       Skipped by doxygen as they are annotated below                                            |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

#ifndef DOXYGEN_SHOULD_SKIP_THIS

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS NTAPI
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID NTAPI
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
);

#endif  // DOXYGEN_SHOULD_SKIP_THIS

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Driver Unload Section                                                     |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

/**
 * @brief       The actual definition of the driver unload routine.
 *              This is responsible with the cleanup.
 *
 * @param[in]   DriverObject    - Describes our current driver.
 *
 * @return      void.
 */
_Use_decl_annotations_
VOID NTAPI
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    //
    // A driver's Unload routine executes in a system thread context at IRQL = PASSIVE_LEVEL.
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-driver_unload
    //
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(NULL != DriverObject);
    SysMonLogInfo("Unloading driver...");

    //
    // Unregister the firmware table handler routine.
    //
    FirmwareTableHandlerFilterStop(DriverObject);

    //
    // Stop the filters.
    //
    ImageFilterStop();
    ThreadFilterStop();
    ProcessFilterStop();

    //
    // Destroy the collectors.
    //
    ModuleCollectorDestroy();

    //
    // Destroy the globals.
    //
    GlobalDataDestroy();

    //
    // And lastly, deinitialize the cpp support. This will free all static global data.
    //
    XpfDeinitializeCppSupport();

    //
    // Clean WPP support.
    //
    SysMonLogInfo("Driver unloaded.");
    WPP_CLEANUP(DriverObject);
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       Driver Entry Section                                                      |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

/**
 * @brief       The actual definition of the driver unload routine.
 *              This is responsible with the cleanup.
 *
 * @param[in]   DriverObject    - Describes our current driver.
 * @param[in]   RegistryPath    - A pointer to a counted Unicode string specifying
 *                                the path to the driver's registry key.
 *
 * @return      A proper NTSTATUS error code.
 */
_Use_decl_annotations_
NTSTATUS NTAPI
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    //
    // DriverEntry routines are called in the context of a system thread at IRQL = PASSIVE_LEVEL.
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/writing-a-driverentry-routine
    //
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(NULL != DriverObject);
    XPF_DEATH_ON_FAILURE(NULL != RegistryPath);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    BOOLEAN isCppSupportInitialized = FALSE;
    BOOLEAN isGlobalDataCreated = FALSE;

    BOOLEAN isModuleCollectorCreated = FALSE;

    BOOLEAN isProcessFilteringStarted = FALSE;
    BOOLEAN isThreadFilteringStarted = FALSE;
    BOOLEAN isImageFilteringStarted = FALSE;

    BOOLEAN isFirmareTableHandleFilterRoutineRegistered = FALSE;

    //
    // POOL_NX_OPTIN allows drivers to dynamically opt-in to making non-paged pool allocations
    // non-executable by default based on whether or not this is supported by the version of the operating system.
    //
    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    SysMonLogInfo("Driver loading...");

    //
    // We want our driver to be unloadable. So we have to set an unload routine.
    //
    DriverObject->DriverUnload = DriverUnload;

    //
    // Prepare the cpp support.
    // This will call the destructor for all global data.
    //
    status = XpfInitializeCppSupport();
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to initialize cpp support %!STATUS!",
                       status);
        goto CleanUp;
    }
    isCppSupportInitialized = TRUE;

    //
    // Now the globals - must be done after we have cpp support.
    //
    status = GlobalDataCreate(RegistryPath, DriverObject);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to create global data %!STATUS!",
                       status);
        goto CleanUp;
    }
    isGlobalDataCreated = TRUE;

    //
    // Now the collectors.
    //
    status = ModuleCollectorCreate();
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to create the module collector %!STATUS!",
                       status);
        goto CleanUp;
    }
    isModuleCollectorCreated = TRUE;

    //
    // Now start the process filter.
    //
    status = ProcessFilterStart();
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to start process filtering %!STATUS!",
                       status);
        goto CleanUp;
    }
    isProcessFilteringStarted = TRUE;

    //
    // Then the thread filter.
    //
    status = ThreadFilterStart();
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to start thread filtering %!STATUS!",
                       status);
        goto CleanUp;
    }
    isThreadFilteringStarted = TRUE;

    //
    // Now image filter.
    //
    status = ImageFilterStart();
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to start image filtering %!STATUS!",
                       status);
        goto CleanUp;
    }
    isImageFilteringStarted = TRUE;

    //
    // Now the firmware table handler routine.
    //
    status = FirmwareTableHandlerFilterStart(DriverObject);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to register firmware table handler routine%!STATUS!",
                       status);
        goto CleanUp;
    }
    isFirmareTableHandleFilterRoutineRegistered = TRUE;

    //
    // All good :)
    //
    status = STATUS_SUCCESS;
    SysMonLogInfo("Driver loaded!");

CleanUp:
    //
    // On fail, be a good citizen and rollback everything.
    //
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to load driver with %!STATUS!. Commencing rollback!",
                       status);

        if (FALSE != isFirmareTableHandleFilterRoutineRegistered)
        {
            FirmwareTableHandlerFilterStop(DriverObject);
            isFirmareTableHandleFilterRoutineRegistered = TRUE;
        }

        if (FALSE != isImageFilteringStarted)
        {
            ImageFilterStop();
            isImageFilteringStarted = FALSE;
        }

        if (FALSE != isThreadFilteringStarted)
        {
            ThreadFilterStop();
            isThreadFilteringStarted = FALSE;
        }

        if (FALSE != isProcessFilteringStarted)
        {
            ProcessFilterStop();
            isProcessFilteringStarted = FALSE;
        }

        if (FALSE != isModuleCollectorCreated)
        {
            ModuleCollectorDestroy();
            isModuleCollectorCreated = FALSE;
        }

        if (FALSE != isGlobalDataCreated)
        {
            GlobalDataDestroy();
            isGlobalDataCreated = FALSE;
        }

        if (FALSE != isCppSupportInitialized)
        {
            XpfDeinitializeCppSupport();
            isCppSupportInitialized = FALSE;
        }

        SysMonLogError("Rollback completed! Will not load driver!");
        WPP_CLEANUP(DriverObject);
    }
    return status;
}

/**
 * @brief End of the name mangling shenanignas.
 */
XPF_EXTERN_C_END();
