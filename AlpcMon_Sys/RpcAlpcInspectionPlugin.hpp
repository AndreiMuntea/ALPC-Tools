/**
 * @file        ALPC-Tools/AlpcMon_Sys/RpcAlpcInspectionPlugin.hpp
 *
 * @brief       This is the plugin responsible with the actual inspection
 *              of RPC messages coming from UM hooking component.
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
#include "PluginManager.hpp"

namespace SysMon
{
/**
 * @brief   This class will be notified about the RPC message
 *          from the user mode component.
 *          For now it just distinguish between different types
 *          of actions that can happen and logs the message.
 */
class RpcAlpcInspectionPlugin final : public IPlugin
{
 private:
    /**
     * @brief       Default constructor.
     *
     * @param[in]   PluginId - the associated id of this plugin.
     *                         Caller must ensure this is unique so
     *                         it can properly identify the plugin.
     *
     * @note        Please use Create method!
     */
    RpcAlpcInspectionPlugin(
        _In_ const uint64_t& PluginId
    ) noexcept(true) : IPlugin(PluginId)
    {
        XPF_NOTHING();
    }

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~RpcAlpcInspectionPlugin(void) noexcept(true) = default;

    /**
     * @brief   A plugin can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::RpcAlpcInspectionPlugin, delete);

    /**
     * @brief          This method is used to create the plugin.
     *
     * @param[in,out]  Plugin          - will contain the newly created plugin instance.
     *
     * @param[in]      PluginId        - the associated id of this plugin.
     *                                   Caller must ensure this is unique so
     *                                   it can properly identify the plugin.
     *
     * @return         A proper NTSTATUS error code.
     *
     * @note           Even if the plugin resources are non paged,
     *                 this routine is intended to be called at passive level (from driver entry).
     */
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    static NTSTATUS XPF_API
    Create(
        _Inout_ xpf::SharedPointer<SysMon::IPlugin, xpf::CriticalMemoryAllocator>& Plugin,
        _In_ const uint64_t& PluginId
    ) noexcept(true);

    /**
     * @brief                 This method is used to generically handle an event.
     *                        It will be automatically invoked for each listener
     *                        registered with the event bus.
     * 
     * @param[in,out] Event - A reference to the event.
     *                        Its internal data should not be modified by the event handler.
     *                        It is the caller responsibility to downcast this to the proper event class.
     * 
     * @param[in,out] Bus   - The event bus where this particular event has been thrown to.
     *                        It has strong guarantees that the bus will be valid until the OnEvent() is called.
     *                        Can be safely used to throw new events from the OnEvent() method itself.
     *
     * @return - void.
     */
    void XPF_API
    OnEvent(
        _Inout_ xpf::IEvent* Event,
        _Inout_ xpf::EventBus* Bus
    ) noexcept(true) override;

 private:
    /**
     * @brief               This method is used to handle a message dispatch from a user mode hook.
     * 
     * @param[in] Event     - A const reference to the event.
     *
     * @return              - void.
     */
    void XPF_API
    OnUmHookEvent(
        _In_ const xpf::IEvent* Event
    ) noexcept(true);

 private:
     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // RpcAlpcInspectionPlugin
};  // namespace SysMon
