/**
 * @file        ALPC-Tools/AlpcMon_Sys/ModuleCollector.hpp
 *
 * @brief       A structure containing data about modules.
 *              Such as hash values and exports.
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
#include "HashUtils.hpp"
#include "WorkQueue.hpp"


namespace SysMon
{
/**
 * @brief   This class is used to store information about modules
 *          from the current machine. The data in these modules is
 *          not associated with a specific process, so it does not
 *          contain information such as image base which may vary.
 *
 *          Rather, it contains generic information like the module path
 *          and the module hash which is the same.
 */
class ModuleData final
{
 public:
    /**
     * @brief           The constructor for module data.
     *
     * @param[in,out]   ModulePath     - a string which contains the path of the module.
     * @param[in]       PathHash       - an unsigned value containing the hash for the ModulePath.
     *                                   This is the hash of the string defining the path.
     * @param[in,out]   ModuleHash     - The hash of the content of the module.
     * @param[in]       ModuleHashType - The type of hash that was computed.
     * @param[in,out]   ModuleSymbols  - Extracted modules symbols.
     */
    ModuleData(
        _Inout_ xpf::String<wchar_t>&& ModulePath,
        _In_ uint32_t PathHash,
        _Inout_ xpf::Buffer&& ModuleHash,
        _In_ KmHelper::File::HashType ModuleHashType,
        _Inout_ xpf::Vector<xpf::pdb::SymbolInformation>&& ModuleSymbols
    ) noexcept(true) : m_ModulePath{xpf::Move(ModulePath)},
                       m_PathHash{ PathHash },
                       m_ModuleHash{xpf::Move(ModuleHash)},
                       m_ModuleHashType{ModuleHashType},
                       m_ModulesSymbols{xpf::Move(ModuleSymbols)}
    {
        /* Path should not be empty. */
        XPF_ASSERT(!this->m_ModulePath.IsEmpty());
        /* Hash should not be zero. */
        XPF_ASSERT(0 != this->m_PathHash);
    }

    /**
     * @brief   Default destructor
     */
    ~ModuleData(void) noexcept(true) = default;

    /**
     * @brief   Copy and move are deleted for this class.
     *          They can be implemented when required.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ModuleData, delete);

    /**
     * @brief   Getter for the module path.
     *
     * @return  A view over the underlying module path.
     */
    inline xpf::StringView<wchar_t> XPF_API
    ModulePath(
        void
    ) const noexcept(true)
    {
        return this->m_ModulePath.View();
    }

    /**
     * @brief   Getter for the hash of the path string.
     *
     * @return  A numerical value for the hash of the path string.
     *          i.e if the path is "C:\\Windows\\System32\\ntdll.dll"
     *          the path hash is the hash for this string.
     */
    inline uint32_t XPF_API
    PathHash(
        void
    ) const noexcept(true)
    {
        return this->m_PathHash;
    }

    /**
     * @brief   Getter for the module hash.
     *
     * @return  const reference of the buffer containing the module hash.
     */
    inline const xpf::Buffer& XPF_API
    ModuleHash(
        void
    ) const noexcept(true)
    {
        return this->m_ModuleHash;
    }

    /**
     * @brief   Getter for the module hash type.
     *
     * @return  Describes what type of hash the ModuleHash is.
     */
    inline const KmHelper::File::HashType& XPF_API
    ModuleHashType(
        void
    ) const noexcept(true)
    {
        return this->m_ModuleHashType;
    }

    /**
     * @brief   Getter for the modules symbols
     *
     * @return  The extracted modules symbols - might be empty if something failed,
     *          or the pdb was not found.
     */
    inline const xpf::Vector<xpf::pdb::SymbolInformation>& XPF_API
    ModuleSymbols(
        void
    ) const noexcept(true)
    {
        return this->m_ModulesSymbols;
    }

    /**
     * @brief       Checks whether this module is equal to the other one.
     *
     * @param[in]   ModulePath - a view over the string which contains the path of the
     *                           module to be compared against.
     * @param[in]   PathHash   - an unsigned value containing the hash for the ModulePath.
     *                           This is the hash of the string defining the path.
     *
     * @return      true if this module is considered equal to the Other.
     *              false otherwise.
     */
    inline bool XPF_API
    Equals(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ uint32_t PathHash
    ) const noexcept(true)
    {
        /* First hashes must match. */
        if (this->PathHash() != PathHash)
        {
            return false;
        }

        /* In case of collision we also check the paths. */
        return this->ModulePath().Equals(ModulePath, true);
    }

 private:
    xpf::String<wchar_t> m_ModulePath{ SYSMON_PAGED_ALLOCATOR };
    uint32_t m_PathHash = 0;

    xpf::Buffer m_ModuleHash{ SYSMON_PAGED_ALLOCATOR };
    KmHelper::File::HashType m_ModuleHashType = KmHelper::File::HashType::kMd5;

    xpf::Vector<xpf::pdb::SymbolInformation> m_ModulesSymbols{ SYSMON_PAGED_ALLOCATOR };
};  // class ModuleData

/**
 * @brief   This will be passed to a work callback to help with async initialization of modules.
 *          From a work routine we'll do all the work and then emplace the module into the
 *          module collector. This is done from a system worker thread.
 */
struct ModuleContext
{
    /**
     * @brief   The module path for which the computations have to be performed.
     */
    xpf::String<wchar_t> Path{ SYSMON_PAGED_ALLOCATOR };
};

/**
 * @brief   This class is used to store information about the modules.
 */
class ModuleCollector final
{
 private:
    /**
     * @brief  Default constructor - private as Create
     *         method must be used to instantiate an object.
     */
    ModuleCollector(
        void
    ) noexcept(true) : m_ModuleContextAllocator{sizeof(ModuleContext), false}
    {
        XPF_NOTHING();
    }

