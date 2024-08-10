/**
 * @file        ALPC-Tools/AlpcMon_Sys/ProcessCollector.cpp
 *
 * @brief       A structure containing data about processes.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "ProcessCollector.hpp"
#include "trace.hpp"

//
// ************************************************************************************************
// *                                This contains the paged section code.                         *
// ************************************************************************************************
//

XPF_SECTION_PAGED


//
// ************************************************************************************************
// *                                Process data API.                                             *
// ************************************************************************************************
//


xpf::SharedPointer<SysMon::ProcessData> XPF_API
SysMon::ProcessData::Create(
    _Inout_ xpf::String<wchar_t>&& ProcessPath,
    _In_ const uint32_t& ProcessId
) noexcept(true)
{
    /* Code is paged. */
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::SharedPointer<SysMon::ProcessData> result{ SYSMON_PAGED_ALLOCATOR };

    result = xpf::MakeSharedWithAllocator<SysMon::ProcessData>(SYSMON_PAGED_ALLOCATOR,
                                                               xpf::Move(ProcessPath),
                                                               ProcessId);
    if (result.IsEmpty())
    {
        return result;
    }

    status = xpf::ReadWriteLock::Create(xpf::AddressOf(result.Get()->m_LoadedModulesLock));
    if (!NT_SUCCESS(status))
    {
        result.Reset();
    }
    return result;
}

