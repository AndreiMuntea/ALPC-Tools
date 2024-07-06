/**
 * @file        ALPC-Tools/AlpcMon_Sys/WorkQueue.cpp
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

#include "precomp.hpp"

#include "WorkQueue.hpp"
#include "trace.hpp"

/**
 * @brief   Everything from belows goes into default section.
 */
XPF_SECTION_DEFAULT;

/**
 * @brief   This structure is used to describe a work item.
 *          This is what gets enqueued, as we need the WorkQueue
 *          to rundown so we are not left with unfreed items.
 *
 * @note    This is more lightweight than having a threadpool.
 */
struct WorkQueueItem
{
    /**
     * @brief   The WORK_QUEUE_ITEM structure is used to post a work items to a system work queue. 
     */
    WORK_QUEUE_ITEM WorkItem = { 0 };
    /**
     * @brief   The Callback that the caller wants to execute.
     */
    xpf::thread::Callback Callback = nullptr;
    /**
     * @brief   The caller-defined context for Callback.
     */
    xpf::thread::CallbackArgument Context = nullptr;
    /**
     * @brief   The work queue associated with this particular item.
     */
    KmHelper::WorkQueue* WorkQueue = nullptr;
    /**
     * @brief   An event that was provided to be signaled if the caller wants to wait
     *          for the result.
     */
    KEVENT* Signal = nullptr;
};  // struct WorkQueueItem


//
// ************************************************************************************************
// *                                This contains the paged section code.                         *
// ************************************************************************************************
//

/**
 * @brief   Everything from belows goes into paged section.
 */
XPF_SECTION_PAGED;

KmHelper::WorkQueue::WorkQueue(
    void
) noexcept(true) : m_WorkQueueAllocator{sizeof(WorkQueueItem), true}
{
    /* We should construct work queues at passive. */
    XPF_MAX_PASSIVE_LEVEL();
}

KmHelper::WorkQueue::~WorkQueue(
    void
) noexcept(true)
{
    /* We should run down work queues at passive. */
    XPF_MAX_PASSIVE_LEVEL();

    /* Wait untill all enqueued items are ran. */
    while (xpf::ApiAtomicCompareExchange(&this->m_EnqueuedItems, uint32_t{ 0 }, uint32_t{ 0 }) != 0)
    {
        xpf::ApiSleep(500);
    }
}

_Use_decl_annotations_
void XPF_API
KmHelper::WorkQueue::WorkQueueWorkItemRoutine(
    _In_ PVOID Parameter
) noexcept(true)
{
    /* This routine will be called from inside a system thread. */
    XPF_MAX_PASSIVE_LEVEL();

    auto item = XPF_CONTAINING_RECORD(Parameter, WorkQueueItem, WorkItem);
    if (item)
    {
        /* Invoke the callback. */
        if (item->Callback)
        {
            item->Callback(item->Context);
        }

        /* Notify the caller. */
        if (item->Signal)
        {
            ::KeSetEvent(item->Signal, IO_NO_INCREMENT, FALSE);
        }

        /* Destroy the item. */
        xpf::MemoryAllocator::Destruct(item);

        /* Free the item. */
        KmHelper::WorkQueue* queue = item->WorkQueue;
        if (queue)
        {
            queue->m_WorkQueueAllocator.FreeMemory(item);
            xpf::ApiAtomicDecrement(&queue->m_EnqueuedItems);
        }
    }
}

//
// ************************************************************************************************
// *                                This contains the nonpaged section code.                      *
// ************************************************************************************************
//

/**
 * @brief   Everything from belows goes into default section.
 */
XPF_SECTION_DEFAULT;

void XPF_API
KmHelper::WorkQueue::EnqueueWork(
    _In_ xpf::thread::Callback Callback,
    _In_opt_ xpf::thread::CallbackArgument Argument,
    _In_ bool Wait
) noexcept(true)
{
    /* We can enequeue at any IRQL. */
    XPF_MAX_DISPATCH_LEVEL();

    WorkQueueItem* item = nullptr;
    KEVENT signal = { 0 };

    /* We're enqueing another item. */
    xpf::ApiAtomicIncrement(&this->m_EnqueuedItems);

    /* Allocate an item. */
    while (item == nullptr)
    {
        /* This is a critical allocator, we don't expect non paged memory to run out. */
        item = static_cast<WorkQueueItem*>(this->m_WorkQueueAllocator.AllocateMemory(sizeof(WorkQueueItem)));
        if (item == nullptr)
        {
            /* Retry again with a delay. */
            xpf::ApiYieldProcesor();
        }
    }

    /* Construct the work queue item. */
    xpf::MemoryAllocator::Construct(item);

    /* Assign other arguments. */
    item->Callback = Callback;
    item->Context = Argument;
    item->WorkQueue = this;

    /* Caller wants us to wait. */
    if (Wait)
    {
        /* Can't wait on higher irqls. */
        XPF_DEATH_ON_FAILURE(::KeGetCurrentIrql() <= APC_LEVEL);
        _Analysis_assume_(KeGetCurrentIrql() <= APC_LEVEL);

        ::KeInitializeEvent(&signal, EVENT_TYPE::NotificationEvent, FALSE);
        item->Signal = &signal;
    }

    /* Apis are marked deprecated. */
    #pragma warning(push)
    #pragma warning(disable: 4996)

    {
        /* Initialize the work item. */
        ::ExInitializeWorkItem(&item->WorkItem,
                               KmHelper::WorkQueue::WorkQueueWorkItemRoutine,
                               item);

        /* Run the work item. */
        ::ExQueueWorkItem(&item->WorkItem,
                          (Wait) ? WORK_QUEUE_TYPE::RealTimeWorkQueue
                                 : WORK_QUEUE_TYPE::CriticalWorkQueue);
    }

    /* Re-enable deprecation warning. */
    #pragma warning(pop)

    /* Wait if required. */
    if (Wait)
    {
        XPF_DEATH_ON_FAILURE(::KeGetCurrentIrql() <= APC_LEVEL);
        _Analysis_assume_(KeGetCurrentIrql() <= APC_LEVEL);

        /* We're not expecting this to fail. */
        NTSTATUS status = ::KeWaitForSingleObject(&signal,
                                                  KWAIT_REASON::Executive,
                                                  KernelMode,
                                                  FALSE,
                                                  NULL);
        XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));
    }
}
