/**
 * @file        ALPC-Tools/AlpcMon_Sys/CppSupport.hpp
 *
 * @brief       Source file with routines to control cpp support in km.
 * 
 * @details     This file contains the definition of init/deinit routines
 *              for cpp support to function properly in KM.
 *              Please see http://www.osronline.com/article.cfm%5earticle=57.htm
 *              for more details.
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
#include "trace.hpp"

/**
 * @brief CPP support was not yet initialized.
 *        ensure these APIs are not name mangled
 *        so the KM compiler will recognize them.
 *
 */
XPF_EXTERN_C_START();

/**
 * @brief   By default everything goes into paged object section.
 *          The code here is expected to be run only at passive level.
 *          It is safe to place everything in paged area. 
 */
XPF_SECTION_PAGED;


//
// ************************************************************************************************
// This is the section of structures and data definitions.
// ************************************************************************************************
//


/**
 * @brief   Definition for a pointer to a function with no arguments
 *          and no return value. This is similar with what crt does
 *          and it shouldn't be changed.
 * 
 * 
 * @return void - This function has no return value.
 */
typedef void (XPF_PLATFORM_CONVENTION* PVFV) (void);

/**
 * @brief For static global objects, we keep a list of destructors.
 *        This list is global and will be kept non paged.
 *        They will be constructed in Driver-Entry and destructed in Driver-Unload.
 */
typedef struct _XPF_CPP_DESTRUCTOR
{
    LIST_ENTRY Entry;
    PVFV Destructor;
} XPF_CPP_DESTRUCTOR;

/**
 * @brief This holds the global list of destructors.
 *        They will be enqueued internally by calling _atexit_
 */
static XPF_CPP_DESTRUCTOR gXpfCppDestructorList;


//
// ************************************************************************************************
// This is the section of properly setting .CRT sections.
// ************************************************************************************************
//

/**
 * @brief   First we restore the default sections.
 *          We need to introduce new sections required by .crt initialization.
 *          So the first thing we do is to restore to default sections.
 */
XPF_SECTION_DEFAULT;

/**
 * @brief   MSVC requires these sections to be defined before usage.
 */
#if defined XPF_COMPILER_MSVC
    #pragma section(".CRT$XCA", read)
    #pragma section(".CRT$XCZ", read)
#endif  // XPF_COMPILER_MSVC

/**
 * @brief These data sections are required by CRT.
 *        This is where pointers of constructors of static objects are saved.
 *        .CRT$XCA is the marker for the beginning of the location.
 *
 */
XPF_ALLOC_SECTION(".CRT$XCA") PVFV __crtXca[] = { nullptr };

/**
 * @brief These data sections are required by CRT.
 *        This is where pointers of constructors of static objects are saved.
 *        .CRT$XCZ is the marker for the end of the location.
 *
 */
XPF_ALLOC_SECTION(".CRT$XCZ") PVFV __crtXcz[] = { nullptr };


/**
 * @brief   MSVC requires these sections to be merged with the .rdata section.
 *          Otherwise a linker error will occur sayint that there are potentially
 *          unhandled static initializers caused by the .CRT sections being present
 *          in the binary. We need them to be merged.
 */
#if defined XPF_COMPILER_MSVC
    #pragma data_seg()
    #pragma comment(linker, "/merge:.CRT=.rdata")
#endif  // XPF_COMPILER_MSVC


/**
 * @brief   By default everything goes into paged object section.
 *          The code here is expected to be run only at passive level.
 *          It is safe to place everything in paged area. 
 */
XPF_SECTION_PAGED;


//
// ************************************************************************************************
// This is the section containing implementation of internal functions.
// ************************************************************************************************
//


/**
 * @brief It takes as input a PVFV function pointer.
 *        It then allocates storage for the pointer and adds
 *        the pointer to a LIFO list of terminator functions.
 *        When the linkage unit in question terminates, the LIFO list is run,
 *        calling through each stored function pointer in turn.
 *
 *
 * @param[in] Destructor - to be registered.
 *
 *
 * @return 0 if successful, or a nonzero value if an error occurs.
 *
 */
