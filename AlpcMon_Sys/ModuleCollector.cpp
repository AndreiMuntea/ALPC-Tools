
#include "precomp.hpp"

#include "HashUtils.hpp"
#include "KmHelper.hpp"
#include "globals.hpp"

#include "ModuleCollector.hpp"
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
     */
    ModuleData(
        _Inout_ xpf::String<wchar_t>&& ModulePath,
        _In_ uint32_t PathHash,
        _Inout_ xpf::Buffer<>&& ModuleHash,
        _In_ KmHelper::File::HashType ModuleHashType
    ) noexcept(true) : m_ModulePath{xpf::Move(ModulePath)},
                       m_PathHash{ PathHash },
                       m_ModuleHash{xpf::Move(ModuleHash)},
                       m_ModuleHashType{ModuleHashType}
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();
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
     * @brief   Getter for the hash of the path string.
     *
     * @return  A numerical value for the hash of the path string.
     *          i.e if the path is "C:\\Windows\\System32\\ntdll.dll"
     *          the path hash is the hash for this string.
     */
    inline
    uint32_t XPF_API
    PathHash(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_PathHash;
    }

    /**
     * @brief   Getter for the module hash.
     *
     * @return  const reference of the buffer containing the module hash.
     */
    inline
    const xpf::Buffer<>& XPF_API
    ModuleHash(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ModuleHash;
    }

    /**
     * @brief   Getter for the module hash type.
     *
     * @return  Describes what type of hash the ModuleHash is.
     */
    inline
    const KmHelper::File::HashType& XPF_API
    ModuleHashType(
        void
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        return this->m_ModuleHashType;
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
    inline
    bool XPF_API
    Equals(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ uint32_t PathHash
    ) const noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        /* First hashes must match. */
        if (this->PathHash() != PathHash)
        {
            return false;
        }

        /* In case of collision we also check the paths. */
        return this->ModulePath().Equals(ModulePath, true);
    }

 private:
    xpf::String<wchar_t> m_ModulePath;
    uint32_t m_PathHash = 0;

    xpf::Buffer<> m_ModuleHash;
    KmHelper::File::HashType m_ModuleHashType = KmHelper::File::HashType::kMd5;
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
    xpf::String<wchar_t> Path;
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
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();
    }

 public:
    /**
     * @brief   Default destructor.
     */
    ~ModuleCollector(void) noexcept(true) = default;

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
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        ModuleCollector* instance = nullptr;
        NTSTATUS status = STATUS_UNSUCCESSFUL;

        /* Create a new module collector. */
        instance = static_cast<ModuleCollector*>(xpf::MemoryAllocator::AllocateMemory(sizeof(SysMon::ModuleCollector)));
        if (nullptr == instance)
        {
            status = STATUS_UNSUCCESSFUL;
            goto CleanUp;
        }
        xpf::MemoryAllocator::Construct(instance);

        /* Now create the members. */
        status = xpf::ReadWriteLock::Create(&instance->m_ModulesLock);
        if (!NT_SUCCESS(status))
        {
            goto CleanUp;
        }

        /* All good. */
        status = STATUS_SUCCESS;

    CleanUp:
        if (!NT_SUCCESS(status))
        {
            SysMon::ModuleCollector::Destroy(instance);
            instance = nullptr;
        }
        return instance;
    }

    /**
     * @brief           Destroys a module collector instance.
     *
     * @param[in,out]   Instance - The instance to be destroyed.
     */
    static void XPF_API
    Destroy(
        _Inout_opt_ ModuleCollector* Instance
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        if (nullptr != Instance)
        {
            /* Destroy the object. */
            xpf::MemoryAllocator::Destruct(Instance);
            /* Free memory. */
            xpf::MemoryAllocator::FreeMemory(Instance);
        }
    }

    /**
     * @brief           Inserts a module in the list.
     *
     * @param[in,out]   ModulePath     - a string which contains the path of the module.
     * @param[in]       PathHash       - an unsigned value containing the hash for the ModulePath.
     *                                   This is the hash of the string defining the path.
     * @param[in,out]   ModuleHash     - The hash of the content of the module.
     * @param[in]       ModuleHashType - The type of hash that was computed.
     *
     * @return          A proper NTSTATUS error value.
     */
    inline
    NTSTATUS XPF_API
    Insert(
        _Inout_ xpf::String<wchar_t>&& ModulePath,
        _In_ uint32_t PathHash,
        _Inout_ xpf::Buffer<>&& ModuleHash,
        _In_ KmHelper::File::HashType ModuleHashType
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        xpf::SharedPointer<SysMon::ModuleData> newmodule;

        /* Check if the module was already added in list. */
        xpf::ExclusiveLockGuard guard{ *this->m_ModulesLock };
        for (size_t i = 0; i < this->m_Modules.Size(); ++i)
        {
            /* Module was already added.*/
            if (this->m_Modules[i].Get()->Equals(ModulePath.View(), PathHash))
            {
                return STATUS_ALREADY_REGISTERED;
            }
        }

        /* Create a new module. */
        newmodule = xpf::MakeShared<SysMon::ModuleData>(xpf::Move(ModulePath),
                                                        PathHash,
                                                        xpf::Move(ModuleHash),
                                                        ModuleHashType);
        if (newmodule.IsEmpty())
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        /* Emplace the new module. */
        return this->m_Modules.Emplace(newmodule);
    }

    /**
     * @brief       Searches for a given module in the list.
     *
     * @param[in]   ModulePath     - a view over the string which contains the path of the
     *                               module
     * @param[in]   PathHash       - an unsigned value containing the hash for the ModulePath.
     *                               This is the hash of the string defining the path.
     *
     * @return      Empty shared pointer if no data is found,
     *              a reference to the stored module data otherwise.
     */
    inline
    xpf::SharedPointer<SysMon::ModuleData> XPF_API
    Find(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath,
        _In_ uint32_t PathHash
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        xpf::SharedPointer<SysMon::ModuleData> foundModule;

        xpf::SharedLockGuard guard{ *this->m_ModulesLock };
        for (size_t i = 0; i < this->m_Modules.Size(); ++i)
        {
            if (this->m_Modules[i].Get()->Equals(ModulePath, PathHash))
            {
                foundModule = this->m_Modules[i];
                break;
            }
        }

        /* Empty if no module was found. */
        return foundModule;
    }

    /**
     * @brief       Creates a new module context.
     *
     * @param[in]   ModulePath - the path of the module.
     *
     * @return      null on failure, or a pointer to the
     *              newly created module context otherwise.
     */
    inline
    ModuleContext* XPF_API
    CreateModuleContext(
        _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        NTSTATUS status = STATUS_UNSUCCESSFUL;
        ModuleContext* context = nullptr;

        /* Allocate the new context. */
        context = static_cast<ModuleContext*>(this->m_ModuleContextAllocator.AllocateMemory(sizeof(ModuleContext)));
        if (nullptr == context)
        {
            return context;
        }

        /* Construct the context. */
        xpf::MemoryAllocator::Construct(context);

        /* Duplicate the module path. */
        status = context->Path.Append(ModulePath);
        if (!NT_SUCCESS(status))
        {
            this->DestroyModuleContext(context);
            context = nullptr;
        }

        return context;
    }

    /**
     * @brief           Destroyes a previously allocated module context.
     *
     * @param[in,out]   Context - context to be destroyed.
     *
     * @return          Nothing.
     */
    inline
    void XPF_API
    DestroyModuleContext(
        _Inout_opt_ ModuleContext* Context
    ) noexcept(true)
    {
        /* Code is paged. */
        XPF_MAX_APC_LEVEL();

        if (Context)
        {
            /* Destroy the object. */
            xpf::MemoryAllocator::Destruct(Context);

            /* Free the object. */
            this->m_ModuleContextAllocator.FreeMemory(Context);
        }
    }

 private:
    xpf::Optional<xpf::ReadWriteLock> m_ModulesLock;
    xpf::Vector<xpf::SharedPointer<SysMon::ModuleData>> m_Modules;
    xpf::LookasideListAllocator m_ModuleContextAllocator;

    /**
     * @brief   Default MemoryAllocator is our friend as it requires access to the private
     *          default constructor. It is used in the Create() method to ensure that
     *          no partially constructed objects are created but instead they will be
     *          all fully initialized.
     */
    friend class xpf::MemoryAllocator;
};  // class ModuleCollector
};  // namespace SysMon

