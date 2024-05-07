/**
 * @file        ALPC-Tools/AlpcMon_Sys/trace.hpp
 *
 * @brief       Definitions for wpp tracing.
 *
 * @author      Andrei-Marius MUNTEA (munteaandrei17@gmail.com)
 *
 * @copyright   Copyright © Andrei-Marius MUNTEA 2020-2024.
 *              All rights reserved.
 *
 * @license     See top-level directory LICENSE file.
 */

#pragma once

/**
 * @brief   We are compiling with cpp - so the code generated will be c-like.
 */
EXTERN_C_START

/**
 * @brief   Sysmon GUID provider : {1AD0EF60-DD8A-496A-A96C-D1FC61C49D3D}
 */
#define WPP_CONTROL_GUIDS  WPP_DEFINE_CONTROL_GUID(SysmonTraceGuid,                                                             \
                                                   (1AD0EF60, DD8A, 496A, A96C, D1FC61C49D3D),                                  \
                                                   WPP_DEFINE_BIT(TRACE_FLAG_SYSMON_CORE)           /* bit  0 = 0x00000001 */   \
                                                  )

/**
 * @brief   WPP required macro to define flags for our logger.
 */
#define WPP_LEVEL_FLAGS_LOGGER(level, flags)    WPP_LEVEL_LOGGER(flags)

/**
 * @brief   WPP required macro to check whether a specific flag is enabled or not.
 */
#define WPP_LEVEL_FLAGS_ENABLED(level, flags)   (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= level)


// begin_wpp config
//
// FUNC SysMonLogTrace{    LEVEL=TRACE_LEVEL_VERBOSE,      FLAGS=TRACE_FLAG_SYSMON_CORE}(MSG, ...);
// FUNC SysMonLogInfo{     LEVEL=TRACE_LEVEL_INFORMATION,  FLAGS=TRACE_FLAG_SYSMON_CORE}(MSG, ...);
// FUNC SysMonLogWarning{  LEVEL=TRACE_LEVEL_WARNING,      FLAGS=TRACE_FLAG_SYSMON_CORE}(MSG, ...);
// FUNC SysMonLogError{    LEVEL=TRACE_LEVEL_ERROR,        FLAGS=TRACE_FLAG_SYSMON_CORE}(MSG, ...);
// FUNC SysMonLogCritical{ LEVEL=TRACE_LEVEL_CRITICAL,     FLAGS=TRACE_FLAG_SYSMON_CORE}(MSG, ...);
//
// end_wpp
//

/**
 * @brief   Useful wrapper macro to stringify an expression.
 *          It is required so we can use it with another macro.
 */
#define TMH_STRINGIFY_EXPRESSION(exp)   #exp

/**
 * @brief   Useful macro to stringify an expression.
 *          This will just call TMH_STRINGIFY_EXPRESSION.
 */
#define TMH_STRINGIFY(x)                TMH_STRINGIFY_EXPRESSION(x)

/**
 * @brief   Include the TMH related file.
 */
#ifdef TMH_FILE
    #ifndef DOXYGEN_SHOULD_SKIP_THIS
        #include TMH_STRINGIFY(TMH_FILE)
    #endif  // DOXYGEN_SHOULD_SKIP_THIS
#endif  // TMH_FILE

EXTERN_C_END
