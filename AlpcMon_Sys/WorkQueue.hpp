/**
 * @file        ALPC-Tools/AlpcMon_Sys/WorkQueue.hpp
 *
 * @brief       In this file we define a wrapper over the work queue
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

namespace KmHelper
{
/**
 * @brief   In Windows KM we have a more light-weight mechanism instead of using threadpools.
 *          This is a work-queue where we just insert items and they are processed on already
 *          spawned system threads. This is more optimal as the OS takes care for us of synchronization.
 */
class WorkQueue final
{
 public:
     /**
      * @brief  Default constructor.
      */
     WorkQueue(void) noexcept(true);

     /**
      * @brief  Default destructor.
      */
     ~WorkQueue(void) noexcept(true);

     /**
      * @brief  The copy and move semantics are deleted.
      *         We can implement them when needed.
      */
     XPF_CLASS_COPY_MOVE_BEHAVIOR(WorkQueue, delete);

     /**
      * @brief      This routine is used to enqueue a work item.
      *             Can be called recursively from an already enqueued work item
      *             to avoid recursivity.
      *
      * @param[in]  Callback - The callback to be executed.
      * @param[in]  Argument - Passed as Callback for context.
      * @param[in]  Wait     - true if the caller should wait for results
      *
      * @note       We will enqueue our own routine (WorkQueueWorkItemRoutine)
      *             From this we will call the Callback. So the callback won't be
      *             directly called from the work item. This is required as we need
      *             to free some resources.
      *
      * @return     Nothing. This API is guaranteed to succeed.
      */
     void XPF_API
     EnqueueWork(
        _In_ xpf::thread::Callback Callback,
        _In_opt_ xpf::thread::CallbackArgument Argument,
        _In_ bool Wait
     ) noexcept(true);

 private:
     /**
      * @brief      We are using a separated callback so we can have better control
      *             on our resources (as we can free them to a lookaside list).
      *
      * @param[in]  Parameter - This is our custom structure defined in cpp file.
      */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    _Function_class_(WORKER_THREAD_ROUTINE)
    static void XPF_API
    WorkQueueWorkItemRoutine(
        _In_ PVOID Parameter
    ) noexcept(true);

 private:
    xpf::LookasideListAllocator m_WorkQueueAllocator;
    volatile uint32_t m_EnqueuedItems = 0;
};  // class WorkQueue
};  // namespace KmHelper
