/**
 * @file        ALPC-Tools/AlpcMon_Sys/precomp.hpp
 *
 * @brief       In this file we define the precompiled headers
 *              used throughout the project.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#pragma once

#include <xpf_lib/xpf.hpp>      // NOLINT(*)
#include <ntimage.h>            // NOLINT(*)
#include <bcrypt.h>             // NOLINT(*)

/**
 * @brief   The default allocator to be used when dealing with paged allocations.
 */
#define SYSMON_PAGED_ALLOCATOR      xpf::PolymorphicAllocator{ .AllocFunction = &xpf::SplitAllocator::AllocateMemory,        \
                                                               .FreeFunction = &xpf::SplitAllocator::FreeMemory }

/**
 * @brief   The default allocator to be used when dealing with non paged allocations.
 */
#define SYSMON_NPAGED_ALLOCATOR     xpf::PolymorphicAllocator{ .AllocFunction = &xpf::SplitAllocatorCritical::AllocateMemory, \
                                                               .FreeFunction = &xpf::SplitAllocatorCritical::FreeMemory }
