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

namespace SysMon
{
/**
 * @brief   This class is used to store information about modules which
 *          is specific to a given process. Such information includes can
 *          be for example the address where a module is loaded in a process.
 */
class ProcessModuleData final
{
 public:
    /**
     * @brief   The constructor for ProcessModuleData.
     *
     * @param[in,out]   ModulePath     - a string which contains the path of the module.
     * @param[in]       ModuleBase     - where the module is loaded in the current process.
     * @param[in]       ModuleSize     - the size of the laoded modules (in bytes). 
     */
    ProcessModuleData(
        _Inout_ xpf::String<wchar_t>&& ModulePath,
        _In_ _Const_ const void* ModuleBase,
        _In_ _Const_ const size_t& ModuleSize
    ) noexcept(true) : m_ModulePath{xpf::Move(ModulePath)},
                       m_ModuleBase{ModuleBase},
                       m_ModuleSize{ModuleSize},
                       m_ModuleEnd{xpf::AlgoAddToPointer(ModuleBase, ModuleSize)}
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        /* Path should not be empty. */
        XPF_DEATH_ON_FAILURE(!this->m_ModulePath.IsEmpty());
        /* Base should not be null. */
        XPF_DEATH_ON_FAILURE(this->m_ModuleBase != nullptr);
        /* Size should not be 0. */
        XPF_DEATH_ON_FAILURE(this->m_ModuleSize != 0);
        /* End should not be null. */
        XPF_DEATH_ON_FAILURE(this->m_ModuleEnd != nullptr);
        /* End should be after base. */
        XPF_DEATH_ON_FAILURE(this->m_ModuleBase < this->m_ModuleEnd);
    }

    /**
     * @brief   Default destructor
     */
    ~ProcessModuleData(void) noexcept(true) = default;

    /**
     * @brief   Copy is deleted.
     */
    XPF_CLASS_COPY_BEHAVIOR(SysMon::ProcessModuleData, delete);

    /**
     * @brief   Move is allowed.
     */
    XPF_CLASS_MOVE_BEHAVIOR(SysMon::ProcessModuleData, default);

    /**
     * @brief   Getter for the module path.
     *
     * @return  A view over the underlying module path.
     */
    inline
    xpf::StringView<wchar_t> XPF_API
    ModulePath(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ModulePath.View();
    }

    /**
     * @brief   Getter for the module base.
     *
     * @return  The address where this module has been mapped in this particular process.
     */
    inline
    const void* XPF_API
    ModuleBase(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ModuleBase;
    }

    /**
     * @brief   Getter for the end address. Note that this is the first address
     *          that is NOT part of the module.
     *
     * @return  The upper limit (not inclusive) in module.
     */
    inline
    const void* XPF_API
    ModuleEnd(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ModuleEnd;
    }

    /**
     * @brief   Getter for the module size.
     *
     * @return  The size of the module.
     */
    inline
    const size_t& XPF_API
    ModuleSize(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ModuleSize;
    }

 private:
    xpf::String<wchar_t> m_ModulePath;

    const void* m_ModuleBase = nullptr;
    const void* m_ModuleEnd = nullptr;
    size_t m_ModuleSize = 0;
};  // class ProcessModuleData

/**
 * @brief   This class store information about a process, such as its pid
 *          or its loaded modules.
 */
class ProcessData final
{
 private:
   /**
    * @brief           The constructor for ProcessData. Private because
    *                  Create method is intended to be used.
    *
    * @param[in,out]   ProcessPath - the path of the started process.
    * @param[in]       ProcessId   - the id of the started process.
    */
    ProcessData(
        _Inout_ xpf::String<wchar_t>&& ProcessPath,
        _In_ const uint32_t& ProcessId
    ) noexcept(true) : m_ProcessPath{xpf::Move(ProcessPath)},
                       m_ProcessId{ProcessId}
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        /* Path should not be empty. */
        XPF_DEATH_ON_FAILURE(!this->m_ProcessPath.IsEmpty());

