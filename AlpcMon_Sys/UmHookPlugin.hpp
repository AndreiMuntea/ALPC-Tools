/**
 * @file        ALPC-Tools/AlpcMon_Sys/UmHookPlugin.hpp
 *
 * @brief       This is the plugin responsible with UM Hooking component.
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
 * @brief   Describes the injection state for a given process.
 *          Once all dlls specified in RequiredDlls are loaded,
 *          we can inject our own dll.
 */
struct UmInjectionDllData
{
    /**
     * @brief   The process id.
     */
    uint32_t    ProcessId = 0;

    /**
     * @brief   The required dlls to be loaded.
     */
    uint32_t    RequiredDlls = 0xFFFFFFFF;

    /**
     * @brief   Actually loaded dlls.
     */
    uint32_t    LoadedDlls = 0;

    /**
     * @brief   The flag in which the LoadLibrary must be searched.
     *          We always use the matching architecture.
     */
    uint32_t    MatchingDll = 0;

    /**
     * @brief   The address of the routine responsible
     *          with loading our dll - will be resolved when the dll is loaded.
     */
    void*       LoadDllRoutine = nullptr;

    /**
     * @brief   The name of the routine to be searched.
     *          This is what it will be used to load the dll.
     */
    xpf::StringView<char>       LoadDllRoutineName;

    /**
     * @brief   Path of the dll to be injected.
     */
    xpf::StringView<wchar_t>    InjectedDllPath;
};  // struct UmInjectionMetadata

/**
 * @brief   This class will inject a DLL in the UM process using APCs.
 *          It will act on the image load event after all required dlls are
 *          loaded in the victim process.
 */
class UmHookPlugin final : public IPlugin
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
    UmHookPlugin(
        _In_ const uint64_t& PluginId
    ) noexcept(true) : IPlugin(PluginId)
    {
        XPF_NOTHING();
    }

 public:
    /**
     * @brief   Default destructor.
     */
     virtual ~UmHookPlugin(void) noexcept(true) = default;

    /**
     * @brief   A plugin can not be copied, nor moved.
     */
    XPF_CLASS_COPY_MOVE_BEHAVIOR(SysMon::UmHookPlugin, delete);

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
        _Inout_ xpf::SharedPointer<SysMon::IPlugin, xpf::SplitAllocatorCritical>& Plugin,
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
     * @brief               This method is used to handle a process creation event.
     * 
     * @param[in] Event     - A const reference to the event.
     *
     * @return              - void.
     */
    void XPF_API
    OnProcessCreateEvent(
        _In_ const xpf::IEvent* Event
    ) noexcept(true);

    /**
     * @brief               This method is used to handle a process termination event.
     * 
     * @param[in] Event     - A const reference to the event.
     *
     * @return              - void.
     */
    void XPF_API
    OnProcessTerminateEvent(
        _In_ const xpf::IEvent* Event
    ) noexcept(true);

    /**
     * @brief               This method is used to handle an image load event.
     * 
     * @param[in] Event     - A const reference to the event.
     *
     * @return              - void.
     */
    void XPF_API
    OnImageLoadEvent(
        _In_ const xpf::IEvent* Event
    ) noexcept(true);

    /**
     * @brief       Removes the details about injection for a given PID.
     *
     * @param[in]   ProcessId - The id of the process for which the details
     *                          we want to remove.
     *
     * @return      Nothing.
     *
     * @note        It is the caller responsibility to hold the m_ProcessDataLock exclusively.
     */
    void XPF_API
    RemoveInjectionDataForPid(
        _In_ uint32_t ProcessPid
    ) noexcept(true);

    /**
     * @brief       Find the details about injection for a given PID.
     *
     * @param[in]   ProcessId - The id of the process for which the details
     *                          we want to find.
     *
     * @return      nullptr if no details are found, otherwise the injection details
     *              corresponding for the pid.
     *
     * @note        It is the caller responsibility to hold the m_ProcessDataLock.
     */
    SysMon::UmInjectionDllData* XPF_API
    FindInjectionDataForPid(
        _In_ uint32_t ProcessPid
    ) noexcept(true);

 private:
     /**
      * @brief  Holds the state for all processes.
      */
     xpf::Vector<SysMon::UmInjectionDllData, xpf::SplitAllocator> m_ProcessData;
     /**
      * @brief  Guards the process data.
      */
     xpf::Optional<xpf::ReadWriteLock> m_ProcessDataLock;

     /**
      * @brief  On windows 7 we have extra dependencies.
      */
     bool m_IsWindows7 = false;

     /**
      * @brief  The full DOS path of the win32 injection dll. 
      */
     xpf::String<wchar_t, xpf::SplitAllocator> m_UmDllWin32Path;
     /**
      * @brief  The full DOS path of the x64 injection dll. 
      */
     xpf::String<wchar_t, xpf::SplitAllocator> m_UmDllX64Path;

     /**
      * @brief   Default MemoryAllocator is our friend as it requires access to the private
      *          default constructor. It is used in the Create() method to ensure that
      *          no partially constructed objects are created but instead they will be
      *          all fully initialized.
      */
     friend class xpf::MemoryAllocator;
};  // UmHookPlugin
};  // namespace SysMon
