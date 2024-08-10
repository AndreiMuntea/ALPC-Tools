/**
 * @file        ALPC-Tools/AlpcMon_Sys/ModuleCollector.cpp
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

#include "precomp.hpp"

#include "HashUtils.hpp"
#include "KmHelper.hpp"
#include "WorkQueue.hpp"
#include "PdbHelper.hpp"

#include "ModuleCollector.hpp"
#include "trace.hpp"


//
// ************************************************************************************************
// *                                This contains the paged section code.                         *
// ************************************************************************************************
//

XPF_SECTION_PAGED

SysMon::ModuleCollector* XPF_API
SysMon::ModuleCollector::Create(
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
    instance->m_ModulesWorkQueue.Emplace();

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

void XPF_API
SysMon::ModuleCollector::Destroy(
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

NTSTATUS XPF_API
SysMon::ModuleCollector::Insert(
    _Inout_ xpf::String<wchar_t>&& ModulePath,
    _In_ uint32_t PathHash,
    _Inout_ xpf::Buffer&& ModuleHash,
    _In_ KmHelper::File::HashType ModuleHashType,
    _Inout_ xpf::Vector<xpf::pdb::SymbolInformation>&& ModulesSymbols
) noexcept(true)
{
    /* Code is paged. */
    XPF_MAX_APC_LEVEL();

    xpf::SharedPointer<SysMon::ModuleData> newmodule{ SYSMON_PAGED_ALLOCATOR };

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
    newmodule = xpf::MakeSharedWithAllocator<SysMon::ModuleData>(SYSMON_PAGED_ALLOCATOR,
                                                                    xpf::Move(ModulePath),
                                                                    PathHash,
                                                                    xpf::Move(ModuleHash),
                                                                    ModuleHashType,
                                                                    xpf::Move(ModulesSymbols));
    if (newmodule.IsEmpty())
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Emplace the new module. */
    return this->m_Modules.Emplace(newmodule);
}

xpf::SharedPointer<SysMon::ModuleData> XPF_API
SysMon::ModuleCollector::Find(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true)
{
    /* Code is paged. */
    XPF_MAX_PASSIVE_LEVEL();

    xpf::SharedPointer<SysMon::ModuleData> foundModule{ SYSMON_PAGED_ALLOCATOR };
    uint32_t modulePathHash = 0;

    /* Path can not be empty. */
    if (ModulePath.IsEmpty())
    {
        return foundModule;
    }

    /* First we need to hash the string for lookup. */
    NTSTATUS status = KmHelper::HelperHashUnicodeString(ModulePath,
                                                        &modulePathHash);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("HelperHashUnicodeString failed with status %!STATUS!",
                       status);
        return foundModule;
    }

    xpf::SharedLockGuard guard{ *this->m_ModulesLock };
    for (size_t i = 0; i < this->m_Modules.Size(); ++i)
    {
        if (this->m_Modules[i].Get()->Equals(ModulePath, modulePathHash))
        {
            foundModule = this->m_Modules[i];
            break;
        }
    }

    /* Empty if no module was found. */
    return foundModule;
}

