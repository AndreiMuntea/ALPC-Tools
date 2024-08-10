/**
 * @file        ALPC-Tools/AlpcMon_Sys/StackDecorator.cpp
 *
 * @brief       In this file we define helper api to aid with stack capture
 *              and decoration.
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

#include "ProcessCollector.hpp"
#include "ModuleCollector.hpp"

#include "StackDecorator.hpp"
#include "trace.hpp"


/**
 * @brief   The code can go into paged section.
 */
XPF_SECTION_PAGED;

static NTSTATUS XPF_API
SysMonStackTracePrintFrame(
    _In_ _Const_ const xpf::StringView<wchar_t>& ModuleName,
    _In_ _Const_ const xpf::StringView<char>& FunctioName,
    _In_ _Const_ const uint64_t& OriginalAddress,
    _In_ _Const_ const uint64_t& Offset,
    _Out_ xpf::String<wchar_t>* DecoratedFrame
) noexcept(true)
{
    /* We shouldn't decorate stacks at higher IRQLs*/
    XPF_MAX_PASSIVE_LEVEL();

    /* Preinit output. */
    (*DecoratedFrame).Reset();

    /* Prepare the buffer for printf. */
    xpf::Buffer buffer{ SYSMON_PAGED_ALLOCATOR };
    NTSTATUS status = buffer.Resize(PAGE_SIZE);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    UNICODE_STRING ustrBuffer = { 0 };
    ::RtlInitEmptyUnicodeString(&ustrBuffer,
                                static_cast<PWCHAR>(buffer.GetBuffer()),
                                static_cast<USHORT>(buffer.GetSize()));

    /* Pretty print. */
    status = ::RtlUnicodeStringPrintf(&ustrBuffer,
                                      L"(0x%016llx) -- %s!%S + 0x%llx",
                                      OriginalAddress,
                                      ModuleName.Buffer(),
                                      FunctioName.Buffer(),
                                      Offset);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    /* Save output. */
    xpf::StringView<wchar_t> ustrView;
    status = KmHelper::HelperUnicodeStringToView(ustrBuffer,
                                                 ustrView);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    return DecoratedFrame->Append(ustrView);
}

static NTSTATUS XPF_API
SysMonStackTraceDecorateFrame(
    _In_ xpf::SharedPointer<SysMon::ProcessData>& ProcessData,
    _In_ const void* Frame,
    _Out_ xpf::String<wchar_t>* DecoratedFrame
) noexcept(true)
{
    /* We shouldn't decorate stacks at higher IRQLs*/
    XPF_MAX_PASSIVE_LEVEL();

    const uint64_t address = xpf::AlgoPointerToValue(Frame);
    uint64_t offset = address;

    xpf::SharedPointer<SysMon::ProcessModuleData> processModuleData{ SYSMON_PAGED_ALLOCATOR };
    xpf::SharedPointer<SysMon::ModuleData> moduleData{ SYSMON_PAGED_ALLOCATOR };

    /* Lookup the module containing data. */
    processModuleData = ProcessData.Get()->FindModuleContainingAddress(Frame);
    if (processModuleData.IsEmpty())
    {
        return SysMonStackTracePrintFrame(L"unknown",
                                          "unknown",
                                          address,
                                          offset,
                                          DecoratedFrame);
    }

    /* Offset is now relative to image base of the found module. */
    offset = address - xpf::AlgoPointerToValue(processModuleData.Get()->ModuleBase());

    /* Now we need to find information about the module to go further. */
    moduleData = ModuleCollectorFindModule(processModuleData.Get()->ModulePath());
    if (moduleData.IsEmpty())
    {
        return SysMonStackTracePrintFrame(processModuleData.Get()->ModulePath(),
                                          "imgbase",
                                          address,
                                          offset,
                                          DecoratedFrame);
    }

    /* The symbols are sorted by their RVA, find the closest one smaller than the offset. */
    xpf::Optional<size_t> index;
    const xpf::Vector<xpf::pdb::SymbolInformation>& symbols = moduleData.Get()->ModuleSymbols();

    if (!symbols.IsEmpty())
    {
        size_t lo = 0;
        size_t hi = symbols.Size() - 1;
        while (lo <= hi)
        {
            size_t mid = lo + ((hi - lo) / 2);

            if (symbols[mid].SymbolRVA <= offset)
            {
                index.Emplace(mid);
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
    }

    /* If we could not find a match, we print relative to image base. */
    if (!index.HasValue())
    {
        return SysMonStackTracePrintFrame(processModuleData.Get()->ModulePath(),
                                          "imgbase",
                                          address,
                                          offset,
                                          DecoratedFrame);
    }

    /* Found the symbol - so we adjust. */
    offset = offset - symbols[*index].SymbolRVA;
    return SysMonStackTracePrintFrame(processModuleData.Get()->ModulePath(),
                                      symbols[*index].SymbolName.View(),
                                      address,
                                      offset,
                                      DecoratedFrame);
}

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::StackTraceCapture(
    _Inout_ StackTrace* Trace
) noexcept(true)
{
    XPF_MAX_APC_LEVEL();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    status = xpf::ApiCaptureStackBacktrace(&Trace->Frames[0],
                                           XPF_ARRAYSIZE(Trace->Frames),
                                           &Trace->CapturedFrames);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    Trace->ProcessPid = HandleToUlong(::PsGetCurrentProcessId());
    return status;
}

_Use_decl_annotations_
NTSTATUS XPF_API
SysMon::StackTraceDecorate(
    _Inout_ StackTrace* Trace
) noexcept(true)
{
    XPF_MAX_PASSIVE_LEVEL();

    /* First we need the process and the system process for km modules. */
    xpf::SharedPointer<SysMon::ProcessData> process{ SYSMON_PAGED_ALLOCATOR };
    xpf::SharedPointer<SysMon::ProcessData> systemProcess{ SYSMON_PAGED_ALLOCATOR };

    /* If we can't find the processes, we bail.*/
    process = ProcessCollectorFindProcess(Trace->ProcessPid);
    if (process.IsEmpty())
    {
        return STATUS_NOT_FOUND;
    }
    systemProcess = ProcessCollectorFindProcess(4);
    if (systemProcess.IsEmpty())
    {
        return STATUS_NOT_FOUND;
    }

    /* Now we decorate each frame. */
    for (size_t i = 0; i < Trace->CapturedFrames; ++i)
    {
        xpf::String<wchar_t> decoratedFrame{ SYSMON_PAGED_ALLOCATOR };

        /* Decorate current frame. */
        NTSTATUS status = SysMonStackTraceDecorateFrame(KmHelper::HelperIsUserAddress(Trace->Frames[i]) ? process
                                                                                                        : systemProcess,
                                                        Trace->Frames[i],
                                                        &decoratedFrame);
        if (!NT_SUCCESS(status))
        {
            Trace->DecoratedFrames.Clear();
            return status;
        }

        /* Append it. */
        status = Trace->DecoratedFrames.Emplace(xpf::Move(decoratedFrame));
        if (!NT_SUCCESS(status))
        {
            Trace->DecoratedFrames.Clear();
            return status;
        }
    }

    /* Decorated the frames. */
    return STATUS_SUCCESS;
}
