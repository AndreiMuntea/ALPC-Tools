/**
 * @file        ALPC-Tools/AlpcMon_Sys/PluginManager.hpp
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

#pragma once

#include "precomp.hpp"


namespace SysMon
{

/**
 * @brief   This is the interface for a plugin.
 *          All other plugins will derive from this one.
 *          The plugins have event listening capabilities.
 */
class IPlugin : public xpf::IEventListener
{
 public:
    /**
     * @brief       Default constructor.
     *
     * @param[in]   PluginId - the associated id of this plugin.
     *                         Caller must ensure this is unique so
     *                         it can properly identify the plugin.
     */
    IPlugin(
        _In_ const uint64_t& PluginId
    ) noexcept(true) : xpf::IEventListener(),
                       m_PluginId{PluginId}
    {
        XPF_NOTHING();
    }

    /**
     * @brief   Default destructor - will unregister
     *          the plugin from the bus.
     */
    virtual ~IPlugin(void) noexcept(true)
    {
        this->Unregister();
    }

    /**
     * @brief   A plugin can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::IPlugin, delete);

    /**
     * @brief           Registers the current plugin to the event bus instance.
     *
     * @param[in,out]   EventBus - The event bus instance to register the plugin to.
     *
     * @return          A proper NTSTATUS value.
     *
     * @note            Even if the plugin and resources are non paged,
     *                  this routine is intended to be called at passive level (from driver entry).
     */
    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS XPF_API
    Register(
        _Inout_ xpf::EventBus* EventBus
    ) noexcept(true);

    /**
     * @brief       Unregisters the current plugin to a previously registered bus instance.
     *
     * @return      void.
     *
     * @note        Even if the plugin and resources are non paged,
     *              this routine is intended to be called at passive level (from driver unload).
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    void XPF_API
    Unregister(
        void
    ) noexcept(true);

 protected:
    /**
     * @brief   The event bus reference which will be valid if and only if the plugin was
     *          properly registered to, otherwise it will be nullptr.
     */
    xpf::EventBus* m_EventBus = nullptr;
    /**
     * @brief   The registration id taken from the event bus.
     */
    xpf::EVENT_LISTENER_ID m_ListenerId{};
    /**
     * @brief   The id of the plugin which coresponds to its position in the plugins list from
     *          the plugin manager.
     */
    uint64_t m_PluginId = 0;
};  // class IPlugin

/**
 *  @brief  This is the class responsible for creating all plugins.
 */
class PluginManager final
{
 private:
    /**
     * @brief   The constructor is private as the static create method
     *          must be used instead so we avoid having objects in invalid state.
     */
     PluginManager(void) noexcept(true) = default;

 public:
    /**
     * @brief   The default destructor.
     */
     ~PluginManager(void) noexcept(true) = default;

     /**
      * @brief  We cannot copy nor move this class.
      */
     XPF_CLASS_MOVE_BEHAVIOR(SysMon::PluginManager, delete);

    /**
     * @brief          This method is used to create the plugin manager.
     *
     * @param[in,out]  PluginManager   - an Optional object which will contain the
     *                                   plugin manager along with all plugins.
     *
     * @param[in,out]  EventBus        - the Event Bus instance to register the plugins to.
     *                                   Must be created before this.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           Even if the plugin manager and resources are non paged,
     *                 this routine is intended to be called at passive level (from driver entry).
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::Optional<SysMon::PluginManager>& PluginManager,
        _Inout_ xpf::EventBus* EventBus
    ) noexcept(true);

 private:
     /**
      * @brief  The plugins are simply classes with event listeners capabilities.
      *         They will be created and registered to events
      */
     xpf::Vector<xpf::SharedPointer<IPlugin, xpf::CriticalMemoryAllocator>,
                 xpf::CriticalMemoryAllocator> m_Plugins;
     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // class PluginManager
};  // namespace SysMon
