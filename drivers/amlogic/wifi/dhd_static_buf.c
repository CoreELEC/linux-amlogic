/*
 * drivers/amlogic/wifi/dhd_static_buf.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt)	"Wifi: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/amlogic/dhd_buf.h>

#define	DHD_STATIC_VERSION_STR		"101.10.361.10 (wlan=r892223-20210623-1)"
#define STATIC_ERROR_LEVEL	BIT(0)
#define STATIC_TRACE_LEVEL	BIT(1)
#define STATIC_MSG_LEVEL	BIT(0)
uint static_msg_level = STATIC_ERROR_LEVEL | STATIC_MSG_LEVEL;

#define DHD_STATIC_MSG(x, args...) \
do { \
	if (static_msg_level & STATIC_MSG_LEVEL) { \
		pr_err("[dhd] STATIC-MSG) %s : " x, __func__, ## args); \
	} \
} while (0)
#define DHD_STATIC_ERROR(x, args...) \
do { \
	if (static_msg_level & STATIC_ERROR_LEVEL) { \
		pr_err("[dhd] STATIC-ERROR) %s : " x, __func__, ## args); \
	} \
} while (0)
#define DHD_STATIC_TRACE(x, args...) \
do { \
	if (static_msg_level & STATIC_TRACE_LEVEL) { \
		pr_err("[dhd] STATIC-TRACE) %s : " x, __func__, ## args); \
	} \
} while (0)

#define BCMDHD_SDIO
#define BCMDHD_PCIE
//#define BCMDHD_USB
#define CONFIG_BCMDHD_VTS { : = y}
#define CONFIG_BCMDHD_DEBUG { : = y}
//#define BCMDHD_UNUSE_MEM

enum dhd_prealloc_index {
	DHD_PREALLOC_PROT = 0,
#if defined(BCMDHD_SDIO)
	DHD_PREALLOC_RXBUF = 1,
	DHD_PREALLOC_DATABUF = 2,
#endif /* BCMDHD_SDIO */
	DHD_PREALLOC_OSL_BUF = 3,
	DHD_PREALLOC_SKB_BUF = 4,
	DHD_PREALLOC_WIPHY_ESCAN0 = 5,
	DHD_PREALLOC_WIPHY_ESCAN1 = 6,
	DHD_PREALLOC_DHD_INFO = 7,
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
	DHD_PREALLOC_DHD_WLFC_INFO = 8,
#endif /* BCMDHD_SDIO | BCMDHD_USB */
#ifdef BCMDHD_PCIE
	DHD_PREALLOC_IF_FLOW_LKUP = 9,
#endif /* BCMDHD_PCIE */
	DHD_PREALLOC_MEMDUMP_BUF = 10,
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
	DHD_PREALLOC_MEMDUMP_RAM = 11,
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
	DHD_PREALLOC_DHD_WLFC_HANGER = 12,
#endif /* BCMDHD_SDIO | BCMDHD_USB */
	DHD_PREALLOC_PKTID_MAP = 13,
	DHD_PREALLOC_PKTID_MAP_IOCTL = 14,
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
	DHD_PREALLOC_DHD_LOG_DUMP_BUF = 15,
	DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX = 16,
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
	DHD_PREALLOC_DHD_PKTLOG_DUMP_BUF = 17,
	DHD_PREALLOC_STAT_REPORT_BUF = 18,
	DHD_PREALLOC_WL_ESCAN = 19,
	DHD_PREALLOC_FW_VERBOSE_RING = 20,
	DHD_PREALLOC_FW_EVENT_RING = 21,
	DHD_PREALLOC_DHD_EVENT_RING = 22,
#if defined(BCMDHD_UNUSE_MEM)
	DHD_PREALLOC_NAN_EVENT_RING = 23,
#endif /* BCMDHD_UNUSE_MEM */
	DHD_PREALLOC_MAX
};

#define STATIC_BUF_MAX_NUM	20
#define STATIC_BUF_SIZE	(PAGE_SIZE * 2)

#ifndef CUSTOM_LOG_DUMP_BUFSIZE_MB
/* DHD_LOG_DUMP_BUF_SIZE 4 MB static memory in kernel */
#define CUSTOM_LOG_DUMP_BUFSIZE_MB	4
#endif /* CUSTOM_LOG_DUMP_BUFSIZE_MB */

