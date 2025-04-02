/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM cdc_ether

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_CDC_ETHER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_CDC_ETHER_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_is_rndis_enabled,
	TP_PROTO(bool *enable),
	TP_ARGS(enable));
#endif /* _TRACE_HOOK_CDC_ETHER_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
