// SPDX-License-Identifier: GPL-2.0-only
/* vendor_hook.c
 *
 * Android Vendor Hook Support
 *
 * Copyright 2020 Google LLC
 */

#define CREATE_TRACE_POINTS
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/net.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/futex.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/wqlockup.h>
#include <trace/hooks/sysrqcrash.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sys.h>
#include <trace/hooks/ufshcd.h>
#include <trace/hooks/usb.h>
#include <trace/hooks/psci.h>
#include <trace/hooks/ion.h>

/*
 * Export tracepoints that act as a bare tracehook (ie: have no trace event
 * associated with them) to allow external modules to probe them.
 */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_select_task_rq_fair);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_select_task_rq_rt);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_select_fallback_rq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_scheduler_tick);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_enqueue_task);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_dequeue_task);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_can_migrate_task);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_find_lowest_rq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_rtmutex_prepare_setprio);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_prepare_prio_fork);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_finish_prio_fork);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_set_user_nice);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_setscheduler);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_wait_start);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_wait_finish);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_read_wait_start);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_read_wait_finish);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_write_wait_start);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_write_wait_finish);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_sched_show_task);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ptype_head);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_kfree_skb);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_sk_alloc);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_sk_free);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_nf_conn_alloc);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_nf_conn_free);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_arch_set_freq_scale);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_transaction_init);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_set_priority);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_restore_priority);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_init);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_wake);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_write_finished);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_alter_rwsem_list_add);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_alter_futex_plist_add);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ipi_stop);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_wq_lockup_pool);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_find_busiest_group);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_sysrq_crash);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_map_util_freq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_em_pd_energy);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cgroup_set_task);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_syscall_prctl_finished);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_fill_prdt);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_prepare_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_update_sysfs);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_send_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_compl_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_send_uic_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_send_tm_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_check_int_errors);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_usb_persist_overwrite);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_psci_tos_resident_on);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_psci_cpu_suspend);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ion_buffer_release);