NTSTATUS XPF_API
SysMon::ProcessData::InsertNewModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
    _In_ _Const_ const void* ModuleBase,
    _In_ _Const_ const size_t& ModuleSize
) noexcept(true)
{
    /* Code is paged. */
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* We need to take ownership of the path, so duplicate it here. */
    xpf::String<wchar_t> modulePath{ SYSMON_PAGED_ALLOCATOR };
    status = modulePath.Append(ModulePath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Create the new module. */
    xpf::SharedPointer<SysMon::ProcessModuleData> moduleData{ SYSMON_PAGED_ALLOCATOR };
    moduleData = xpf::MakeSharedWithAllocator<SysMon::ProcessModuleData>(SYSMON_PAGED_ALLOCATOR,
                                                                         xpf::Move(modulePath),
                                                                         ModuleBase,
                                                                         ModuleSize);
    if (moduleData.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* We're touching the modules so acquire exclusively. */
    xpf::ExclusiveLockGuard guard{ *this->m_LoadedModulesLock };

    xpf::Optional<size_t> index = FindIndexOfModuleContainingAddress(ModuleBase);
    if (index.HasValue())
    {
        status = this->m_LoadedModules.Erase(*index);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }
    index = FindIndexOfModuleContainingAddress(xpf::AlgoAddToPointer(ModuleBase, ModuleSize - 1));
    if (index.HasValue())
    {
        status = this->m_LoadedModules.Erase(*index);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    /* Now insert the newly loaded module. */
    status = this->m_LoadedModules.Emplace(xpf::Move(moduleData));
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Keep modules sorted by module base. */
    this->m_LoadedModules.Sort([&](const xpf::SharedPointer<SysMon::ProcessModuleData>& Left,
                                   const xpf::SharedPointer<SysMon::ProcessModuleData>& Right)
                               {
                                   XPF_MAX_PASSIVE_LEVEL();
                                   return xpf::AlgoPointerToValue(Left.Get()->ModuleBase()) <
                                          xpf::AlgoPointerToValue(Right.Get()->ModuleBase());
                               });
    /* All good. */
    return STATUS_SUCCESS;
}

xpf::SharedPointer<SysMon::ProcessModuleData> XPF_API
SysMon::ProcessData::FindModuleContainingAddress(
    _In_ _Const_ const void* Address
) noexcept(true)
{
    /* Code is paged. */
    XPF_MAX_APC_LEVEL();

    xpf::SharedPointer<SysMon::ProcessModuleData> foundModuleData{ SYSMON_PAGED_ALLOCATOR };

    /* Shared as we're only looking up. */
    xpf::SharedLockGuard guard{ *this->m_LoadedModulesLock };

    /* If we found something, we return it. */
    xpf::Optional<size_t> index = FindIndexOfModuleContainingAddress(Address);
    if (index.HasValue())
    {
        foundModuleData = this->m_LoadedModules[*index];
    }
    return foundModuleData;
}

xpf::Optional<size_t> XPF_API
SysMon::ProcessData::FindIndexOfModuleContainingAddress(
    _In_ _Const_ const void* Address
) noexcept(true)
{
    /* Code is paged. */
    XPF_MAX_APC_LEVEL();

    xpf::Optional<size_t> index;
    const uint64_t addressValue = xpf::AlgoPointerToValue(Address);

    if (this->m_LoadedModules.IsEmpty())
    {
        return index;
    }

    size_t lo = 0;
    size_t hi = this->m_LoadedModules.Size() - 1;

    while (lo <= hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        const uint64_t moduleBase = xpf::AlgoPointerToValue(this->m_LoadedModules[mid].Get()->ModuleBase());
        const uint64_t moduleEnd  = xpf::AlgoPointerToValue(this->m_LoadedModules[mid].Get()->ModuleEnd());

        /* [moduleBase, moduleEnd) contains address. */
        if (moduleBase <= addressValue && addressValue < moduleEnd)
        {
            index.Emplace(mid);
            break;
        }
        /* address is greater than module end, so we lookup right. */
        else if (moduleEnd <= addressValue)
        {
            if (mid == xpf::NumericLimits<size_t>::MaxValue())
            {
                break;
            }
            lo = mid + 1;
        }
        /* address is smaller than module base, so we lookup left. */
        else
        {
            if (mid == 0)
            {
                break;
            }
            hi = mid - 1;
        }
    }

    return index;
}

//
// ************************************************************************************************
// *                                Process collector API.                                        *
// ************************************************************************************************
//


SysMon::ProcessCollector* XPF_API
SysMon::ProcessCollector::Construct(
    void
) noexcept(true)
{
    /* Expected on driver entry. */
    XPF_MAX_PASSIVE_LEVEL();

    SysMon::ProcessCollector* collector = nullptr;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /* Allocate the collector. */
    collector = static_cast<SysMon::ProcessCollector*>(
                xpf::MemoryAllocator::AllocateMemory(sizeof(SysMon::ProcessCollector)));
    if (nullptr == collector)
    {
        goto CleanUp;
    }
    xpf::MemoryAllocator::Construct(collector);

    /* Create the members. */
    status = xpf::ReadWriteLock::Create(&collector->m_ProcessesLock);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* All good. */
    status = STATUS_SUCCESS;

CleanUp:
    if (!NT_SUCCESS(status))
    {
        SysMon::ProcessCollector::Destruct(&collector);
    }
    return collector;
}

void XPF_API
SysMon::ProcessCollector::Destruct(
    _Inout_ SysMon::ProcessCollector** Collector
) noexcept(true)
{
    /* Expected on driver unload. */
    XPF_MAX_PASSIVE_LEVEL();

    if (nullptr == Collector || nullptr == (*Collector))
    {
        return;
    }

    xpf::MemoryAllocator::Destruct(*Collector);
    xpf::MemoryAllocator::FreeMemory(*Collector);

    *Collector = nullptr;
}

NTSTATUS XPF_API
SysMon::ProcessCollector::InsertProcess(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ProcessPath
) noexcept(true)
{
    /* This should happen on process create only. */
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    xpf::String<wchar_t> processPath{ SYSMON_PAGED_ALLOCATOR };

    /* We need ownership over path, so we duplicate it here. */
    status = processPath.Append(ProcessPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Create a shared pointer structure. */
    xpf::SharedPointer<SysMon::ProcessData> process = SysMon::ProcessData::Create(xpf::Move(processPath),
                                                                                  ProcessId);
    if (process.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Exclusive because we're editing the processees list. */
    xpf::ExclusiveLockGuard guard{ *this->m_ProcessesLock };

    /* If we somehow missed the process terminate notification, we need to erase it. */
    const xpf::Optional<size_t> existingProcessIndex = this->FindProcessIndex(ProcessId);
    if (existingProcessIndex.HasValue())
    {
        status = this->m_Processes.Erase(*existingProcessIndex);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    /* Emplace the new process. */
    status = this->m_Processes.Emplace(xpf::Move(process));
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Keep it sorted. */
    this->m_Processes.Sort([&](const xpf::SharedPointer<SysMon::ProcessData>& Left,
                               const xpf::SharedPointer<SysMon::ProcessData>& Right)
                           {
                               XPF_MAX_PASSIVE_LEVEL();
                               return Left.Get()->ProcessId() < Right.Get()->ProcessId();
                           });
    return STATUS_SUCCESS;
}

NTSTATUS XPF_API
SysMon::ProcessCollector::RemoveProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true)
{
    /* This should happen on process terminate only. */
    XPF_MAX_PASSIVE_LEVEL();

    /* Exclusive because we're editing the processees list. */
    xpf::ExclusiveLockGuard guard{ *this->m_ProcessesLock };

    const xpf::Optional<size_t> existingProcessIndex = this->FindProcessIndex(ProcessId);
    if (existingProcessIndex.HasValue())
    {
        return this->m_Processes.Erase(*existingProcessIndex);
    }

    /* Process does not exist - might be from before we started the sysmon, we're done. */
    return STATUS_SUCCESS;
}

xpf::SharedPointer<SysMon::ProcessData> XPF_API
SysMon::ProcessCollector::FindProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true)
{
    /* They are allocated paged. */
    XPF_MAX_APC_LEVEL();

    xpf::SharedPointer<SysMon::ProcessData> result{ SYSMON_PAGED_ALLOCATOR };
    xpf::Optional<size_t> index;

    /* Shared guard because we're just doing a lookup for the process data. */
    xpf::SharedLockGuard guard{ *this->m_ProcessesLock };
    index = this->FindProcessIndex(ProcessId);
    if (index.HasValue())
    {
        result = this->m_Processes[(*index)];
    }

    return result;
}

NTSTATUS XPF_API
SysMon::ProcessCollector::HandleModuleLoad(
    _In_ _Const_ const uint32_t& ProcessPid,
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
    _In_ _Const_ const void* ModuleBase,
    _In_ _Const_ const size_t& ModuleSize
) noexcept(true)
{
    /* On image load :) */
    XPF_MAX_PASSIVE_LEVEL();

    /* Lookup the process. */
    xpf::SharedPointer<SysMon::ProcessData> process = this->FindProcess(ProcessPid);
    if (process.IsEmpty())
    {
        return STATUS_NOT_FOUND;
    }

    /* Associate the module with the process. */
    return process.Get()->InsertNewModule(ModulePath,
                                          ModuleBase,
                                          ModuleSize);
}

xpf::Optional<size_t> XPF_API
SysMon::ProcessCollector::FindProcessIndex(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    xpf::Optional<size_t> index;
    if (this->m_Processes.IsEmpty())
    {
        return index;
    }

    size_t lo = 0;
    size_t hi = this->m_Processes.Size() - 1;

    while (lo <= hi)
    {
        size_t mid = lo + ((hi - lo) / 2);
        if (this->m_Processes[mid].Get()->ProcessId() == ProcessId)
        {
            index.Emplace(mid);
            break;
        }
        else if (this->m_Processes[mid].Get()->ProcessId() < ProcessId)
        {
            if (mid == xpf::NumericLimits<size_t>::MaxValue())
            {
                break;
            }
            lo = mid + 1;
        }
        else
        {
            if (mid == 0)
            {
                break;
            }
            hi = mid - 1;
        }
    }

    return index;
}


//
// ************************************************************************************************
// *                                This contains the user interface APIs                         *
// ************************************************************************************************
//

/**
 * @brief   Global instance containing process data.
 */
static SysMon::ProcessCollector* gProcessCollector = nullptr;

_Use_decl_annotations_
NTSTATUS XPF_API
ProcessCollectorCreate(
    void
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    /* This should not be called twice. */
    XPF_DEATH_ON_FAILURE(gProcessCollector == nullptr);

    SysMonLogInfo("Creating process collector...");

    gProcessCollector = SysMon::ProcessCollector::Construct();
    if (nullptr == gProcessCollector)
    {
        SysMonLogError("Insufficient resources to create the process collector!");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SysMonLogInfo("Successfully created the process collector!");
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void XPF_API
ProcessCollectorDestroy(
    void
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    SysMonLogInfo("Destroying the process collector...");

    if (nullptr != gProcessCollector)
    {
        SysMon::ProcessCollector::Destruct(&gProcessCollector);
        gProcessCollector = nullptr;
    }

    SysMonLogInfo("Successfully destroyed the process collector!");
}

_Use_decl_annotations_
void XPF_API
ProcessCollectorHandleCreateProcess(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ProcessPath
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    const NTSTATUS status = gProcessCollector->InsertProcess(ProcessId,
                                                             ProcessPath);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to insert a new process in the collector. Pid = %d (0x%x), status = %!STATUS!",
                       ProcessId,
                       ProcessId,
                       status);
    }
}

_Use_decl_annotations_
void XPF_API
ProcessCollectorHandleTerminateProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    const NTSTATUS status = gProcessCollector->RemoveProcess(ProcessId);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to insert remove a process from the collector. Pid = %d (0x%x), status = %!STATUS!",
                       ProcessId,
                       ProcessId,
                       status);
    }
}

_IRQL_requires_max_(APC_LEVEL)
xpf::SharedPointer<SysMon::ProcessData> XPF_API
ProcessCollectorFindProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true)
{
    /* The routine can be called only at APC_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    return gProcessCollector->FindProcess(ProcessId);
}


_Use_decl_annotations_
void XPF_API
ProcessCollectorHandleLoadModule(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
    _In_ _Const_ const void* ModuleBase,
    _In_ _Const_ const size_t& ModuleSize
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    const NTSTATUS status = gProcessCollector->HandleModuleLoad(ProcessId,
                                                                ModulePath,
                                                                ModuleBase,
                                                                ModuleSize);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Failed to handle module Load %S. Pid = %d (0x%x), status = %!STATUS!",
                       ModulePath.Buffer(),
                       ProcessId,
                       ProcessId,
                       status);
    }
}