 public:
    /**
     * @brief   Default destructor.
     */
    ~ModuleCollector(void) noexcept(true)
    {
        /* The queue must be ran down before destroying other members. */
        this->m_IsQueueRunDown = true;
        this->m_ModulesWorkQueue.Reset();
    }

    /**
     * @brief   Copy and move are deleted for this class.
     *          They can be implemented when required.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::ModuleCollector, delete);

    /**
     * @brief   Creates a module collector object.
     *
     * @return  NULL on failure or a properly constructed
     *          ModuleCollector object.
     */
    static ModuleCollector* XPF_API
    Create(
        void
    ) noexcept(true);

    /**
     * @brief           Destroys a module collector instance.
     *
     * @param[in,out]   Instance - The instance to be destroyed.
     */
    static void XPF_API
    Destroy(
        _Inout_opt_ ModuleCollector* Instance
    ) noexcept(true);

    /**
     * @brief           Inserts a module in the list.
     *
     * @param[in,out]   ModulePath     - a string which contains the path of the module.
     * @param[in]       PathHash       - an unsigned value containing the hash for the ModulePath.
     *                                   This is the hash of the string defining the path.
     * @param[in,out]   ModuleHash     - The hash of the content of the module.
     * @param[in]       ModuleHashType - The type of hash that was computed.
     * @param[in]       ModulesSymbols - Extracted symbols information.
     *
     * @return          A proper NTSTATUS error value.
     */
    NTSTATUS XPF_API
    Insert(
        _Inout_ xpf::String<wchar_t>&& ModulePath,
        _In_ uint32_t PathHash,
        _Inout_ xpf::Buffer&& ModuleHash,
        _In_ KmHelper::File::HashType ModuleHashType,
        _Inout_ xpf::Vector<xpf::pdb::SymbolInformation>&& ModulesSymbols
    ) noexcept(true);

    /**
     * @brief       Searches for a given module in the list.
     *
     * @param[in]   ModulePath     - a view over the string which contains the path of the
     *                               module
     *
     * @return      Empty shared pointer if no data is found,
     *              a reference to the stored module data otherwise.
     */
    inline
    xpf::SharedPointer<SysMon::ModuleData> XPF_API
    Find(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
    ) noexcept(true);

    /**
     * @brief       Creates a new module context.
     *
     * @param[in]   ModulePath - the path of the module.
     *
     * @return      null on failure, or a pointer to the
     *              newly created module context otherwise.
     */
    ModuleContext* XPF_API
    CreateModuleContext(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
    ) noexcept(true);

    /**
     * @brief           Destroyes a previously allocated module context.
     *
     * @param[in,out]   Context - context to be destroyed.
     *
     * @return          Nothing.
     */
    void XPF_API
    DestroyModuleContext(
        _Inout_opt_ ModuleContext* Context
    ) noexcept(true);

    /**
     * @brief       Grabs the modules work queue which can be used to schedule work
     *              related to offline initialization of a module.
     *
     * @return      A reference to the underlying WorkQueue.
     */
    inline KmHelper::WorkQueue&
    XPF_API
    WorkQueue(
        void
    ) noexcept(true)
    {
        return (*this->m_ModulesWorkQueue);
    }

    /**
     * @brief   Checks if queue is running down - useful for early bailing when
     *          there are items enqueued left.
     *
     * @return  true if queue is running down, false otherwise.
     */
    inline bool
    XPF_API
    IsQueueRunDown(
        void
    ) noexcept(true)
    {
        return this->m_IsQueueRunDown;
    }

 private:
    xpf::Optional<xpf::ReadWriteLock> m_ModulesLock;
    xpf::Vector<xpf::SharedPointer<SysMon::ModuleData>> m_Modules{ SYSMON_PAGED_ALLOCATOR };
    xpf::LookasideListAllocator m_ModuleContextAllocator;
    xpf::Optional<KmHelper::WorkQueue> m_ModulesWorkQueue;
    bool m_IsQueueRunDown = false;

    /**
     * @brief   Default MemoryAllocator is our friend as it requires access to the private
     *          default constructor. It is used in the Create() method to ensure that
     *          no partially constructed objects are created but instead they will be
     *          all fully initialized.
     */
    friend class xpf::MemoryAllocator;
};  // class ModuleCollector
};  // namespace SysMon


/**
 * @brief       Creates the module collector.
 *
 * @return      A proper ntstatus error code.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver entry.
 *
 * @note        Must be called before registering the image filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS XPF_API
ModuleCollectorCreate(
    void
) noexcept(true);

/**
 * @brief       Destroys the previously created module collector
 *
 * @return      VOID.
 *
 * @note        This method can be called only at passive level.
 *              It is expected to be called only at driver unload.
 *
 * @note        Must be called after unregistering image filter.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ModuleCollectorDestroy(
    void
) noexcept(true);

/**
 * @brief       This API handles queries to the module collector
 *              to find a specific module given its path.
 *
 * @param[in]   ModulePath      - the path of the new module.
 *
 * @return      A shared pointer to module data.
 */
_IRQL_requires_max_(APC_LEVEL)
xpf::SharedPointer<SysMon::ModuleData> XPF_API
ModuleCollectorFindModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true);

/**
 * @brief       This API handles the creation of a new module.
 *              It first looks up in the module collector cache.
 *              If the module is not found, it enqueues an async
 *              work item to compute the details about this module.
 *
 * @param[in]   ModulePath      - the path of the new module.
 *
 * @return      Nothing.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
void XPF_API
ModuleCollectorHandleNewModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true);