int
XPF_PLATFORM_CONVENTION
atexit(
    _In_ PVFV Destructor
)
{
    XPF_CPP_DESTRUCTOR* destructorEntry = nullptr;

    //
    // We should always be called at passive.
    //
    XPF_MAX_PASSIVE_LEVEL();

    SysMonLogInfo("Registering atexit for destructor %p",
                  Destructor);

    //
    // Attempt to allocate space for a destructor.
    // Try to allocate from nonpaged pool - so it shouldn't fail - mark the allocation as critical.
    // If nonpagedpool memory is depleted we have bigger problems.
    //
    // On fail we'll simply not call the destructor so we'll have a leak.
    //
    destructorEntry = static_cast<XPF_CPP_DESTRUCTOR*>(
                      xpf::CriticalMemoryAllocator::AllocateMemory(sizeof(XPF_CPP_DESTRUCTOR)));

    //
    // We didn't manage to allocate resources for the destructor.
    // Shouldn't happen under normal circumstances.
    //
    if (nullptr == destructorEntry)
    {
        SysMonLogCritical("Failed to allocate resources for destructor %p. This is critical!",
                          Destructor);

        XPF_DEATH_ON_FAILURE(nullptr != destructorEntry);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Preinitialize the memory.
    //
    xpf::ApiZeroMemory(destructorEntry,
                       sizeof(*destructorEntry));
    ::InitializeListHead(&destructorEntry->Entry);

    //
    // Now let's continue by inserting the destructor in the list.
    //
    destructorEntry->Destructor = Destructor;
    ::InsertTailList(&gXpfCppDestructorList.Entry,
                     &destructorEntry->Entry);

    //
    // All good. The destructor will be called during deinitialization.
    //
    SysMonLogInfo("Successfully registered atexit for destructor %p",
                  Destructor);
    return STATUS_SUCCESS;
}


//
// ************************************************************************************************
// This is the section containing implementation of public functions.
// ************************************************************************************************
//


_Use_decl_annotations_
NTSTATUS
XPF_API
XpfInitializeCppSupport(
    void
)
{
    //
    // We should always be called at passive.
    //
    XPF_MAX_PASSIVE_LEVEL();

    SysMonLogInfo("Initializing cpp support...");

    //
    // First we initialize the global destructors list.
    // We need to ensure this is a valid list.
    //
    xpf::ApiZeroMemory(&gXpfCppDestructorList,
                       sizeof(gXpfCppDestructorList));
    ::InitializeListHead(&gXpfCppDestructorList.Entry);

    //
    // Sanity check that __crtXca and __crtXcz are somewhat valid.
    // We can't initialize cpp support whithout them.
    //
    // In this scenario something went wrong with the build.
    // Check the compile options and the above declaration.
    //
    if ((nullptr == __crtXca) || (nullptr == __crtXcz) ||
        xpf::AlgoPointerToValue(__crtXca) >= xpf::AlgoPointerToValue(__crtXcz))
    {
        SysMonLogCritical("Invalid __crtXCa (%p) and __crtXcz (%p). This is critical!",
                          __crtXca,
                          __crtXcz);

        XPF_DEATH_ON_FAILURE(false);
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Now we also construct our global static objects.
    // __crtXca is the start section and __crtXcz the end section.
    //
    for (PVFV* currentEntry = __crtXca; currentEntry != __crtXcz; ++currentEntry)
    {
        PVFV constructor = (*currentEntry);
        if ((nullptr != constructor) && (nullptr != (*constructor)))
        {
            SysMonLogInfo("Found constructor for static global data at %p. Will execute!",
                          constructor);

            (*constructor)();
        }
    }

    //
    // Everything went ok.
    //
    SysMonLogInfo("Successfully initialized cpp support!");
    return STATUS_SUCCESS;
}

void
XPF_API
XpfDeinitializeCppSupport(
    void
)
{
    XPF_CPP_DESTRUCTOR* destructorEntry = nullptr;
    PLIST_ENTRY listEntry = nullptr;

    //
    // We should always be called at passive.
    //
    XPF_MAX_PASSIVE_LEVEL();

    SysMonLogInfo("Deinitializing cpp support...");


    //
    // Now let's free the resources.
    // The MSDN doc for _atexit specifies that the registered functions
    // are executed in a LIFO (last-in, first-out) manner.
    // We inserted at tail, so we pop at tail.
    //
    while (FALSE == ::IsListEmpty(&gXpfCppDestructorList.Entry))
    {
        //
        // Pop last element.
        //
        listEntry = ::RemoveTailList(&gXpfCppDestructorList.Entry);
        if (nullptr == listEntry)
        {
            SysMonLogCritical("Invalid list entry %p. This is critical - logic error somewhere!",
                              listEntry);

            XPF_DEATH_ON_FAILURE(nullptr != listEntry);
            continue;
        }

        //
        // Get the XPF_CPP_DESTRUCTOR structure from list entry element.
        //
        destructorEntry = XPF_CONTAINING_RECORD(listEntry, XPF_CPP_DESTRUCTOR, Entry);
        if (nullptr == destructorEntry)
        {
            SysMonLogCritical("Invalid destructor entry %p. This is critical - logic error somewhere!",
                              destructorEntry);

            XPF_DEATH_ON_FAILURE(nullptr != destructorEntry);
            continue;
        }

        //
        // Now we call the registered destructor
        //
        if (nullptr != destructorEntry->Destructor)
        {
            SysMonLogInfo("Found destructor for static global data at %p. Will execute!",
                          destructorEntry->Destructor);
            destructorEntry->Destructor();
        }

        //
        // Invalidate the destructor memory.
        //
        xpf::ApiZeroMemory(destructorEntry,
                           sizeof(*destructorEntry));
        ::InitializeListHead(&destructorEntry->Entry);

        //
        // And finally clean up allocated resources.
        //
        xpf::CriticalMemoryAllocator::FreeMemory(destructorEntry);
        destructorEntry = nullptr;
    }

    //
    // Leave the list in a known - neutral state.
    //
    xpf::ApiZeroMemory(&gXpfCppDestructorList,
                       sizeof(gXpfCppDestructorList));
    ::InitializeListHead(&gXpfCppDestructorList.Entry);

    SysMonLogInfo("Successfully deinitialized cpp support!");
}


/**
 * @brief CPP support is initialized :)
 */
XPF_EXTERN_C_END();
