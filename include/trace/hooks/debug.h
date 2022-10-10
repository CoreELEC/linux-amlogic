/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM debug

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DEBUG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DEBUG_H

#include <trace/hooks/vendor_hooks.h>

#ifdef __GENKSYMS__
#include <asm/ptrace.h>
#endif

struct pt_regs;

DECLARE_HOOK(android_vh_ipi_stop,
	TP_PROTO(struct pt_regs *regs),
	TP_ARGS(regs))

#endif /* _TRACE_HOOK_DEBUG_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
