/**
 * @file        ALPC-Tools/AlpcMon_Sys/ApcQueue.cpp
 *
 * @brief       In this file we define a wrapper over the APC functionality.
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
#include "globals.hpp"

#include "ApcQueue.hpp"
#include "trace.hpp"

 /**
  * @brief   Everything from belows goes into paged section.
  */
XPF_SECTION_PAGED;

KmHelper::ApcQueue::~ApcQueue(void) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    /* Cancel all pending apcs. */
    PFUNC_KeRemoveQueueApc keRemoveApcApi = GlobalDataGetDynamicData()->ApiKeRemoveQueueApc;
    if (keRemoveApcApi != nullptr)
    {
        xpf::ExclusiveLockGuard guard{ this->m_ApcListLock };
        for (size_t i = 0; i < this->m_ApcList.Size();)
        {
            KmHelper::Apc* apc = this->m_ApcList[i].Get();

            const BOOLEAN removed = keRemoveApcApi(xpf::AddressOf(apc->OriginalApc));
            if (FALSE != removed)
            {
                if (apc->OriginalCleanupRoutine)
                {
                    apc->OriginalCleanupRoutine(apc->OriginalNormalConext,
                                                apc->OriginalSystemArgument1,
                                                apc->OriginalSystemArgument2);
                }
                this->ApcRemove(xpf::AddressOf(apc->OriginalApc));
            }
            else
            {
                i++;
            }
        }
    }

    /* Rundown until all apcs are executed. */
    while (true)
    {
        xpf::ApiSleep(300);

        xpf::SharedLockGuard guard{ this->m_ApcListLock };
        if (this->m_ApcList.IsEmpty())
        {
            break;
        }
    }
}

void NTAPI
KmHelper::ApcQueue::ApcKernelRoutine(
    _In_ PKAPC Apc,
    _Inout_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ PVOID* NormalContext,
    _Inout_ PVOID* SystemArgument1,
    _Inout_ PVOID* SystemArgument2
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    /* We don't expect this to be null. */
    KmHelper::Apc* apc = XPF_CONTAINING_RECORD(Apc, KmHelper::Apc, OriginalApc);
    if (nullptr == apc)
    {
        XPF_DEATH_ON_FAILURE(false);
        return;
    }

    /* Invariants. */
    XPF_DEATH_ON_FAILURE(apc->ApcQueueObject != nullptr);
    XPF_DEATH_ON_FAILURE(apc->OriginalNormalRoutine == *NormalRoutine);
    XPF_DEATH_ON_FAILURE(apc->OriginalNormalConext == *NormalContext);
    XPF_DEATH_ON_FAILURE(apc->OriginalSystemArgument1 == *SystemArgument1);
    XPF_DEATH_ON_FAILURE(apc->OriginalSystemArgument2 == *SystemArgument2);

    if (apc->Mode == KernelMode)
    {
        /* If the mode is kernel, we'll replace with our own routine. */
        /* We need to keep the driver up & running until the apcs are executed. */

        *NormalRoutine = KmHelper::ApcQueue::ApcNormalRoutine;
        *NormalContext = apc;
    }
    else
    {
        /* The mode is user mode. We can safely clean the resources. */
        /* The user routine will be executed afterwards. */
        apc->ApcQueueObject->ApcRemove(Apc);
    }
}

void NTAPI
KmHelper::ApcQueue::ApcNormalRoutine(
    _In_opt_ PVOID NormalContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    /* We don't expect this to be null. */
    KmHelper::Apc* apc = static_cast<KmHelper::Apc*>(NormalContext);
    if (nullptr == apc)
    {
        XPF_DEATH_ON_FAILURE(false);
        return;
    }

    /* Invariants. */
    XPF_DEATH_ON_FAILURE(apc->ApcQueueObject != nullptr);
    XPF_DEATH_ON_FAILURE(apc->OriginalSystemArgument1 == SystemArgument1);
    XPF_DEATH_ON_FAILURE(apc->OriginalSystemArgument2 == SystemArgument2);

    /* Execute the original APC. */
    if (apc->OriginalNormalRoutine)
    {
        apc->OriginalNormalRoutine(apc->OriginalNormalConext,
                                   apc->OriginalSystemArgument1,
                                   apc->OriginalSystemArgument2);
    }

    /* And dequeue the apc from the list. */
    apc->ApcQueueObject->ApcRemove(xpf::AddressOf(apc->OriginalApc));
}

void NTAPI
KmHelper::ApcQueue::ApcRundownRoutine(
    _In_ PKAPC Apc
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    /* We don't expect this to be null. */
    KmHelper::Apc* apc = XPF_CONTAINING_RECORD(Apc, KmHelper::Apc, OriginalApc);
    if (nullptr == apc)
    {
        XPF_DEATH_ON_FAILURE(false);
        return;
    }

    /* Invariants. */
    XPF_DEATH_ON_FAILURE(apc->ApcQueueObject != nullptr);

    /* If the mode is kernel, we'll execute the cleanup here. */
    if (apc->Mode == KernelMode && apc->OriginalCleanupRoutine != nullptr)
    {
        apc->OriginalCleanupRoutine(apc->OriginalNormalConext,
                                    apc->OriginalSystemArgument1,
                                    apc->OriginalSystemArgument2);
    }

    /* Remove the apc from the list. */
    apc->ApcQueueObject->ApcRemove(Apc);
}

