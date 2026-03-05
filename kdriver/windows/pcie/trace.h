/**
 * @file trace.h
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief Header file for the debug tracing related function defintions and macros.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */

//
// Define the tracing flags.
//
// Tracing GUID - 8fc7470d-8b92-4676-a374-315183670128
//
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(MEMXTraceGuid, (8fc7470d,8b92,4676,a374,315183670128), \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                       /* bit  0 = 0x00000001 */ \
        WPP_DEFINE_BIT(TRACE_DRIVER)                            /* bit  1 = 0x00000002 */ \
        WPP_DEFINE_BIT(TRACE_DEVICE)                            /* bit  2 = 0x00000004 */ \
        WPP_DEFINE_BIT(TRACE_QUEUE)                             /* bit  3 = 0x00000008 */ \
        /* You can have up to 32 defines. If you want more than that,                     \
            you have to provide another trace control GUID */                             \
        )

#define WPP_FLAG_LEVEL_LOGGER(flag, level)  WPP_LEVEL_LOGGER(flag)
#define WPP_FLAG_LEVEL_ENABLED(flag, level) (WPP_LEVEL_ENABLED(flag) && WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)
#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

//
// WPP orders static parameters before dynamic parameters. To support the Trace function
// defined below which sets FLAGS=MYDRIVER_ALL_INFO, a custom macro must be defined to
// reorder the arguments to what the .tpl configuration file expects.
//
#define WPP_RECORDER_FLAGS_LEVEL_ARGS(flags, lvl) WPP_RECORDER_LEVEL_FLAGS_ARGS(lvl, flags)
#define WPP_RECORDER_FLAGS_LEVEL_FILTER(flags, lvl) WPP_RECORDER_LEVEL_FLAGS_FILTER(lvl, flags)

//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
// FUNC Trace{FLAGS=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// end_wpp
//