#define DHD_PREALLOC_PROT_SIZE	(16 * 1024)
#define DHD_PREALLOC_RXBUF_SIZE	(24 * 1024)
#define DHD_PREALLOC_DATABUF_SIZE	(64 * 1024)
#define DHD_PREALLOC_OSL_BUF_SIZE	(STATIC_BUF_MAX_NUM * STATIC_BUF_SIZE)
#define DHD_PREALLOC_WIPHY_ESCAN0_SIZE	(64 * 1024)
#define DHD_PREALLOC_DHD_INFO_SIZE	(34 * 1024)
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
#define DHD_PREALLOC_MEMDUMP_RAM_SIZE	(1290 * 1024)
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
#define DHD_PREALLOC_DHD_WLFC_HANGER_SIZE	(73 * 1024)
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
#define DHD_PREALLOC_DHD_LOG_DUMP_BUF_SIZE \
	(1024 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#define DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX_SIZE (8 * 1024)
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
#define DHD_PREALLOC_WL_ESCAN_SIZE	(70 * 1024)
#ifdef CONFIG_64BIT
#define DHD_PREALLOC_IF_FLOW_LKUP_SIZE	(20 * 1024 * 2)
#else
#define DHD_PREALLOC_IF_FLOW_LKUP_SIZE	(20 * 1024)
#endif
#define FW_VERBOSE_RING_SIZE		(256 * 1024)
#define FW_EVENT_RING_SIZE		(64 * 1024)
#define DHD_EVENT_RING_SIZE		(64 * 1024)
#define NAN_EVENT_RING_SIZE		(64 * 1024)

#if defined(CONFIG_64BIT)
#define WLAN_DHD_INFO_BUF_SIZE	(24 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE	(64 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE	(64 * 1024)
#else
#define WLAN_DHD_INFO_BUF_SIZE	(16 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE	(64 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE	(20 * 1024)
#endif /* CONFIG_64BIT */
#define WLAN_DHD_MEMDUMP_SIZE	(800 * 1024)

#define DHD_SKB_1PAGE_BUFSIZE	(PAGE_SIZE * 1)
#define DHD_SKB_2PAGE_BUFSIZE	(PAGE_SIZE * 2)
#define DHD_SKB_4PAGE_BUFSIZE	(PAGE_SIZE * 4)

#define DHD_SKB_1PAGE_BUF_NUM	8
#ifdef BCMDHD_PCIE
#define DHD_SKB_2PAGE_BUF_NUM	192
#elif defined(BCMDHD_SDIO)
#define DHD_SKB_2PAGE_BUF_NUM	8
#endif /* BCMDHD_PCIE */
#define DHD_SKB_4PAGE_BUF_NUM	1

/* The number is defined in linux_osl.c
 * WLAN_SKB_1_2PAGE_BUF_NUM => STATIC_PKT_1_2PAGE_NUM
 * WLAN_SKB_BUF_NUM => STATIC_PKT_MAX_NUM
 */
#if defined(BCMDHD_SDIO) || defined(BCMDHD_PCIE)
#define WLAN_SKB_1_2PAGE_BUF_NUM ((DHD_SKB_1PAGE_BUF_NUM) + \
		(DHD_SKB_2PAGE_BUF_NUM))
#define WLAN_SKB_BUF_NUM ((WLAN_SKB_1_2PAGE_BUF_NUM) + (DHD_SKB_4PAGE_BUF_NUM))
#endif

void *wlan_static_prot;
void *wlan_static_rxbuf;
void *wlan_static_databuf;
void *wlan_static_osl_buf;
void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *wlan_static_dhd_info_buf;
void *wlan_static_dhd_wlfc_info_buf;
void *wlan_static_if_flow_lkup;
void *wlan_static_dhd_memdump_ram_buf;
void *wlan_static_dhd_wlfc_hanger_buf;
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
void *wlan_static_dhd_log_dump_buf;
void *wlan_static_dhd_log_dump_buf_ex;
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
void *wlan_static_wl_escan_info_buf;
void *wlan_static_fw_verbose_ring_buf;
void *wlan_static_fw_event_ring_buf;
void *wlan_static_dhd_event_ring_buf;
void *wlan_static_nan_event_ring_buf;

#if defined(BCMDHD_SDIO) || defined(BCMDHD_PCIE)
static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];
#endif /* BCMDHD_SDIO | BCMDHD_PCIE */

