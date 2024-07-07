/**
 * @file        ALPC-Tools/AlpcMon_Sys/ApcQueue.hpp
 *
 * @brief       In this file we define a wrapper over apc functionality in KM.
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
#include "globals.hpp"

namespace KmHelper
{

/**
 * @brief   Forward definition - declared below.
 */
class ApcQueue;

/**
 * @brief   We'll store a list of APCs but we also need some extra context for our queue.
 */
struct Apc
{
    /**
     * @brief   The apc object that is actually used to execute.
     *          Using this we'll get to the bigger Apc structure.
     */
    KAPC OriginalApc = { 0 };
    /**
     * @brief   Specifies whether the apc is a kernel or a user one.
     */
    KPROCESSOR_MODE Mode = KernelMode;
    /**
     * @brief   The queue in which the apc belongs.
     *          This is used to keep the driver loaded as long as it
     *          has outstanding apcs.
     */
    KmHelper::ApcQueue* ApcQueueObject = nullptr;
    /**
     * @brief   The apc routine to be executed.
     *          This is the one passed by caller.
     */
    PKNORMAL_ROUTINE OriginalNormalRoutine = nullptr;
    /**
     * @brief   The apc routine to be executed as cleanup.
     *          This is the one passed by caller.
     */
    PKNORMAL_ROUTINE OriginalCleanupRoutine = nullptr;
    /**
     * @brief   The original context to be passed to the
     *          apc routine. This is the orginal value
     *          passed by the caller.
     */
    PVOID OriginalNormalConext = nullptr;
    /**
     * @brief   The original argument 1 to be passed to the
     *          apc routine. This is the orginal value
     *          passed by the caller.
     */
    PVOID OriginalSystemArgument1 = nullptr;
    /**
     * @brief   The original argument 2 to be passed to the
     *          apc routine. This is the orginal value
     *          passed by the caller.
     */
    PVOID OriginalSystemArgument2 = nullptr;
};  // struct Apc

/**
 * @brief   In Windows KM we have a possibility to enqueue APCs to be executed at a later date.
 *          This is a convenience wrapper to enqueue APCs and avoid the driver from being ran down
 *          while they are executed.
 */
class ApcQueue final
{
 public:
     /**
      * @brief  Default constructor.
      */
     ApcQueue(void) noexcept(true) = default;

     /**
      * @brief  Default destructor.
      */
     ~ApcQueue(void) noexcept(true);

     /**
      * @brief  The copy and move semantics are deleted.
      *         We can implement them when needed.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(ApcQueue, delete);

     /**
      * @brief      Schedules an APC on the current thread.
      *
      * @param[in]  Mode            - specified whether the apc is a kernel or a user apc.
      * @param[in]  NormalRoutine   - the routine to be executed.
      * @param[in]  CleanupRoutine  - the routine to be used when apc is canceled.
      * @param[in]  NormalContext   - context to be passed to the routine.
      * @param[in]  SystemArgument1 - argument 1 to be passed to the routine.
      * @param[in]  SystemArgument2 - argument 2 to be passed to the routine.
      *
      * @return     A proper NTSTATUS error code.
      */
     NTSTATUS XPF_API
     ScheduleApc(
         _In_ KPROCESSOR_MODE Mode,
         _In_opt_ PKNORMAL_ROUTINE NormalRoutine,
         _In_opt_ PKNORMAL_ROUTINE CleanupRoutine,
         _In_opt_ PVOID NormalContext,
         _In_opt_ PVOID SystemArgument1,
         _In_opt_ PVOID SystemArgument2
     ) noexcept(true);

 private:
    /**
     * @brief           This routine is executed before executing the actual APC.
     *                  If the requested mode is kernel, we'll swap the routine
     *                  with our own (ApcNormalRoutine) thus delaying the cleanup.
     *                  If the requested mode is user, then we'll do the cleanup and bail.
     *
     * @param[in]       Apc             - an intialized KAPC which is used to get to our own
     *                                    Apc object.
     * @param[in,out]   NormalRoutine   - The routine which will be executed. If the mode
     *                                    is kernel, then this is swapped with ApcNormalRoutine.
     * @param[in,out]   NormalContext   - The context to be passed to the NormalRoutine.
     *                                    If the mode is kernel, then this is swapped with our
     *                                    own Apc object.
     * @param[in,out]   SystemArgument1 - We're not touching this.
     * @param[in,out]   SystemArgument2 - We're not touching this.
     *
     * @return          Nothing.
     */
    static void NTAPI
    ApcKernelRoutine(
        _In_ PKAPC Apc,
        _Inout_ PKNORMAL_ROUTINE* NormalRoutine,
        _Inout_ PVOID* NormalContext,
        _Inout_ PVOID* SystemArgument1,
        _Inout_ PVOID* SystemArgument2
    ) noexcept(true);

    /**
     * @brief           From this routine we'll call the original caller routine, and then
     *                  we'll remove the apc from the list.
     *
     * @param[in,out]   NormalRoutine   - This will be the Apc which can be used to get to the
     *                                    other details.
     * @param[in,out]   SystemArgument1 - We're not touching this.
     * @param[in,out]   SystemArgument2 - We're not touching this.
     *
     * @return          Nothing.
     */
    static void NTAPI
    ApcNormalRoutine(
        _In_opt_ PVOID NormalContext,
        _In_opt_ PVOID SystemArgument1,
        _In_opt_ PVOID SystemArgument2
    ) noexcept(true);

    /**
     * @brief       This is called when the apc is not executed but rather it is ran down.
     *              Not ideal, but from this one we'll invoke the caller routine.
     *              We might want to change in future.
     *
     * @param[in]   Apc - The passed apc.
     *
     * @return      Nothing.
     */
    static void NTAPI
    ApcRundownRoutine(
        _In_ PKAPC Apc
    ) noexcept(true);

    /**
     * @brief           This is used to remove the apc from the apc list.
     *
     * @param[in,out]   Apc - An enqueued APC, on output, this should not be used.
     *
     * @return          Nothing.
     */
    void XPF_API
    ApcRemove(
        _Inout_ PKAPC Apc
    ) noexcept(true);

 private:
     xpf::BusyLock m_ApcListLock;
     xpf::Vector<xpf::SharedPointer<KmHelper::Apc>> m_ApcList{ SYSMON_NPAGED_ALLOCATOR };
};  // class WorkQueue
};  // namespace KmHelper