        /* Process Id should be a multiple of 4 and non-null. */
        XPF_DEATH_ON_FAILURE(0 != ProcessId);
        XPF_DEATH_ON_FAILURE(ProcessId % 4 == 0);
    }

 public:
    /**
     * @brief  Default destructor.
     */
    ~ProcessData(void) noexcept(true) = default;

    /**
     * @brief   Copy and is deleted for this class.
     *          It can be implemented when required.
     */
    XPF_CLASS_COPY_BEHAVIOR(SysMon::ProcessData, delete);

    /**
     * @brief   Move behavior is allowed.
     */
    XPF_CLASS_MOVE_BEHAVIOR(SysMon::ProcessData, default);

   /**
    * @brief           The constructor for ProcessData. Private because
    *                  Create method is intended to be used.
    *
    * @param[in,out]   ProcessPath - the path of the started process.
    * @param[in]       ProcessId   - the id of the started process.
    *
    * @return          A properly initialized shared pointer with Process data.
    *                  Empty shared pointer on failure.
    */
    static xpf::SharedPointer<SysMon::ProcessData> XPF_API
    Create(
        _Inout_ xpf::String<wchar_t>&& ProcessPath,
        _In_ const uint32_t& ProcessId
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        NTSTATUS status = STATUS_UNSUCCESSFUL;
        xpf::SharedPointer<SysMon::ProcessData> result;

        result = xpf::MakeShared<SysMon::ProcessData>(xpf::Move(ProcessPath),
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

    /**
     * @brief   Inserts a new module in the list of loaded modules.
     *          This is a bit tricky because we are notified about
     *          image loads but not about image unloads.
     *          So we first need to see whether there are modules
     *          which may overlap with the newly loaded module
     *          and if there are, we need to erase them.
     *
     * @param[in] ModulePath - the loaded module path.
     * @param[in] ModuleBase - where the module is loaded in the current process.
     * @param[in] ModuleSize - the size of the laoded modules (in bytes).
     *
     * @return  A proper NTSTATUS error code.
     */
    inline
    NTSTATUS XPF_API
    InsertNewModule(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ _Const_ const void* ModuleBase,
        _In_ _Const_ const size_t& ModuleSize
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        NTSTATUS status = STATUS_UNSUCCESSFUL;

        /* We need to take ownership of the path, so duplicate it here. */
        xpf::String<wchar_t> modulePath;
        status = modulePath.Append(ModulePath);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        /* Create the new module. */
        SysMon::ProcessModuleData moduleData{ xpf::Move(modulePath),
                                              ModuleBase,
                                              ModuleSize };

        /* We're touching the modules so acquire exclusively. */
        xpf::ExclusiveLockGuard guard{ *this->m_LoadedModulesLock };

        /* Before insertion we erase all modules which may overlap. */
        for (size_t i = 0; i < this->m_LoadedModules.Size();)
        {
            /* Assume module is loaded at 0x8000 and has a size of 0x1000 => module end is 0x9000 */

            /* Module is loaded before this one is: (eg: at 0x1000 and ends at 0x2000 */
            if (this->m_LoadedModules[i].ModuleEnd() <= moduleData.ModuleBase())
            {
                ++i;
                continue;
            }

            /* Module is loaded after this one is: (eg: at 0x10000 and ends at 0x12000 */
            if (this->m_LoadedModules[i].ModuleBase() >= moduleData.ModuleEnd())
            {
                ++i;
                continue;
            }

            /* Module overlaps - we need to erase it. */
            status = this->m_LoadedModules.Erase(i);
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

        /* All good. */
        return STATUS_SUCCESS;
    }

    /**
     * @brief   Getter for the process id.
     *
     * @return  The id of the process
     */
    inline
    const uint32_t& XPF_API
    ProcessId(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ProcessId;
    }

 private:
    uint32_t m_ProcessId = 0;
    xpf::String<wchar_t> m_ProcessPath;

    xpf::Optional<xpf::ReadWriteLock> m_LoadedModulesLock;
    xpf::Vector<SysMon::ProcessModuleData> m_LoadedModules;

    /**
     * @brief   This is a friend class as it needs access so it can properly initialize
     *          the object so we won't return partially constructed objects.
     */
    friend class xpf::MemoryAllocator;
};  // class ProcessData

/**
 * @brief   This class store information about all running processes.
 */
class ProcessCollector final
{
 private:
    /**
     * @brief Private constructor as static method Construct should be used to
     *        properly initialize an object of type ProcessCollector.
     */
    ProcessCollector(void) noexcept(true) = default;
 public:
    /**
     * @brief Default destructor.
     */
    ~ProcessCollector(void) noexcept(true) = default;

    /**
     * @brief   Copy and move behavior are deleted.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ProcessCollector, delete);

    /**
     * @brief   Constructs an object of type process collector.
     *
     * @return  NULL on failure or a pointer to a properly initialized
     *          process collector instance.
     */
    static inline
    SysMon::ProcessCollector*
    XPF_API
    Construct(
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

    /**
     * @brief           Destroys a previously constructed process collector instance.
     *
     * @param[in,out]   Collector to be destroyed.
     *
     * @return          Nothing.
     */
    static inline
    void XPF_API
    Destruct(
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

    /**
     * @brief       Inserts a new process in the collector.
     *
     * @param[in]   ProcessId   - the id of the process which is to be inserted.
     * @param[in]   ProcessPath - the path of the process.
     *
     * @note        Processes will be sorted by their pid for fast lookup.
     *
     * @return      A proper NTSTATUS error code.
     */
    inline
    NTSTATUS XPF_API
    InsertProcess(
        _In_ _Const_ const uint32_t& ProcessId,
        _In_ _Const_ const xpf::StringView<wchar_t>& ProcessPath
    ) noexcept(true)
    {
        /* This should happen on process create only. */
        XPF_MAX_PASSIVE_LEVEL();

        NTSTATUS status = STATUS_UNSUCCESSFUL;
        xpf::String<wchar_t> processPath;

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

    /**
     * @brief       Erases an existing process from the collector.
     *
     * @param[in]   ProcessId   - the id of the process which is to be erased.
     *
     * @return      A proper NTSTATUS error code.
     */
    inline
    NTSTATUS XPF_API
    RemoveProcess(
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

    /**
     * @brief     Handles the module load notification.
     *
     * @param[in] ProcessPid - the id of the process in which the module is loaded.
     * @param[in] ModulePath - the loaded module path.
     * @param[in] ModuleBase - where the module is loaded in the current process.
     * @param[in] ModuleSize - the size of the laoded modules (in bytes).
     *
     * @return  A proper NTSTATUS error code.
     */
    inline
    NTSTATUS XPF_API
    HandleModuleLoad(
        _In_ _Const_ const uint32_t& ProcessPid,
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ _Const_ const void* ModuleBase,
        _In_ _Const_ const size_t& ModuleSize
    ) noexcept(true)
    {
        /* On image load :) */
        XPF_MAX_PASSIVE_LEVEL();

        /* Lookup the process. */
        xpf::SharedPointer<SysMon::ProcessData> process = this->LookupProcessData(ProcessPid);
        if (process.IsEmpty())
        {
            return STATUS_NOT_FOUND;
        }

        /* Associate the module with the process. */
        return process.Get()->InsertNewModule(ModulePath,
                                              ModuleBase,
                                              ModuleSize);
    }

 private:
     /**
      * @brief      Uses binary search to map a process id to its position in the vector.
      *             The m_ProcessesLock must be taken - it is caller responsibility.
      *
      * @param[in]  ProcessId - The process id to be searched.
      *
      * @return     An Optional size_t which has no value if the process id is not found, or
      *             it contains the index in which the element is found in the list.
      */
     inline
     xpf::Optional<size_t> XPF_API
     FindProcessIndex(
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

     /**
      * @brief      Uses FindProcessIndex to map a process id to its position in the vector.
      *             The m_ProcessesLock must NOT be taken - this function will acquire it.
      *
      * @param[in]  ProcessId - The process id to be searched.
      *
      * @return     The shared pointer describing the process. Empty if not found.
      */
     inline
     xpf::SharedPointer<SysMon::ProcessData> XPF_API
     LookupProcessData(
         _In_ _Const_ const uint32_t& ProcessId
     )
     {
         XPF_MAX_APC_LEVEL();
         xpf::SharedPointer<SysMon::ProcessData> result;
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

 private:
    xpf::Optional<xpf::ReadWriteLock> m_ProcessesLock;
    xpf::Vector<xpf::SharedPointer<SysMon::ProcessData>> m_Processes;

    /**
     * @brief   This is a friend class as it needs access so it can properly initialize
     *          the object so we won't return partially constructed objects.
     */
    friend class xpf::MemoryAllocator;
};  // class ProcessCollector
};  // namespace SysMon

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
