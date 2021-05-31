/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM typec
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_TYPEC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_TYPEC_H
#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_typec_tcpm_log,
	     TP_PROTO(const char *log, bool *bypass),
	     TP_ARGS(log, bypass));

#endif /* _TRACE_HOOK_TYPEC_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