//
// ************************************************************************************************
// *                Modules will be created from a work item.                                     *
// *                The callback will execute in a worker thread.                                 *
// ************************************************************************************************
//

/**
 * @brief   Global instance containing module data.
 */
static SysMon::ModuleCollector* gModuleCollector = nullptr;


static void XPF_API
ModuleCollectorWorkerCallback(
    _In_opt_ xpf::thread::CallbackArgument Argument
) noexcept(true)
{
    /* The routine can be called only at max PASSIVE_LEVEL from worker thread. */
    XPF_MAX_PASSIVE_LEVEL();

    uint32_t modulePathHash = 0;
    HANDLE fileHandle = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    KmHelper::File::HashType hashType = KmHelper::File::HashType::kMd5;
    xpf::Buffer<> hash;

    /* Don't expect this to be null. */
    SysMon::ModuleContext* data = static_cast<SysMon::ModuleContext*>(Argument);
    if (nullptr == data)
    {
        XPF_ASSERT(false);
        return;
    }

    /* Hash the string path. */
    status = KmHelper::HelperHashUnicodeString(data->Path.View(),
                                               &modulePathHash);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Open the module path. */
    status = KmHelper::File::OpenFile(data->Path.View(),
                                      KmHelper::File::FileAccessType::kRead,
                                      &fileHandle);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Hash the file. */
    status = KmHelper::File::HashFile(fileHandle,
                                      hashType,
                                      hash);
    /* Close the handle regardless of status. */
    KmHelper::File::CloseFile(&fileHandle);

    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Now insert it into module collector. */
    /* We already allocated the path in module context - so we'll move that memory. */
    status = gModuleCollector->Insert(xpf::Move(data->Path),
                                      modulePathHash,
                                      xpf::Move(hash),
                                      hashType);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

CleanUp:
    gModuleCollector->DestroyModuleContext(data);
}

