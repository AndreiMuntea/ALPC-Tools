/**
 * @file        ALPC-Tools/AlpcMon_Sys/ProcessCollector.hpp
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

#pragma once

#include "precomp.hpp"
#include "KmHelper.hpp"

//
// ************************************************************************************************
// *                                These are the underlying structures.                          *
// ************************************************************************************************
//

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
        return this->m_ModuleSize;
    }

 private:
     xpf::String<wchar_t> m_ModulePath{ SYSMON_PAGED_ALLOCATOR };

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
        /* Path should not be empty. */
        XPF_DEATH_ON_FAILURE(!this->m_ProcessPath.IsEmpty());

        /* Process Id should be a multiple of 4. */
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
    ) noexcept(true);

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
    NTSTATUS XPF_API
    InsertNewModule(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ _Const_ const void* ModuleBase,
        _In_ _Const_ const size_t& ModuleSize
    ) noexcept(true);

    /**
     * @brief   Look up module data associated with the current process
     *          for the module which contains the given address.
     *
     * @param[in]   Address - The address for which we need to retrieve the module.
     *
     * @return      A shared pointer to the process module data.
     *              Empty (nullptr) if the address is not found as part of any module.
     */
    xpf::SharedPointer<SysMon::ProcessModuleData> XPF_API
    FindModuleContainingAddress(
        _In_ _Const_ const void* Address
    ) noexcept(true);

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
        return this->m_ProcessId;
    }

 private:
    /**
     * @brief       Looks up the index in m_LoadedModules where we can find a given module.
     *              The lock must be acquired by the caller.
     *
     * @param[in]   Address - The address for which we need to retrieve the module.
     *
     * @return      An optional value representing the index in array to the process module data.
     *              Empty if the address is not found as part of any module.
     */
    xpf::Optional<size_t> XPF_API
    FindIndexOfModuleContainingAddress(
        _In_ _Const_ const void* Address
    ) noexcept(true);

 private:
    uint32_t m_ProcessId = 0;
    xpf::String<wchar_t> m_ProcessPath{ SYSMON_PAGED_ALLOCATOR };

    xpf::Optional<xpf::ReadWriteLock> m_LoadedModulesLock;
    xpf::Vector<xpf::SharedPointer<SysMon::ProcessModuleData>> m_LoadedModules{ SYSMON_PAGED_ALLOCATOR };

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
    static SysMon::ProcessCollector* XPF_API
    Construct(
        void
    ) noexcept(true);

    /**
     * @brief           Destroys a previously constructed process collector instance.
     *
     * @param[in,out]   Collector to be destroyed.
     *
     * @return          Nothing.
     */
    static
    void XPF_API
    Destruct(
        _Inout_ SysMon::ProcessCollector** Collector
    ) noexcept(true);

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
    NTSTATUS XPF_API
    InsertProcess(
        _In_ _Const_ const uint32_t& ProcessId,
        _In_ _Const_ const xpf::StringView<wchar_t>& ProcessPath
    ) noexcept(true);

    /**
     * @brief       Erases an existing process from the collector.
     *
     * @param[in]   ProcessId   - the id of the process which is to be erased.
     *
     * @return      A proper NTSTATUS error code.
     */
    NTSTATUS XPF_API
    RemoveProcess(
        _In_ _Const_ const uint32_t& ProcessId
    ) noexcept(true);

    /**
     * @brief       Finds an existing process from the collector.
     *
     * @param[in]   ProcessId   - the id of the process which is to be queried.
     *
     * @return      A shared pointer to found process data, nullptr if not found.
     */
    xpf::SharedPointer<SysMon::ProcessData> XPF_API
    FindProcess(
        _In_ _Const_ const uint32_t& ProcessId
    ) noexcept(true);

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
    NTSTATUS XPF_API
    HandleModuleLoad(
        _In_ _Const_ const uint32_t& ProcessPid,
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ _Const_ const void* ModuleBase,
        _In_ _Const_ const size_t& ModuleSize
    ) noexcept(true);

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
     xpf::Optional<size_t> XPF_API
     FindProcessIndex(
         _In_ _Const_ const uint32_t& ProcessId
     ) noexcept(true);

 private:
    xpf::Optional<xpf::ReadWriteLock> m_ProcessesLock;
    xpf::Vector<xpf::SharedPointer<SysMon::ProcessData>> m_Processes{ SYSMON_PAGED_ALLOCATOR };

    /**
     * @brief   This is a friend class as it needs access so it can properly initialize
     *          the object so we won't return partially constructed objects.
     */
    friend class xpf::MemoryAllocator;
};  // class ProcessCollector
};  // namespace SysMon

//
// ************************************************************************************************
// *                                These are the public facing APIs                              *
// ************************************************************************************************
//

/**
 * @brief       Creates the process collector.
 *
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver entry.
 *
 * @note        Must be called before registering the process filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ProcessCollectorCreate(
    void
) noexcept(true);

/**
 * @brief       Destroys the previously created process collector
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver unload.
 *
 * @note        Must be called after unregistering process filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorDestroy(
    void
) noexcept(true);

/**
 * @brief       This API handles the creation of a new process.
 *
 * @param[in]   ProcessId   - the id of the process which is created.
 * @param[in]   ProcessPath - the path of the process
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorHandleCreateProcess(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ProcessPath
) noexcept(true);

/**
 * @brief       This API handles the termination of an existing process.
 *
 * @param[in]   ProcessId - the id of the process which is terminated.
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorHandleTerminateProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true);

/**
 * @brief       This API handles queries to the process collector
 *              to find a specific process given its pid.
 *
 * @param[in]   ProcessId   - the id of the process which is queried.
 *
 * @return      A shared pointer to process data.
 */
_IRQL_requires_max_(APC_LEVEL)
xpf::SharedPointer<SysMon::ProcessData> XPF_API
ProcessCollectorFindProcess(
    _In_ _Const_ const uint32_t& ProcessId
) noexcept(true);

/**
 * @brief       This API handles the creation of a new module
 *              associated with a given process.
 *
 * @param[in]   ProcessId   - the id of the process in which the module is loaded.
 * @param[in]   ModulePath  - the path of the new module.
 * @param[in]   ModuleBase  - the base address in the process where the module is loaded.
 * @param[in]   ModuleSize  - the size of the module.
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ProcessCollectorHandleLoadModule(
    _In_ _Const_ const uint32_t& ProcessId,
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
    _In_ _Const_ const void* ModuleBase,
    _In_ _Const_ const size_t& ModuleSize
) noexcept(true);