SysMon::ModuleContext* XPF_API
SysMon::ModuleCollector::CreateModuleContext(
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

void XPF_API
SysMon::ModuleCollector::DestroyModuleContext(
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
    xpf::Optional<SysMon::File::FileObject> moduleFile;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    KmHelper::File::HashType hashType = KmHelper::File::HashType::kMd5;
    xpf::Buffer hash{ SYSMON_PAGED_ALLOCATOR };
    xpf::Vector<xpf::pdb::SymbolInformation> symbolsInformation{ SYSMON_PAGED_ALLOCATOR };

    /* Don't expect this to be null. */
    SysMon::ModuleContext* data = static_cast<SysMon::ModuleContext*>(Argument);
    if (nullptr == data)
    {
        XPF_ASSERT(false);
        return;
    }

    /* If queue is running down, we need to bail. Fast as we are unloading. */
    /* This check is done before expensive operations. */
    if (gModuleCollector->IsQueueRunDown())
    {
        goto CleanUp;
    }

    /* Hash the string path. */
    status = KmHelper::HelperHashUnicodeString(data->Path.View(),
                                               &modulePathHash);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Open the module path. */
    status = SysMon::File::FileObject::Create(data->Path.View(),
                                              XPF_FILE_ACCESS_READ,
                                              &moduleFile);
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

    /* Hash the file. */
    if (data->Path.View().EndsWith(L".exe", false))
    {
        status = KmHelper::File::HashFile((*moduleFile),
                                          hashType,
                                          hash);
        if (!NT_SUCCESS(status))
        {
            goto CleanUp;
        }

        /* Also log for tracing. */
        {
            const unsigned char* hashBuffer = static_cast<const unsigned char*>(hash.GetBuffer());
            SysMonLogTrace("Successfully computed md5 hash for %S : %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",  // NOLINT(*)
                            data->Path.View().Buffer(),                                                                                 // NOLINT(*)
                            uint16_t{hashBuffer[0]},  uint16_t{hashBuffer[1]},  uint16_t{hashBuffer[2]},  uint16_t{hashBuffer[3]},      // NOLINT(*)
                            uint16_t{hashBuffer[4]},  uint16_t{hashBuffer[5]},  uint16_t{hashBuffer[6]},  uint16_t{hashBuffer[7]},      // NOLINT(*)
                            uint16_t{hashBuffer[8]},  uint16_t{hashBuffer[9]},  uint16_t{hashBuffer[10]}, uint16_t{hashBuffer[11]},     // NOLINT(*)
                            uint16_t{hashBuffer[12]}, uint16_t{hashBuffer[13]}, uint16_t{hashBuffer[14]}, uint16_t{hashBuffer[15]});    // NOLINT(*)
        }
    }

    /* If this is a windows module we try to retrieve .pdb information */
    if (data->Path.View().Substring(L"\\Windows\\", false, nullptr) ||
        data->Path.View().Substring(L"\\SystemRoot\\", false, nullptr) ||
        data->Path.View().Substring(L"\\Microsoft\\", false, nullptr))
    {
        status = PdbHelper::ExtractPdbSymbolInformation((*moduleFile),
                                                        L"\\??\\C:\\Symbols\\",
                                                        &symbolsInformation);
        if (!NT_SUCCESS(status))
        {
            /* Non critical - we simply won't have symbols for this module. */
            symbolsInformation.Clear();
            status = STATUS_SUCCESS;
        }
    }

    /* Now insert it into module collector. */
    /* We already allocated the path in module context - so we'll move that memory. */
    status = gModuleCollector->Insert(xpf::Move(data->Path),
                                      modulePathHash,
                                      xpf::Move(hash),
                                      hashType,
                                      xpf::Move(symbolsInformation));
    if (!NT_SUCCESS(status))
    {
        goto CleanUp;
    }

CleanUp:
    gModuleCollector->DestroyModuleContext(data);
}

static void XPF_API
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
        gModuleCollector->WorkQueue().EnqueueWork(ModuleCollectorWorkerCallback,
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
void XPF_API
ModuleCollectorHandleNewModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true)
{
    /* The routine can be called only at max PASSIVE_LEVEL. */
    XPF_MAX_PASSIVE_LEVEL();

    xpf::SharedPointer<SysMon::ModuleData> cachedModule{ SYSMON_PAGED_ALLOCATOR };

    /* Lookup the module in cache. */
    cachedModule = gModuleCollector->Find(ModulePath);
    if (cachedModule.IsEmpty())
    {
        /* Create a new module. */
        ModuleCollectorCacheNewModule(ModulePath);
    }
}

_IRQL_requires_max_(APC_LEVEL)
xpf::SharedPointer<SysMon::ModuleData> XPF_API
ModuleCollectorFindModule(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModulePath
) noexcept(true)
{
    /* Modules are paged, so we can query them only at max apc level.*/
    XPF_MAX_APC_LEVEL();

    return gModuleCollector->Find(ModulePath);
}
