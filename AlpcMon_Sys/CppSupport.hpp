/**
 * @file        ALPC-Tools/AlpcMon_Sys/CppSupport.hpp
 *
 * @brief       Header file with routines to control cpp support in km.
 *              The file is taken as is from the test driver used by xpf lib.
 *              For more details see the original:
 *                  xpf_tests\win_km_build\CppSupport\CppSupport.hpp 
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

/**
 * @brief CPP support was not yet initialized.
 *        ensure these APIs are not name mangled
 *        so the KM compiler will recognize them.
 *
 */
XPF_EXTERN_C_START();


/**
 * @brief This will be called in driver entry routine and is used to
 *        initialize the cpp support in KM.
 *
 *
 * @return STATUS_SUCCESS if everything went good, or STATUS_NOT_SUPPORTED if not.
 *         In such scenarios it means something went wrong with the build
 *         and internal pointers are considered invalid.
 *         You should check the compile options and ensure everthing is in order.
 *
 */
_Must_inspect_result_
NTSTATUS
XPF_API
XpfInitializeCppSupport(
    void
);

/**
 * @brief This will be called in driver unload routine and is used to
 *        deinitialize the cpp support in KM.
 *        Do not construct any other objects or use any cpp after this is called.
 *
 *
 * @return Nothing.
 */
void
XPF_API
XpfDeinitializeCppSupport(
    void
);

/**
 * @brief Don't add anything after this macro.
 *        Stop the C-Specific section here.
 */
XPF_EXTERN_C_END();