void *bcmdhd_mem_prealloc(int section, unsigned long size)
{
	DHD_STATIC_TRACE("sectoin %d, size %ld\n", section, size);
	if (section == DHD_PREALLOC_PROT)
		return wlan_static_prot;

#if defined(BCMDHD_SDIO)
	if (section == DHD_PREALLOC_RXBUF)
		return wlan_static_rxbuf;

	if (section == DHD_PREALLOC_DATABUF)
		return wlan_static_databuf;
#endif /* BCMDHD_SDIO */

#if defined(BCMDHD_SDIO) || defined(BCMDHD_PCIE)
	if (section == DHD_PREALLOC_SKB_BUF)
		return wlan_static_skb;
#endif /* BCMDHD_SDIO | BCMDHD_PCIE */

	if (section == DHD_PREALLOC_WIPHY_ESCAN0)
		return wlan_static_scan_buf0;

	if (section == DHD_PREALLOC_WIPHY_ESCAN1)
		return wlan_static_scan_buf1;

	if (section == DHD_PREALLOC_OSL_BUF) {
		if (size > DHD_PREALLOC_OSL_BUF_SIZE) {
			DHD_STATIC_ERROR("request OSL_BUF(%lu) > %ld\n",
				size, DHD_PREALLOC_OSL_BUF_SIZE);
			return NULL;
		}
		return wlan_static_osl_buf;
	}

	if (section == DHD_PREALLOC_DHD_INFO) {
		if (size > DHD_PREALLOC_DHD_INFO_SIZE) {
			DHD_STATIC_ERROR("request DHD_INFO(%lu) > %d\n",
				size, DHD_PREALLOC_DHD_INFO_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
	if (section == DHD_PREALLOC_DHD_WLFC_INFO) {
		if (size > WLAN_DHD_WLFC_BUF_SIZE) {
			DHD_STATIC_ERROR("request DHD_WLFC_INFO(%lu) > %d\n",
				size, WLAN_DHD_WLFC_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_info_buf;
	}
#endif /* BCMDHD_SDIO | BCMDHD_USB */
#ifdef BCMDHD_PCIE
	if (section == DHD_PREALLOC_IF_FLOW_LKUP)  {
		if (size > DHD_PREALLOC_IF_FLOW_LKUP_SIZE) {
			DHD_STATIC_ERROR("request DHD_IF_FLOW_LKUP(%lu) > %d\n",
				size, DHD_PREALLOC_IF_FLOW_LKUP_SIZE);
			return NULL;
		}
		return wlan_static_if_flow_lkup;
	}
#endif /* BCMDHD_PCIE */
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
	if (section == DHD_PREALLOC_MEMDUMP_RAM) {
		if (size > DHD_PREALLOC_MEMDUMP_RAM_SIZE) {
			DHD_STATIC_ERROR("request MEMDUMP_RAM(%lu) > %d\n",
				size, DHD_PREALLOC_MEMDUMP_RAM_SIZE);
			return NULL;
		}
		return wlan_static_dhd_memdump_ram_buf;
	}
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
	if (section == DHD_PREALLOC_DHD_WLFC_HANGER) {
		if (size > DHD_PREALLOC_DHD_WLFC_HANGER_SIZE) {
			DHD_STATIC_ERROR("request DHD_WLFC_HANGER(%lu) > %d\n",
				size, DHD_PREALLOC_DHD_WLFC_HANGER_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_hanger_buf;
	}
#endif /* BCMDHD_SDIO | BCMDHD_USB */
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
	if (section == DHD_PREALLOC_DHD_LOG_DUMP_BUF) {
		if (size > DHD_PREALLOC_DHD_LOG_DUMP_BUF_SIZE) {
			DHD_STATIC_ERROR("request DHD_LOG_DUMP_BUF(%lu) > %d\n",
				size, DHD_PREALLOC_DHD_LOG_DUMP_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_log_dump_buf;
	}
	if (section == DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX) {
		if (size > DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX_SIZE) {
			DHD_STATIC_ERROR("request DUMP_BUF_EX(%lu) > %d\n",
				size, DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX_SIZE);
			return NULL;
		}
		return wlan_static_dhd_log_dump_buf_ex;
	}
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
	if (section == DHD_PREALLOC_WL_ESCAN) {
		if (size > DHD_PREALLOC_WL_ESCAN_SIZE) {
			DHD_STATIC_ERROR("request WL_ESCAN(%lu) > %d\n",
				size, DHD_PREALLOC_WL_ESCAN_SIZE);
			return NULL;
		}
		return wlan_static_wl_escan_info_buf;
	}
	if (section == DHD_PREALLOC_FW_VERBOSE_RING) {
		if (size > FW_VERBOSE_RING_SIZE) {
			DHD_STATIC_ERROR("request FW_VERBOSE_RING(%lu) > %d\n",
				size, FW_VERBOSE_RING_SIZE);
			return NULL;
		}
		return wlan_static_fw_verbose_ring_buf;
	}
	if (section == DHD_PREALLOC_FW_EVENT_RING) {
		if (size > FW_EVENT_RING_SIZE) {
			DHD_STATIC_ERROR("request FW_EVENT_RING(%lu) > %d\n",
				size, FW_EVENT_RING_SIZE);
			return NULL;
		}
		return wlan_static_fw_event_ring_buf;
	}
	if (section == DHD_PREALLOC_DHD_EVENT_RING) {
		if (size > DHD_EVENT_RING_SIZE) {
			DHD_STATIC_ERROR("request DHD_EVENT_RING(%lu) > %d\n",
				size, DHD_EVENT_RING_SIZE);
			return NULL;
		}
		return wlan_static_dhd_event_ring_buf;
	}
#if defined(BCMDHD_UNUSE_MEM)
	if (section == DHD_PREALLOC_NAN_EVENT_RING) {
		if (size > NAN_EVENT_RING_SIZE) {
			DHD_STATIC_ERROR("request DHD_NAN_RING(%lu) > %d\n",
				size, NAN_EVENT_RING_SIZE);
			return NULL;
		}
		return wlan_static_nan_event_ring_buf;
	}
#endif /* BCMDHD_UNUSE_MEM */
	if (section < 0 || section > DHD_PREALLOC_MAX)
		DHD_STATIC_ERROR("request section id(%d) is out of max %d\n",
			section, DHD_PREALLOC_MAX);

	DHD_STATIC_ERROR("failed to alloc section %d, size=%ld\n",
		section, size);

	return NULL;
}
EXPORT_SYMBOL(bcmdhd_mem_prealloc);

int bcmdhd_init_wlan_mem(unsigned int all_buf)
{
#if defined(BCMDHD_SDIO) || defined(BCMDHD_PCIE)
	int i;
#endif
	unsigned long size = 0;

	DHD_STATIC_MSG("%s\n", DHD_STATIC_VERSION_STR);

#if defined(BCMDHD_SDIO) || defined(BCMDHD_PCIE)
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++)
		wlan_static_skb[i] = NULL;

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;

		size += DHD_SKB_1PAGE_BUFSIZE;
		DHD_STATIC_TRACE("sectoin %d skb[%d], size=%ld\n",
			DHD_PREALLOC_SKB_BUF, i, DHD_SKB_1PAGE_BUFSIZE);
	}

	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;

		size += DHD_SKB_2PAGE_BUFSIZE;
		DHD_STATIC_TRACE("sectoin %d skb[%d], size=%ld\n",
			DHD_PREALLOC_SKB_BUF, i, DHD_SKB_2PAGE_BUFSIZE);
	}
#endif /* BCMDHD_SDIO | BCMDHD_PCIE */
	if (all_buf == 1) {
#if defined(BCMDHD_SDIO)
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
		size += DHD_SKB_4PAGE_BUFSIZE;
		DHD_STATIC_TRACE("sectoin %d skb[%d], size=%ld\n",
			DHD_PREALLOC_SKB_BUF, i, DHD_SKB_4PAGE_BUFSIZE);
#endif /* BCMDHD_SDIO */

		wlan_static_prot = kmalloc(DHD_PREALLOC_PROT_SIZE, GFP_KERNEL);
		if (!wlan_static_prot)
			goto err_mem_alloc;
		size += DHD_PREALLOC_PROT_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_PROT, DHD_PREALLOC_PROT_SIZE);

#if defined(BCMDHD_SDIO)
		wlan_static_rxbuf =
			kmalloc(DHD_PREALLOC_RXBUF_SIZE, GFP_KERNEL);
		if (!wlan_static_rxbuf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_RXBUF_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_RXBUF, DHD_PREALLOC_RXBUF_SIZE);

		wlan_static_databuf =
			kmalloc(DHD_PREALLOC_DATABUF_SIZE, GFP_KERNEL);
		if (!wlan_static_databuf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_DATABUF_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DATABUF, DHD_PREALLOC_DATABUF_SIZE);
#endif /* BCMDHD_SDIO */

		wlan_static_osl_buf =
			kmalloc(DHD_PREALLOC_OSL_BUF_SIZE, GFP_KERNEL);
		if (!wlan_static_osl_buf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_OSL_BUF_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%ld\n",
			DHD_PREALLOC_OSL_BUF, DHD_PREALLOC_OSL_BUF_SIZE);

		wlan_static_scan_buf0 =
			kmalloc(DHD_PREALLOC_WIPHY_ESCAN0_SIZE, GFP_KERNEL);
		if (!wlan_static_scan_buf0)
			goto err_mem_alloc;
		size += DHD_PREALLOC_WIPHY_ESCAN0_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_WIPHY_ESCAN0,
			DHD_PREALLOC_WIPHY_ESCAN0_SIZE);

		wlan_static_dhd_info_buf =
			kmalloc(DHD_PREALLOC_DHD_INFO_SIZE, GFP_KERNEL);
		if (!wlan_static_dhd_info_buf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_DHD_INFO_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DHD_INFO, DHD_PREALLOC_DHD_INFO_SIZE);

#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
		wlan_static_dhd_wlfc_info_buf =
			kmalloc(WLAN_DHD_WLFC_BUF_SIZE, GFP_KERNEL);
		if (!wlan_static_dhd_wlfc_info_buf)
			goto err_mem_alloc;
		size += WLAN_DHD_WLFC_BUF_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DHD_WLFC_INFO, WLAN_DHD_WLFC_BUF_SIZE);
#endif /* BCMDHD_SDIO | BCMDHD_USB */

#ifdef BCMDHD_PCIE
		wlan_static_if_flow_lkup =
			kmalloc(DHD_PREALLOC_IF_FLOW_LKUP_SIZE, GFP_KERNEL);
		if (!wlan_static_if_flow_lkup)
			goto err_mem_alloc;
		size += DHD_PREALLOC_IF_FLOW_LKUP_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_IF_FLOW_LKUP,
			DHD_PREALLOC_IF_FLOW_LKUP_SIZE);
#endif /* BCMDHD_PCIE */
	}

#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
	wlan_static_dhd_memdump_ram_buf =
		kmalloc(DHD_PREALLOC_MEMDUMP_RAM_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_memdump_ram_buf)
		goto err_mem_alloc;
	size += DHD_PREALLOC_MEMDUMP_RAM_SIZE;
	DHD_STATIC_TRACE("sectoin %d, size=%d\n",
		DHD_PREALLOC_MEMDUMP_RAM, DHD_PREALLOC_MEMDUMP_RAM_SIZE);
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
	if (all_buf == 1) {
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
		wlan_static_dhd_wlfc_hanger_buf =
			kmalloc(DHD_PREALLOC_DHD_WLFC_HANGER_SIZE, GFP_KERNEL);
		if (!wlan_static_dhd_wlfc_hanger_buf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_DHD_WLFC_HANGER_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DHD_WLFC_HANGER,
			DHD_PREALLOC_DHD_WLFC_HANGER_SIZE);
#endif /* BCMDHD_SDIO | BCMDHD_USB */

#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
		wlan_static_dhd_log_dump_buf =
			kmalloc(DHD_PREALLOC_DHD_LOG_DUMP_BUF_SIZE, GFP_KERNEL);
		if (!wlan_static_dhd_log_dump_buf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_DHD_LOG_DUMP_BUF_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DHD_LOG_DUMP_BUF,
			DHD_PREALLOC_DHD_LOG_DUMP_BUF_SIZE);

		wlan_static_dhd_log_dump_buf_ex =
			kmalloc(DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX_SIZE,
			GFP_KERNEL);
		if (!wlan_static_dhd_log_dump_buf_ex)
			goto err_mem_alloc;
		size += DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX,
			DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX_SIZE);
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */

		wlan_static_wl_escan_info_buf =
			kmalloc(DHD_PREALLOC_WL_ESCAN_SIZE, GFP_KERNEL);
		if (!wlan_static_wl_escan_info_buf)
			goto err_mem_alloc;
		size += DHD_PREALLOC_WL_ESCAN_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_WL_ESCAN, DHD_PREALLOC_WL_ESCAN_SIZE);
	}
	wlan_static_fw_verbose_ring_buf =
		kmalloc(FW_VERBOSE_RING_SIZE, GFP_KERNEL);
	if (!wlan_static_fw_verbose_ring_buf)
		goto err_mem_alloc;
	size += FW_VERBOSE_RING_SIZE;
	DHD_STATIC_TRACE("sectoin %d, size=%d\n",
		DHD_PREALLOC_FW_VERBOSE_RING, FW_VERBOSE_RING_SIZE);

	if (all_buf == 1) {
		wlan_static_fw_event_ring_buf =
			kmalloc(FW_EVENT_RING_SIZE, GFP_KERNEL);
		if (!wlan_static_fw_event_ring_buf)
			goto err_mem_alloc;
		size += FW_EVENT_RING_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_FW_EVENT_RING, FW_EVENT_RING_SIZE);

		wlan_static_dhd_event_ring_buf =
			kmalloc(DHD_EVENT_RING_SIZE, GFP_KERNEL);
		if (!wlan_static_dhd_event_ring_buf)
			goto err_mem_alloc;
		size += DHD_EVENT_RING_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_DHD_EVENT_RING, DHD_EVENT_RING_SIZE);