static VOID XPF_API
ModuleCollectorCacheNewModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
)
{
    /* The routine can be called only at max PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    SysMon::ModuleContext* moduleContext = gModuleCollector->CreateModuleContext(ModulePath);
    if (moduleContext)
    {
        /* Enqueue the work item and do not wait inline to finish. */
        GlobalDataGetWorkQueueInstance()->EnqueueWork(ModuleCollectorWorkerCallback,
                                                      moduleContext,
                                                      false);
    }
}

//
// ************************************************************************************************
// *                                This contains the user interface APIs                         *
// ************************************************************************************************
//

_Use_decl_annotations_
NTSTATUS XPF_API
ModuleCollectorCreate(
    void
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    /* This should not be called twice. */
    XPF_DEATH_ON_FAILURE(gModuleCollector == nullptr);

    SysMonLogInfo("Creating module collector...");

    gModuleCollector = SysMon::ModuleCollector::Create();
    if (nullptr == gModuleCollector)
    {
        SysMonLogError("Insufficient resources to create the module collector!");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SysMonLogInfo("Successfully created the module collector!");
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void XPF_API
ModuleCollectorDestroy(
    void
) noexcept(true)
{
    /* The routine can be called only at PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    SysMonLogInfo("Destroying the module collector...");

    if (nullptr != gModuleCollector)
    {
        SysMon::ModuleCollector::Destroy(gModuleCollector);
        gModuleCollector = nullptr;
    }

    SysMonLogInfo("Successfully destroyed the module collector!");
}

_Use_decl_annotations_
VOID XPF_API
ModuleCollectorHandleNewModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true)
{
    /* The routine can be called only at max PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    uint32_t modulePathHash = 0;
    xpf::SharedPointer<SysMon::ModuleData> cachedModule;

    /* If module path is empty, we are done. */
    if (ModulePath.IsEmpty())
    {
        return;
    }

    /* First we need to hash the string for lookup. */
    status = KmHelper::HelperHashUnicodeString(ModulePath,
                                               &modulePathHash);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperHashUnicodeString failed with status %!STATUS!",
                       status);
        return;
    }

    /* Lookup the module in cache. */
    cachedModule = gModuleCollector->Find(ModulePath, modulePathHash);
    if (cachedModule.IsEmpty())
    {
        /* Create a new module. */
        ModuleCollectorCacheNewModule(ModulePath);
    }
}
