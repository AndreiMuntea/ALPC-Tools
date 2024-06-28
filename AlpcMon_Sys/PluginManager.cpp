/**
 * @file        ALPC-Tools/AlpcMon_Sys/PluginManager.cpp
 *
 * @brief       We're using plugins throughout the project.
 *              A plugin basically performes actions on the
 *              events that happen in the system.
 *              This is the "manager" or all plugins, which
 *              is responsible for creating and destroying them.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#include "precomp.hpp"

#include "UmHookPlugin.hpp"
#include "RpcAlpcInspectionPlugin.hpp"

#include "PluginManager.hpp"
#include "trace.hpp"


///
/// -------------------------------------------------------------------------------------------------------------------
/// | ****************************************************************************************************************|
/// |                                       PAGED SECTION AREA                                                        |
/// | ****************************************************************************************************************|
/// -------------------------------------------------------------------------------------------------------------------
///


 /**
  * @brief   Put everything below here in paged section.
  */
XPF_SECTION_PAGED;


//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::IPlugin::Register                                                 |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::IPlugin::Register(
    _Inout_ xpf::EventBus* EventBus
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != EventBus);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Registering plugin event listener with id %I64d to bus instance %p...",
                  this->m_PluginId,
                  EventBus);

    this->m_EventBus = EventBus;
    status = (*this->m_EventBus).RegisterListener(this,
                                                  &this->m_ListenerId);
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Registering plugin event listener with id %I64d failed with %!STATUS!",
                       this->m_PluginId,
                       status);
        goto CleanUp;
    }

    SysMonLogInfo("Plugin with id %I64d successfully registered! Registration id = %!GUID!",
                  this->m_PluginId,
                  &this->m_ListenerId);

CleanUp:
    if (!NT_SUCCESS(status))
    {
        this->m_EventBus = nullptr;
        this->m_ListenerId = {};
    }
    return status;
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::IPlugin::Unregister                                               |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
void XPF_API
SysMon::IPlugin::Unregister(
    void
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (nullptr != this->m_EventBus)
    {
        SysMonLogInfo("Unregistering plugin event listener with id %I64d and with registration id = %!GUID! from bus %p...",
                      this->m_PluginId,
                      &this->m_ListenerId,
                      this->m_EventBus);

        status = (*this->m_EventBus).UnregisterListener(this->m_ListenerId);
        if (!NT_SUCCESS(status))
        {
            XPF_ASSERT(false);
            SysMonLogError("Plugin with id %I64d could not be unregistered! Registration id = %!GUID! status = %!STATUS!",
                           this->m_PluginId,
                           &this->m_ListenerId,
                           status);
        }
        else
        {
            SysMonLogInfo("Successfully unregistered plugin with id %I64d and with registration id = %!GUID! from bus %p",
                          this->m_PluginId,
                          &this->m_ListenerId,
                          this->m_EventBus);
        }

        this->m_EventBus = nullptr;
    }
}

//
// -------------------------------------------------------------------------------------------------------------------
// | ****************************************************************************************************************|
// |                                       SysMon::PluginManager::Create                                             |
// | ****************************************************************************************************************|
// -------------------------------------------------------------------------------------------------------------------
//

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::PluginManager::Create(
    _Inout_ xpf::Optional<SysMon::PluginManager>& PluginManager,
    _Inout_ xpf::EventBus* EventBus
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    XPF_DEATH_ON_FAILURE(nullptr != EventBus);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    SysMonLogInfo("Creating the Plugin Manager...");

    //
    // Allocate space for the plugin manager.
    //
    PluginManager.Emplace();
    SysMon::PluginManager& pluginManager = (*PluginManager);

    #ifndef DOXYGEN_SHOULD_SKIP_THIS
    #define SYSMON_PLUGIN_MANAGER_MAKE_PLUGIN(PluginType, PluginId)                                                     /* NOLINT(*) */  \
    {                                                                                                                   /* NOLINT(*) */  \
        xpf::SharedPointer<IPlugin> instance {SYSMON_NPAGED_ALLOCATOR};                                                 /* NOLINT(*) */  \
                                                                                                                        /* NOLINT(*) */  \
        status = PluginType::Create(instance, PluginId);                                                                /* NOLINT(*) */  \
        if(!NT_SUCCESS(status))                                                                                         /* NOLINT(*) */  \
        {                                                                                                               /* NOLINT(*) */  \
            goto CleanUp;                                                                                               /* NOLINT(*) */  \
        }                                                                                                               /* NOLINT(*) */  \
                                                                                                                        /* NOLINT(*) */  \
        status = pluginManager.m_Plugins.Emplace(instance);                                                             /* NOLINT(*) */  \
        if(!NT_SUCCESS(status))                                                                                         /* NOLINT(*) */  \
        {                                                                                                               /* NOLINT(*) */  \
            goto CleanUp;                                                                                               /* NOLINT(*) */  \
        }                                                                                                               /* NOLINT(*) */  \
                                                                                                                        /* NOLINT(*) */  \
        status = (*instance).Register(EventBus);                                                                        /* NOLINT(*) */  \
        if(!NT_SUCCESS(status))                                                                                         /* NOLINT(*) */  \
        {                                                                                                               /* NOLINT(*) */  \
            goto CleanUp;                                                                                               /* NOLINT(*) */  \
        }                                                                                                               /* NOLINT(*) */  \
    }
    #endif  // DOXYGEN_SHOULD_SKIP_THIS

    //
    // From this point on, we just create plugins.
    //
    SYSMON_PLUGIN_MANAGER_MAKE_PLUGIN(SysMon::UmHookPlugin, 0);
    SYSMON_PLUGIN_MANAGER_MAKE_PLUGIN(SysMon::RpcAlpcInspectionPlugin, 1);

    //
    // This macro is no longer needed - we created all plugins.
    //
    #undef SYSMON_PLUGIN_MANAGER_MAKE_PLUGIN

    //
    // If we reached this point, all good.
    //
    status = STATUS_SUCCESS;
    SysMonLogInfo("Successfully created plugin manager!");

CleanUp:
    if (!NT_SUCCESS(status))
    {
        SysMonLogError("Creating the plugin manager failed with status %!STATUS!",
                       status);

        PluginManager.Reset();
    }
    return status;
}