NTSTATUS XPF_API
KmHelper::ApcQueue::ScheduleApc(
    _In_ KPROCESSOR_MODE Mode,
    _In_opt_ PKNORMAL_ROUTINE NormalRoutine,
    _In_opt_ PKNORMAL_ROUTINE CleanupRoutine,
    _In_opt_ PVOID NormalContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::SharedPointer<KmHelper::Apc> apc{ this->m_ApcList.GetAllocator() };

    PFUNC_KeInitializeApc keInitializeApcApi = GlobalDataGetDynamicData()->ApiKeInitializeApc;
    PFUNC_KeInsertQueueApc keInsertQueueApcApi = GlobalDataGetDynamicData()->ApiKeInsertQueueApc;

    /* If the APIs are not resolved, we can't do much.*/
    if (nullptr == keInitializeApcApi || nullptr == keInsertQueueApcApi)
    {
        return STATUS_NOINTERFACE;
    }

    /* If the thread is terminating, we can't do much. */
    if (FALSE != ::PsIsThreadTerminating(::PsGetCurrentThread()))
    {
        return STATUS_TOO_LATE;
    }

    /* On x64 we need to encode the routine when injecting in wow processes. */
    if constexpr (SysMon::CurrentOsArchitecture() == SysMon::OsArchitecture::amd64)
    {
        if (Mode == UserMode && KmHelper::WrapperIsWow64Process(PsGetCurrentProcess()))
        {
            PVOID wrappedRoutine = NormalRoutine;
            PVOID wrappedContext = NormalContext;

            status = ::PsWrapApcWow64Thread(&wrappedContext,
                                            &wrappedRoutine);
            if (!NT_SUCCESS(status))
            {
                return status;
            }

            NormalRoutine = static_cast<PKNORMAL_ROUTINE>(wrappedRoutine);
            NormalContext = wrappedContext;
        }
    }

    /* Create a new apc. */
    apc = xpf::MakeSharedWithAllocator<KmHelper::Apc>(this->m_ApcList.GetAllocator());
    if (apc.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Now let's properly initialize the apc object. */
    KmHelper::Apc& apcRef = (*apc);

    apcRef.Mode = Mode;
    apcRef.ApcQueueObject = this;
    apcRef.OriginalNormalRoutine = NormalRoutine;
    apcRef.OriginalCleanupRoutine = CleanupRoutine;
    apcRef.OriginalNormalConext = NormalContext;
    apcRef.OriginalSystemArgument1 = SystemArgument1;
    apcRef.OriginalSystemArgument2 = SystemArgument2;

    keInitializeApcApi(xpf::AddressOf(apcRef.OriginalApc),
                       ::PsGetCurrentThread(),
                       KAPC_ENVIRONMENT::OriginalApcEnvironment,
                       KmHelper::ApcQueue::ApcKernelRoutine,
                       KmHelper::ApcQueue::ApcRundownRoutine,
                       NormalRoutine,
                       Mode,
                       NormalContext);

    /* Enqueue it. */
    {
        xpf::ExclusiveLockGuard guard{ this->m_ApcListLock };
        status = this->m_ApcList.Emplace(apc);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    /* Now we need to insert it. */
    const BOOLEAN result = keInsertQueueApcApi(xpf::AddressOf(apcRef.OriginalApc),
                                               SystemArgument1,
                                               SystemArgument2,
                                               IO_NO_INCREMENT);
    if (result == FALSE)
    {
        this->ApcRemove(xpf::AddressOf(apcRef.OriginalApc));
        status = STATUS_TOO_LATE;
    }

    return status;
}

void XPF_API
KmHelper::ApcQueue::ApcRemove(
    _Inout_ PKAPC Apc
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    /* We don't expect this to be null. */
    KmHelper::Apc* apc = XPF_CONTAINING_RECORD(Apc, KmHelper::Apc, OriginalApc);
    if (nullptr == apc)
    {
        XPF_DEATH_ON_FAILURE(false);
        return;
    }

    /* Prevent races. */
    xpf::ExclusiveLockGuard guard{ this->m_ApcListLock };

    /* Find the apc and remove it. */
    for (size_t i = 0; i < this->m_ApcList.Size();)
    {
        if (this->m_ApcList[i].Get() == apc)
        {
            const NTSTATUS status = this->m_ApcList.Erase(i);
            XPF_DEATH_ON_FAILURE(NT_SUCCESS(status));
        }
        else
        {
            i++;
        }
    }
}