#if defined(BCMDHD_UNUSE_MEM)
		wlan_static_nan_event_ring_buf =
			kmalloc(NAN_EVENT_RING_SIZE, GFP_KERNEL);
		if (!wlan_static_nan_event_ring_buf)
			goto err_mem_alloc;
		size += NAN_EVENT_RING_SIZE;
		DHD_STATIC_TRACE("sectoin %d, size=%d\n",
			DHD_PREALLOC_NAN_EVENT_RING, NAN_EVENT_RING_SIZE);
#endif /* BCMDHD_UNUSE_MEM */
	}
	DHD_STATIC_MSG("prealloc ok: %ld(%ldK)\n", size, size / 1024);
	return 0;

err_mem_alloc:
	if (all_buf == 1) {
		kfree(wlan_static_prot);
#if defined(BCMDHD_SDIO)
		kfree(wlan_static_rxbuf);
		kfree(wlan_static_databuf);
#endif /* BCMDHD_SDIO */
		kfree(wlan_static_osl_buf);
		kfree(wlan_static_scan_buf0);
		kfree(wlan_static_scan_buf1);
		kfree(wlan_static_dhd_info_buf);
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
		kfree(wlan_static_dhd_wlfc_info_buf);
#endif /* BCMDHD_SDIO | BCMDHD_USB */
#ifdef BCMDHD_PCIE
		kfree(wlan_static_if_flow_lkup);
#endif /* BCMDHD_PCIE */
	}
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
		kfree(wlan_static_dhd_memdump_ram_buf);
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
	if (all_buf == 1) {
#if defined(BCMDHD_SDIO) || defined(BCMDHD_USB)
		kfree(wlan_static_dhd_wlfc_hanger_buf);
#endif /* BCMDHD_SDIO | BCMDHD_USB */
#if defined(CONFIG_BCMDHD_VTS) || defined(CONFIG_BCMDHD_DEBUG)
		kfree(wlan_static_dhd_log_dump_buf);
		kfree(wlan_static_dhd_log_dump_buf_ex);
#endif /* CONFIG_BCMDHD_VTS | CONFIG_BCMDHD_DEBUG */
		kfree(wlan_static_wl_escan_info_buf);
	}
#ifdef BCMDHD_PCIE
	kfree(wlan_static_fw_verbose_ring_buf);
	if (all_buf == 1) {
		kfree(wlan_static_fw_event_ring_buf);
		kfree(wlan_static_dhd_event_ring_buf);
#if defined(BCMDHD_UNUSE_MEM)
		kfree(wlan_static_nan_event_ring_buf);
#endif /* BCMDHD_UNUSE_MEM */
	}
#endif /* BCMDHD_PCIE */

	DHD_STATIC_ERROR("Failed to mem_alloc for WLAN\n");

#if defined(BCMDHD_SDIO) || defined(BCMDHD_PCIE)
err_skb_alloc:
	DHD_STATIC_ERROR("Failed to skb_alloc for WLAN\n");
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		if (wlan_static_skb[i])
			dev_kfree_skb(wlan_static_skb[i]);
	}
#endif /* BCMDHD_SDIO | BCMDHD_PCIE */

	return -ENOMEM;
}
EXPORT_SYMBOL(bcmdhd_init_wlan_mem);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");
