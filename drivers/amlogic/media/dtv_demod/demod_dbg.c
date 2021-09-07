/*
 * drivers/amlogic/media/dtv_demod/demod_dbg.c
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/aml_dtvdemod.h>
#include "dvbs.h"
#include "demod_func.h"
#include "amlfrontend.h"
#include "demod_dbg.h"
#include "dvbt_func.h"
#include "dvbs_diseqc.h"

static unsigned int dtmb_mode;
static unsigned int atsc_mode_para;

static void get_chip_name(struct amldtvdemod_device_s *devp, char *str)
{
	switch (devp->data->hw_ver) {
	case DTVDEMOD_HW_ORG:
		strcpy(str, "DTVDEMOD_HW_ORG");
		break;

	case DTVDEMOD_HW_TXLX:
		strcpy(str, "DTVDEMOD_HW_TXLX");
		break;

	case DTVDEMOD_HW_SM1:
		strcpy(str, "DTVDEMOD_HW_SM1");
		break;

	case DTVDEMOD_HW_TL1:
		strcpy(str, "DTVDEMOD_HW_TL1");
		break;

	case DTVDEMOD_HW_TM2:
		strcpy(str, "DTVDEMOD_HW_TM2");
		break;

	case DTVDEMOD_HW_TM2_B:
		strcpy(str, "DTVDEMOD_HW_TM2_B");
		break;

	case DTVDEMOD_HW_T5:
		strcpy(str, "DTVDEMOD_HW_T5");
		break;

	case DTVDEMOD_HW_T5D:
		strcpy(str, "DTVDEMOD_HW_T5D");
		break;

	case DTVDEMOD_HW_T5D_B:
		strcpy(str, "DTVDEMOD_HW_T5D_B");
		break;

	default:
		strcpy(str, "UNKNOWN");
		break;
	}
}

static int dvbc_fast_search_open(struct inode *inode, struct file *file)
{
	PR_DVBC("dvbc fast channel search Open\n");
	return 0;
}

static int dvbc_fast_search_release(struct inode *inode,
	struct file *file)
{
	PR_DVBC("dvbc fast channel search Release\n");
	return 0;
}

#define BUFFER_SIZE 100
static ssize_t dvbc_fast_search_show(struct file *file,
	char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[BUFFER_SIZE];
	unsigned int len;

	len = snprintf(buf, BUFFER_SIZE, "channel fast search en : %d\n",
		demod_dvbc_get_fast_search());
	/*len += snprintf(buf + len, BUFFER_SIZE - len, "");*/

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t dvbc_fast_search_store(struct file *file,
		const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	char cmd[80], para[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%s %s", cmd, para);

	if (!strcmp(cmd, "fast_search")) {
		PR_INFO("channel fast search: ");

		if (!strcmp(para, "on")) {
			PR_INFO("on\n");
			demod_dvbc_set_fast_search(1);
		} else if (!strcmp(para, "off")) {
			PR_INFO("off\n");
			demod_dvbc_set_fast_search(0);
		}
	}

	return count;
}

static int adc_clk_open(struct inode *inode, struct file *file)
{
	PR_INFO("adc clk Open\n");
	return 0;
}

static int adc_clk_release(struct inode *inode,
	struct file *file)
{
	PR_INFO("adc clk Release\n");
	return 0;
}

#define BUFFER_SIZE 100
static unsigned int adc_clk;
static ssize_t adc_clk_show(struct file *file,
	char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[BUFFER_SIZE];
	unsigned int len;

	len = snprintf(buf, BUFFER_SIZE, "adc clk  sys setting %dM, dbg %dM\n",
		demod_get_adc_clk() / 1000, adc_clk);
	/*len += snprintf(buf + len, BUFFER_SIZE - len, "");*/

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static void adc_clk_set(unsigned int clk)
{
	int nco_rate = 0;

	if (is_meson_tl1_cpu()) {
		if (clk == 24) {
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x012004e0);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x312004e0);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL1_TL1, 0x05400000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL2_TL1, 0xe0800000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x111104e0);
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);
			dtmb_write_reg(0x5b, 0x50a30a25);
			nco_rate = (24000 * 256) / demod_get_sys_clk() + 2;
			adc_clk = 24;
		} else if (clk == 25) {
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x001104c8);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x301104c8);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL1_TL1, 0x03000000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL2_TL1, 0xe1800000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x101104c8);
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x62c1a5);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x131a747d);
			dtmb_write_reg(0x5b, 0x4d6a0a25);
			nco_rate = (25000 * 256) / demod_get_sys_clk() + 2;
			adc_clk = 25;
		} else {
			PR_ERR("wrong setting : adc clk\n");
		}

		if (nco_rate != 0)
			front_write_bits(AFIFO_ADC, nco_rate,
					 AFIFO_NCO_RATE_BIT, AFIFO_NCO_RATE_WID);
	} else {
		PR_ERR("only TL1 has this functionality\n");
	}
}

static ssize_t adc_clk_store(struct file *file,
		const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	char cmd[80], para[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%s %s", cmd, para);

	if (!strcmp(cmd, "adc_clk")) {
		PR_INFO("set adc clk = ");

		if (!strcmp(para, "24")) {
			PR_INFO("24M\n");
			adc_clk_set(24);
		} else if (!strcmp(para, "25")) {
			PR_INFO("25M\n");
			adc_clk_set(25);
		}
	}

	return count;
}

#define DEFINE_SHOW_STORE_DEMOD(__name) \
static const struct file_operations __name ## _fops = {	\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.release = __name ## _release,	\
	.read = __name ## _show,		\
	.write = __name ## _store,	\
}

/*echo fast_search on > /sys/kernel/debug/demod/dvbc_channel_fast*/
DEFINE_SHOW_STORE_DEMOD(dvbc_fast_search);

/*echo adc_clk 24 > /sys/kernel/debug/demod/adc_clk*/
DEFINE_SHOW_STORE_DEMOD(adc_clk);
static void seq_dump_regs(struct seq_file *seq)
{
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	unsigned int reg_start, polling_en = 1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		seq_puts(seq, "demod top start\n");
		for (reg_start = 0; reg_start <= 0xc; reg_start += 4)
			seq_printf(seq, "[0x%x]=0x%x\n", reg_start, demod_top_read_reg(reg_start));

		if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
			seq_printf(seq, "[0x10]=0x%x\n", demod_top_read_reg(DEMOD_TOP_CFG_REG_4));
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		}

		seq_puts(seq, "demod front start\n");
		for (reg_start = 0x20; reg_start <= 0x68; reg_start++)
			seq_printf(seq, "[0x%x]=0x%x\n", reg_start, front_read_reg(reg_start));
	}

	switch (devp->last_delsys) {
	case SYS_ATSC:
	case SYS_ATSCMH:
		if (is_meson_txlx_cpu()) {
			for (reg_start = 0; reg_start <= 0xfff; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, atsc_read_reg(reg_start));
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, atsc_read_reg_v4(reg_start));
		}
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_DVBC_ANNEX_B:
		seq_puts(seq, "dvbc/j83b start\n");
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			for (reg_start = 0; reg_start <= 0xff; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, qam_read_reg(reg_start));
		}
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		polling_en = devp->demod_thread;
		devp->demod_thread = 0;
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);

		if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
			for (reg_start = 0x0; reg_start <= 0xf4; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, dvbt_t2_rdb(reg_start));

			for (reg_start = 0x538; reg_start <= 0xfff; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, dvbt_t2_rdb(reg_start));

			/* TODO: flash weak signal during dump the following regs */
			//for (reg_start = 0x1500; reg_start <= 0x3776; reg_start++)
				//seq_printf(seq, "[0x%x] = 0x%x\n",
					   //reg_start, dvbt_t2_rdb(reg_start));
		}

		if (devp->last_delsys == SYS_DVBT2 && demod_is_t5d_cpu(devp))
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

		devp->demod_thread = polling_en;
		break;

	case SYS_DTMB:
		for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
			seq_printf(seq, "[0x%x] = 0x%x\n",
				   reg_start, dtmb_read_reg(reg_start));
		break;

	case SYS_ISDBT:
		for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
			seq_printf(seq, "[0x%x] = 0x%x\n",
				   reg_start, dvbt_isdbt_rd_reg_new(reg_start));
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		for (reg_start = 0x0; reg_start <= 0xfbf; reg_start++)
			seq_printf(seq, "[0x%x] = 0x%x\n",
				   reg_start, dvbs_rd_byte(reg_start));
		break;

	default:
		seq_puts(seq, "current mode is unknown\n");
		break;
	}

	seq_puts(seq, "\n");
}

static void seq_dump_status(struct seq_file *seq)
{
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	int snr, lock_status, agc_if_gain[3];
	unsigned int ser;
	int strength = 0;
	char chip_name[30];
	struct aml_demod_sts demod_sts;
	unsigned int polling_en = 1;

	strength = tuner_get_ch_power2();
	seq_printf(seq, "version:%s\n", DTVDEMOD_VER);
	seq_printf(seq, "cur delsys: %s\n", dtvdemod_get_cur_delsys(devp->last_delsys));
	seq_printf(seq, "freq: %d\n", devp->freq);
	seq_printf(seq, "demod_thread: %d\n", devp->demod_thread);
	get_chip_name(devp, chip_name);
	seq_printf(seq, "chip: %d, %s\n", devp->data->hw_ver, chip_name);

	switch (devp->last_delsys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_DVBC_ANNEX_B:
		dvbc_status(devp, &demod_sts, seq);
		break;

	case SYS_DTMB:
		dtmb_information(seq);
		if (strength <= -56) {
			dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
			strength = dtmb_get_power_strength(agc_if_gain[0]);
		}
		break;

	case SYS_ISDBT:
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
		if (devp->atsc_mode != VSB_8)
			return;

		snr = atsc_read_snr();
		seq_printf(seq, "snr: %d\n", snr);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			lock_status = dtvdemod_get_atsc_lock_sts();
		else
			lock_status = atsc_read_reg(0x0980);

		seq_printf(seq, "lock: %d\n", lock_status);

		ser = atsc_read_ser();
		seq_printf(seq, "ser: %d\n", ser);
		break;

	case SYS_DVBT:
		dvbt_info(devp, seq);
		break;

	case SYS_DVBT2:
		polling_en = devp->demod_thread;
		devp->demod_thread = 0;
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt2_info(seq);

		if (demod_is_t5d_cpu(devp))
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

		devp->demod_thread = polling_en;
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		dvbs_check_status(seq);
		break;

	default:
		break;
	}

	seq_printf(seq, "tuner strength : %d, 0x%x\n", strength, strength);
}

static int demod_dump_info_show(struct seq_file *seq, void *v)
{
	seq_dump_status(seq);
	seq_dump_regs(seq);

	return 0;
}

#define DEFINE_SHOW_DEMOD(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

/* cat /sys/kernel/debug/demod/dump_info */
DEFINE_SHOW_DEMOD(demod_dump_info);

static struct demod_debugfs_files_t demod_debug_files[] = {
	{"dump_info", S_IFREG | 0644, &demod_dump_info_fops},
	{"dvbc_channel_fast", S_IFREG | 0644, &dvbc_fast_search_fops},
	{"adc_clk", S_IFREG | 0644, &adc_clk_fops},
};

static void dtv_dmd_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static void wait_capture(int cap_cur_addr, int depth_MB, int start)
{
	int readfirst;
	int tmp;
	int time_out;
	int last = 0x1000000;

	time_out = 0;
	tmp = depth_MB << 20;

	/* 10 seconds time out */
	while (tmp && (time_out < 500)) {
		time_out = time_out + 1;
		usleep_range(1000, 2000);
		readfirst = front_read_reg(cap_cur_addr);

		if ((last - readfirst) > 0)
			tmp = 0;
		else
			last = readfirst;

		usleep_range(10000, 20000);
	}
}

unsigned int capture_adc_data_mass(void)
{
	int testbus_addr, width, vld;
	unsigned int tb_start, tb_depth;
	unsigned int start_addr = 0;

	/* capture point: common fe */
	testbus_addr = 0x1000;
	tb_depth = 10;
	/* sample bit width */
	width = 9;
	vld = 0x100000;

	/* enable arb */
	front_write_bits(0x39, 1, 30, 1);
	/* enable mask p2 */
	front_write_bits(0x3a, 1, 10, 1);
	/* Stop cap */
	front_write_bits(0x3a, 1, 12, 1);
	/* testbus addr */
	front_write_bits(0x39, testbus_addr, 0, 16);

	/* disable test_mode */
	front_write_bits(0x3a, 0, 13, 1);
	start_addr = dtvdd_devp->mem_start;

	/* 5M for DTMB working use */
	start_addr += (5 * 1024 * 1024);
	front_write_reg(0x3c, start_addr);
	front_write_reg(0x3d, start_addr + ((CAP_SIZE + 2) << 20));
	tb_depth = CAP_SIZE + 2;
	/* set testbus width */
	front_write_bits(0x3a, width, 0, 5);
	/* collect by system clk */
	front_write_reg(0x3b, vld);
	/* disable mask p2 */
	front_write_bits(0x3a, 0, 10, 1);
	front_write_bits(0x39, 0, 31, 1);
	front_write_bits(0x39, 1, 31, 1);
	/* go tb */
	front_write_bits(0x3a, 0, 12, 1);
	wait_capture(0x3f, tb_depth, start_addr);
	/* stop tb */
	/* front_write_bits(0x3a, 1, 12, 1); */
	tb_start = front_read_reg(0x3f);

	return tb_start;
}

int write_usb_mass(struct dtvdemod_capture_s *cap, unsigned int read_start)
{
	int count;
	unsigned int i, memsize, y, block_size;
	int times;
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	char *parm;
	mm_segment_t old_fs = get_fs();

	parm = cap->cap_dev_name;

	memsize = CAP_SIZE << 20;
	set_fs(KERNEL_DS);
	filp = filp_open(parm, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp))
		return -1;

	buf =	phys_to_virt(read_start);
	times = memsize / PER_WR;
	block_size = PER_WR;

	for (i = 0; i < cap->cap_size; i++) {
		while (front_read_reg(0x3f) <= (read_start + memsize * 3 / 4))
			usleep_range(5000, 5001);

		for (y = 0; y < times; y++) {
			count = vfs_write(filp, buf + y * PER_WR, block_size, &pos);
			if (count <= 0)
				pr_info("block_size is %d, 0x%lx, count is %d\n", block_size,
					y * PER_WR, count);
			usleep_range(5000, 5001);
		}
	}

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	/* stop tb */
	front_write_bits(0x3a, 1, 12, 1);
	return 0;
}

static u8 *demod_vmap(ulong addr, u32 size)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	/*pgprot_t pgprot = pgprot_noncached(PAGE_KERNEL);*/
	pgprot_t pgprot = PAGE_KERNEL;

	if (!PageHighMem(phys_to_page(addr)))
		return phys_to_virt(addr);

	offset = offset_in_page(addr);
	page_start = addr - offset;
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		phys = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(phys >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("the phy(%lx) vmaped fail, size: %d\n",
		       page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}

	kfree(pages);
	pr_info("[demod vmap] %s, pa(%lx) to va(%p), size: %d\n",
		__func__, page_start, vaddr, npages << PAGE_SHIFT);

	return vaddr + offset;
}

static void demod_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr)) {
		vunmap(addr);
		PR_INFO("%s:%p\n", __func__, vaddr);
	}
}

static void demod_dma_flush(void *vaddr, int size, enum dma_data_direction dir)
{
	ulong phy_addr;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("%s:devp is NULL\n", __func__);
		return;
	}

	if (is_vmalloc_or_module_addr(vaddr)) {
		phy_addr = page_to_phys(vmalloc_to_page(vaddr))
			+ offset_in_page(vaddr);
		if (phy_addr && PageHighMem(phys_to_page(phy_addr))) {
			dma_sync_single_for_device(&devp->this_pdev->dev,
						   phy_addr, size, dir);
		}
		return;
	}
}

static int read_memory_to_file(char *path, unsigned int start_addr,
				unsigned int size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (!start_addr) {
		PR_ERR("%s: start addr is NULL\n", __func__);
		return -1;
	}

	if (unlikely(!path)) {
		PR_ERR("%s:capture path is NULL\n", __func__);
		return -1;
	}

	PR_INFO("capture data path:%s,start addr:0x%x, size:%dM\n ",
		path, start_addr, size / SZ_1M);

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	buf = demod_vmap(start_addr, size);
	if (!buf) {
		PR_ERR("%s:buf is NULL\n", __func__);
		return -1;
	}

	demod_dma_flush(buf, size, DMA_FROM_DEVICE);
	vfs_write(filp, buf, size, &pos);
	demod_unmap_phyaddr(buf);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	return 0;
}

unsigned int capture_adc_data_once(char *path, unsigned int capture_mode)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	int testbus_addr, width, vld;
	unsigned int tb_start, tb_depth;
	unsigned int start_addr = 0;
	unsigned int top_saved, polling_en;
	unsigned int offset, size;

	if (unlikely(!devp)) {
		PR_ERR("%s:devp is NULL\n", __func__);
		return -1;
	}

	PR_INFO("%s:[path]%s,[cap_mode]%d\n", __func__, path, capture_mode);
	switch (capture_mode) {
	case 0: /* common fe */
		testbus_addr = 0x1000;
		//tb_depth = 10;
		/* sample bit width */
		width = 9;
		vld = 0x100000;
		break;

	case 3: /* ts stream */
		testbus_addr = 0x3000;
		//tb_depth = 100;
		/* sample bit width */
		width = 7;
		vld = 0x10000;
		break;

	case 4: /* T/T2 */
		testbus_addr = 0x1000;
		//tb_depth = 10;
		/* sample bit width */
		width = 11;
		vld = 0x100000;
		break;

	case 5: /* S/S2 */
		testbus_addr = 0x1012;
		//tb_depth = 10;
		/* sample bit width */
		width = 19;
		vld = 0x100000;
		break;

	default: /* common fe */
		testbus_addr = 0x1000;
		//tb_depth = 10;
		/* sample bit width */
		width = 9;
		vld = 0x100000;
		break;
	}

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		polling_en = devp->demod_thread;
		devp->demod_thread = 0;
		top_saved = demod_top_read_reg(DEMOD_TOP_CFG_REG_4);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	}

	/* enable arb */
	front_write_bits(0x39, 1, 30, 1);
	/* enable mask p2 */
	front_write_bits(0x3a, 1, 10, 1);
	/* Stop cap */
	front_write_bits(0x3a, 1, 12, 1);
	/* testbus addr */
	front_write_bits(0x39, testbus_addr, 0, 16);

	/* disable test_mode */
	front_write_bits(0x3a, 0, 13, 1);
	start_addr = devp->mem_start;

	switch (devp->last_delsys) {
	case SYS_DTMB:
	case SYS_ISDBT:
		offset = 5 * 1024 * 1024;
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
	case SYS_DVBT:
	case SYS_DVBS:
	case SYS_DVBS2:
		offset = 0 * 1024 * 1024;
		break;

	case SYS_DVBT2:
		offset = 32 * 1024 * 1024;
		break;

	default:
		offset = 32 * 1024 * 1024;
		PR_ERR("%s: unknown delivery system\n", __func__);
		break;
	}

	size = devp->cma_mem_size - offset;
	PR_INFO("%s:addr offset:%dM, cap_size:%dM\n", __func__,
		offset / SZ_1M, size / SZ_1M);
	start_addr += offset;
	front_write_reg(0x3c, start_addr);
	front_write_reg(0x3d, start_addr + size);
	tb_depth = size / SZ_1M;
	/* set testbus width */
	front_write_bits(0x3a, width, 0, 5);

	/* sel adc 10bit */
	if (width == 9)
		demod_top_write_bits(DEMOD_TOP_REGC, 0, 8, 1);
	else if (width == 11)/* sel adc 12bit */
		demod_top_write_bits(DEMOD_TOP_REGC, 1, 8, 1);

	/* collect by system clk */
	front_write_reg(0x3b, vld);
	/* disable mask p2 */
	front_write_bits(0x3a, 0, 10, 1);
	front_write_bits(0x39, 0, 31, 1);
	front_write_bits(0x39, 1, 31, 1);
	/* go tb */
	front_write_bits(0x3a, 0, 12, 1);
	wait_capture(0x3f, tb_depth, start_addr);
	/* stop tb */
	front_write_bits(0x3a, 1, 12, 1);
	tb_start = front_read_reg(0x3f);

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, top_saved);
		devp->demod_thread = polling_en;
	}

	read_memory_to_file(path, tb_start, size);
	return 0;
}

static void info_show(void)
{
	int snr, lock_status, agc_if_gain[3];
	unsigned int ser;
	int strength = 0;
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	unsigned int debug_mode = aml_demod_debug;
	char chip_name[30];
	struct aml_demod_sts demod_sts;

	strength = tuner_get_ch_power2();
	PR_INFO("version:%s\n", DTVDEMOD_VER);
	PR_INFO("cur delsys: %s\n", dtvdemod_get_cur_delsys(devp->last_delsys));
	PR_INFO("freq: %d\n", devp->freq);
	PR_INFO("demod_thread: %d\n", devp->demod_thread);
	get_chip_name(devp, chip_name);
	PR_INFO("chip: %d, %s\n", devp->data->hw_ver, chip_name);

	switch (devp->last_delsys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_DVBC_ANNEX_B:
		dvbc_status(devp, &demod_sts, NULL);
		break;

	case SYS_DTMB:
		dtmb_information(NULL);
		if (strength <= -56) {
			dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
			strength = dtmb_get_power_strength(agc_if_gain[0]);
		}
		break;

	case SYS_ISDBT:
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
		if (devp->atsc_mode != VSB_8)
			return;

		snr = atsc_read_snr();
		PR_INFO("snr: %d\n", snr);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			lock_status = dtvdemod_get_atsc_lock_sts();
		else
			lock_status = atsc_read_reg(0x0980);

		PR_INFO("lock: %d\n", lock_status);

		ser = atsc_read_ser();
		PR_INFO("ser: %d\n", ser);

		PR_INFO("atsc rst done: %d\n", devp->atsc_rst_done);
		break;

	case SYS_DVBT:
		aml_demod_debug |= DBG_DVBT;
		dvbt_info(devp, NULL);
		aml_demod_debug = debug_mode;
		break;

	case SYS_DVBT2:
		aml_demod_debug |= DBG_DVBT;
		devp->demod_thread = 0;
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt2_info(NULL);

		if (demod_is_t5d_cpu(devp))
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

		devp->demod_thread = 1;
		aml_demod_debug = debug_mode;
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		dvbs_check_status(NULL);
		break;

	default:
		break;
	}

	PR_INFO("tuner strength : %d, 0x%x\n", strength, strength);
}

static void dump_regs(struct amldtvdemod_device_s *devp)
{
	unsigned int reg_start, polling_en = 1;
	enum fe_delivery_system delsys = devp->last_delsys;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		pr_info("demod top start\n");
		for (reg_start = 0; reg_start <= 0xc; reg_start += 4)
			pr_info("[0x%x]=0x%x\n", reg_start, demod_top_read_reg(reg_start));
		pr_info("demod top end\n\n");

		pr_info("demod front start\n");
		for (reg_start = 0x20; reg_start <= 0x68; reg_start++)
			pr_info("[0x%x]=0x%x\n", reg_start, front_read_reg(reg_start));
		pr_info("demod front end\n\n");
	}

	switch (delsys) {
	case SYS_ATSC:
	case SYS_ATSCMH:
		pr_info("atsc start\n");
		if (is_meson_txlx_cpu()) {
			for (reg_start = 0; reg_start <= 0xfff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, atsc_read_reg(reg_start));
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			for (reg_start = 0; reg_start <= 0xff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, atsc_read_reg_v4(reg_start));
		}
		pr_info("atsc end\n");
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_DVBC_ANNEX_B:
		pr_info("dvbc/j83b start\n");
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			for (reg_start = 0; reg_start <= 0xff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, qam_read_reg(reg_start));
		}
		pr_info("dvbc/j83b end\n");
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		pr_info("dvbt/t2 start\n");
		if (devp->last_delsys == SYS_DVBT2) {
			polling_en = devp->demod_thread;
			devp->demod_thread = 0;
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		}

		if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
			for (reg_start = 0x0; reg_start <= 0xf4; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_t2_rdb(reg_start));

			for (reg_start = 0x538; reg_start <= 0xfff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_t2_rdb(reg_start));

			for (reg_start = 0x1500; reg_start <= 0x3776; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_t2_rdb(reg_start));
		}

		if (devp->last_delsys == SYS_DVBT2) {
			if (demod_is_t5d_cpu(devp))
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

			devp->demod_thread = polling_en;
		}
		pr_info("dvbt/t2 end\n");
		break;

	case SYS_DTMB:
		pr_info("dtmb start\n");
		for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
			pr_info("[0x%x] = 0x%x\n", reg_start, dtmb_read_reg(reg_start));

		pr_info("dtmb end\n");
		break;

	case SYS_ISDBT:
		pr_info("isdbt start\n");
		for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
			pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_isdbt_rd_reg_new(reg_start));

		pr_info("isdbt end\n");
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		pr_info("dvbs start\n");
		for (reg_start = 0x0; reg_start <= 0xfbf; reg_start++)
			pr_info("[0x%x] = 0x%x\n", reg_start, dvbs_rd_byte(reg_start));

		pr_info("dvbs end\n");
		break;

	default:
		break;
	}
}

static ssize_t dtv_dmd_attr_store(struct class *cls,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	struct amldtvdemod_device_s *devp;
	unsigned int capture_start = 0;
	unsigned int val = 0;
	unsigned int addr = 0;
	unsigned int i;
	static struct dvb_frontend *fe;
	//struct demod_config config;
	struct dtv_property tvp;
	unsigned int delay;
	enum fe_status sts;
	unsigned int cap_mode;

	if (!buf)
		return count;

	devp = dtvdd_devp;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	dtv_dmd_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "symb_rate")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val) == 0))
			devp->symbol_rate_manu = val;
	} else if (!strcmp(parm[0], "symb_rate_en")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val) == 0))
			devp->symb_rate_en = val;
	} else if (!strcmp(parm[0], "capture_mass")) {
		if (parm[1] && (strlen(parm[1]) < CAP_NAME_LEN)) /* path/name of ssd */
			strcpy(devp->capture_para.cap_dev_name, parm[1]);
		else
			goto fail_exec_cmd;

		if (parm[2] && (kstrtouint(parm[2], 10, &val) == 0)) /* capture size */
			devp->capture_para.cap_size = val;
		else
			goto fail_exec_cmd;

		capture_start = capture_adc_data_mass();
		write_usb_mass(&devp->capture_para, capture_start);

	} else if (!strcmp(parm[0], "capture_once")) {
		if (!parm[1]) {
			PR_ERR("%s:capture_once, no path para\n", __func__);
			goto fail_exec_cmd;
		}

		if (parm[2] && kstrtouint(parm[2], 10, &cap_mode) == 0)
			capture_adc_data_once(parm[1], cap_mode);
		else
			capture_adc_data_once(parm[1], 0);
	} else if (!strcmp(parm[0], "state")) {
		info_show();
	} else if (!strcmp(parm[0], "delsys")) {
		fe = &dtvdd_devp->frontend;
		//fe = aml_dtvdm_attach(&config);
		if (!fe) {
			pr_err("delsys, fe is NULL\n");
			goto fail_exec_cmd;
		}

		tvp.cmd = DTV_DELIVERY_SYSTEM;

		/* ref include/uapi/linux/dvb/frontend.h */
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0) {
			tvp.u.data = val;
			fe->dtv_property_cache.delivery_system = val;
		} else {
			tvp.u.data = SYS_DTMB;
			fe->dtv_property_cache.delivery_system = SYS_DTMB;
		}

		/* ref include/uapi/linux/dvb/frontend.h */
		if (parm[2] && (kstrtouint(parm[2], 10, &val)) == 0)
			fe->dtv_property_cache.frequency = val;
		else
			fe->dtv_property_cache.frequency = 88000000;

		if (parm[3] && (kstrtouint(parm[3], 10, &val)) == 0)
			fe->dtv_property_cache.symbol_rate = val;
		else
			fe->dtv_property_cache.symbol_rate = 0;

		if (parm[4] && (kstrtouint(parm[4], 10, &val)) == 0)
			fe->dtv_property_cache.bandwidth_hz = val;
		else
			fe->dtv_property_cache.bandwidth_hz = 8000000;

		if (parm[5] && (kstrtouint(parm[5], 10, &val)) == 0)
			fe->dtv_property_cache.modulation = val;
		else
			fe->dtv_property_cache.modulation = QAM_AUTO;

		if (fe->dtv_property_cache.delivery_system == SYS_DVBS ||
			fe->dtv_property_cache.delivery_system == SYS_DVBS2)
			fe->dtv_property_cache.frequency = fe->dtv_property_cache.frequency / 1000;

		if (fe->ops.init)
			fe->ops.init(fe);
		if (fe->ops.set_property)
			fe->ops.set_property(fe, &tvp);
		if (fe->ops.tune)
			fe->ops.tune(fe, true, 0, &delay, &sts);
	} else if (!strcmp(parm[0], "tune")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			fe->ops.tune(fe, val, 0, &delay, &sts);
	} else if (!strcmp(parm[0], "stop_wr")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->stop_reg_wr = val;
	} else if (!strcmp(parm[0], "timeout_atsc")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->timeout_atsc_ms = val;
	} else if (!strcmp(parm[0], "timeout_dvbt")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->timeout_dvbt_ms = val;
	} else if (!strcmp(parm[0], "timeout_dvbs")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->timeout_dvbs_ms = val;
	} else if (!strcmp(parm[0], "timeout_dvbc")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->timeout_dvbc_ms = val;
	} else if (!strcmp(parm[0], "dump_reg")) {
		dump_regs(devp);
	} else if (!strcmp(parm[0], "get_plp")) {
		dtvdemod_get_plp_dbg();
	} else if (!strcmp(parm[0], "set_plp")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			dtvdemod_set_plpid(val);
	} else if (!strcmp(parm[0], "lnb_en")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			aml_def_set_lnb_en(val);
	} else if (!strcmp(parm[0], "lnb_sel")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			aml_def_set_lnb_sel(val);
	} else if (!strcmp(parm[0], "diseqc_reg")) {
		demod_dump_reg_diseqc();
	} else if (!strcmp(parm[0], "diseqc_dbg")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			aml_diseqc_dbg_en(val);
	} else if (!strcmp(parm[0], "diseqc_sts")) {
		aml_diseqc_status();
	} else if (!strcmp(parm[0], "diseqc_cmd")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			diseqc_cmd_bypass = val;
		PR_INFO("diseqc_cmd_bypass %d\n", val);
	} else if (!strcmp(parm[0], "diseqc_burston")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			sendburst_on = val;
		PR_INFO("sendburst_on %d\n", val);
	} else if (!strcmp(parm[0], "diseqc_burstsa")) {
		aml_diseqc_toneburst_sa();
	} else if (!strcmp(parm[0], "diseqc_burstsb")) {
		aml_diseqc_toneburst_sb();
	} else if (!strcmp(parm[0], "diseqc_toneon")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0) {
			aml_diseqc_tone_on(val);
			aml_diseqc_flag_tone_on(val);
			PR_INFO("continuous_tone %d\n", val);
		}
	} else if (!strcmp(parm[0], "monitor")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->print_on = val;
	} else if (!strcmp(parm[0], "strength_limit")) {
		if (parm[1] && (kstrtoint(parm[1], 10, &devp->tuner_strength_limit)) == 0)
			;
	} else if (!strcmp(parm[0], "debug_on")) {
		if (parm[1] && (kstrtoint(parm[1], 10, &devp->debug_on)) == 0)
			;
	} else if (!strcmp(parm[0], "dvbsw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				dvbs_wr_byte(addr, val);
				PR_INFO("dvbs wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "dvbsr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = dvbs_rd_byte(addr);
			PR_INFO("dvds rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dvbsd")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				for (i = addr; i < (addr + val); i++)
					PR_INFO("dvds rd addr:0x%x, val:0x%x\n",
						i, dvbs_rd_byte(i));
			}
		}
	} else if (!strcmp(parm[0], "dvbtr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = dvbt_t2_rdb(addr);
			PR_INFO("dvdt rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dvbtw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				dvbt_t2_wrb(addr, val);
				PR_INFO("dvbt wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "topr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = demod_top_read_reg(addr);
			PR_INFO("top rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "topw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				demod_top_write_reg(addr, val);
				PR_INFO("top wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "frontr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = front_read_reg(addr);
			PR_INFO("front rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "frontw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				front_write_reg(addr, val);
				PR_INFO("front wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "atscr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = atsc_read_reg_v4(addr);
			PR_INFO("atsc rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "atscw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				atsc_write_reg_v4(addr, val);
				PR_INFO("atsc wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "dvbcr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = qam_read_reg(addr);
			PR_INFO("dvbc rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dvbcw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				qam_write_reg(addr, val);
				PR_INFO("atsc wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "demod_thread")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0)
			devp->demod_thread = val;
	} else if (!strcmp(parm[0], "blind_stop")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0)
			devp->blind_scan_stop = val;
		PR_INFO("set blind scan to %d\n", devp->blind_scan_stop);
	} else if (!strcmp(parm[0], "cr_val")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0)
			devp->atsc_cr_step_size_dbg = val;
		PR_INFO("set atsc cr val to 0x%x\n", devp->atsc_cr_step_size_dbg);
	} else if (!strcmp(parm[0], "ci_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0) {
			if (val == 0)
				qam_write_bits(0x11, 0x90, 24, 8);
			else if (val == 1)
				qam_write_bits(0x11, 0x00, 24, 8);
			PR_INFO("ic card set mode to %d\n", val);
		}
	}

fail_exec_cmd:
	kfree(buf_orig);

	return count;
}

static ssize_t dtv_dmd_attr_show(struct class *cls,
				 struct class_attribute *attr, char *buf)
{
	/* struct amldtvdemod_device_s *devp = dev_get_drvdata(dev); */
	ssize_t len = 0;

	len += sprintf(buf + len, "symb_rate [val1]\n");
	len += sprintf(buf + len, "symb_rate_en [val1]\n");
	len += sprintf(buf + len, "capture [val1] [val2]\n");
	len += sprintf(buf + len, "state\n");
	len += sprintf(buf + len, "delsys [val1]\n");
	len += sprintf(buf + len, "tune [val1]\n");
	len += sprintf(buf + len, "stop_wr [val1]\n");
	len += sprintf(buf + len, "lnb_en [val1]\n");
	len += sprintf(buf + len, "lnb_sel [val1]\n");
	len += sprintf(buf + len, "diseqc_reg\n");
	len += sprintf(buf + len, "dvbsw [addr] [val]\n");
	len += sprintf(buf + len, "dvbsr [addr]\n");
	len += sprintf(buf + len, "dvbsd [addr] [len]\n");
	len += sprintf(buf + len, "diseqc_dbg [val]\n");
	len += sprintf(buf + len, "diseqc_sts\n");
	len += sprintf(buf + len, "diseqc_cmd [val]\n");
	len += sprintf(buf + len, "diseqc_burston [val]\n");
	len += sprintf(buf + len, "diseqc_burstsa\n");
	len += sprintf(buf + len, "diseqc_burstsb\n");
	len += sprintf(buf + len, "diseqc_toneon [val]\n");

	return len;
}

static ssize_t dtmb_para_show(struct class *cls,
			      struct class_attribute *attr, char *buf)
{
	int snr, lock_status, bch, agc_if_gain[3];
	struct dvb_frontend *dvbfe;
	int strength = 0;

	if (dtmb_mode == DTMB_READ_STRENGTH) {
		dvbfe = aml_get_fe();/*get_si2177_tuner();*/

		strength = tuner_get_ch_power2();
		if (strength <= -56) {
			dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
			strength = dtmb_get_power_strength(agc_if_gain[0]);
		}
		return sprintf(buf, "strength is %d\n", strength);
	} else if (dtmb_mode == DTMB_READ_SNR) {
		/*snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;*/
		snr = dtmb_reg_r_che_snr();
		snr = convert_snr(snr);
		return sprintf(buf, "snr is %d\n", snr);
	} else if (dtmb_mode == DTMB_READ_LOCK) {
		lock_status = dtmb_reg_r_fec_lock();
		return sprintf(buf, "lock_status is %d\n", lock_status);
	} else if (dtmb_mode == DTMB_READ_BCH) {
		bch = dtmb_reg_r_bch();
		return sprintf(buf, "bch is %d\n", bch);
	} else {
		return sprintf(buf, "dtmb_para_show can't match mode\n");
	}

	return 0;
}

static ssize_t dtmb_para_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	if (buf[0] == '0')
		dtmb_mode = DTMB_READ_STRENGTH;
	else if (buf[0] == '1')
		dtmb_mode = DTMB_READ_SNR;
	else if (buf[0] == '2')
		dtmb_mode = DTMB_READ_LOCK;
	else if (buf[0] == '3')
		dtmb_mode = DTMB_READ_BCH;

	return count;
}

static ssize_t atsc_para_show(struct class *cls,
			      struct class_attribute *attr, char *buf)
{
	int snr, lock_status;
	unsigned int ser;
	int strength = 0;

	if (dtvdd_devp->atsc_mode != VSB_8)
		return 0;

	if (atsc_mode_para == ATSC_READ_STRENGTH) {
		strength = tuner_get_ch_power2();
		return sprintf(buf, "strength is %d\n", strength);
	} else if (atsc_mode_para == ATSC_READ_SNR) {
		snr = atsc_read_snr();
		return sprintf(buf, "snr is %d\n", snr);
	} else if (atsc_mode_para == ATSC_READ_LOCK) {
		lock_status =
			atsc_read_reg(0x0980);
		return sprintf(buf, "lock_status is %x\n", lock_status);
	} else if (atsc_mode_para == ATSC_READ_SER) {
		ser = atsc_read_ser();
		return sprintf(buf, "ser is %d\n", ser);
	} else if (atsc_mode_para == ATSC_READ_FREQ) {
		return sprintf(buf, "freq is %d\n", dtvdd_devp->freq);
	} else {
		return sprintf(buf, "atsc_para_show can't match mode\n");
	}

	return 0;
}

static ssize_t atsc_para_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	if (buf[0] == '0')
		atsc_mode_para = ATSC_READ_STRENGTH;
	else if (buf[0] == '1')
		atsc_mode_para = ATSC_READ_SNR;
	else if (buf[0] == '2')
		atsc_mode_para = ATSC_READ_LOCK;
	else if (buf[0] == '3')
		atsc_mode_para = ATSC_READ_SER;
	else if (buf[0] == '4')
		atsc_mode_para = ATSC_READ_FREQ;

	return count;
}

static ssize_t diseq_cmd_store(struct class *cla, struct class_attribute *attr,
	const char *bu, size_t count)
{
	int tmpbuf[20] = {};
	int i;
	int cnt;
	struct dvb_diseqc_master_cmd cmd;
	/*int ret;*/

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		    &tmpbuf[0], &tmpbuf[1], &tmpbuf[2], &tmpbuf[3],
		    &tmpbuf[4], &tmpbuf[5], &tmpbuf[6], &tmpbuf[7],
		    &tmpbuf[8], &tmpbuf[9], &tmpbuf[10], &tmpbuf[11],
		    &tmpbuf[12], &tmpbuf[13], &tmpbuf[14], &tmpbuf[15]);
	if (cnt < 0)
		return -EINVAL;
	if (cnt > 6)
		cnt = 6;

	for (i = 0; i < cnt; i++) {
		cmd.msg[i] = (char)tmpbuf[i];
		PR_INFO(" 0x%x\n", cmd.msg[i]);
	}
	cmd.msg_len = cnt;
	/* send diseqc msg */
	aml_diseqc_send_cmd(&cmd);

	return count;
}

static ssize_t diseq_cmd_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "diseq_cmd debug interface\n");
}

static CLASS_ATTR(diseq_cmd, 0664, diseq_cmd_show, diseq_cmd_store);
static CLASS_ATTR(attr, 0664, dtv_dmd_attr_show, dtv_dmd_attr_store);
static CLASS_ATTR(dtmb_para, 0644, dtmb_para_show, dtmb_para_store);
static CLASS_ATTR(atsc_para, 0644, atsc_para_show, atsc_para_store);

int dtvdemod_create_class_files(struct class *clsp)
{
	int ret = 0;

	ret = class_create_file(clsp, &class_attr_dtmb_para);
	ret |= class_create_file(clsp, &class_attr_atsc_para);
	ret |= class_create_file(clsp, &class_attr_attr);
	ret |= class_create_file(clsp, &class_attr_diseq_cmd);

	return ret;
}

void dtvdemod_remove_class_files(struct class *clsp)
{
	class_remove_file(clsp, &class_attr_dtmb_para);
	class_remove_file(clsp, &class_attr_atsc_para);
	class_remove_file(clsp, &class_attr_attr);
	class_remove_file(clsp, &class_attr_diseq_cmd);
}

void aml_demod_dbg_init(void)
{
	struct dentry *root_entry = dtvdd_devp->demod_root;
	struct dentry *entry;
	unsigned int i;

	PR_INFO("%s\n", __func__);

	root_entry = debugfs_create_dir("demod", NULL);
	if (!root_entry) {
		PR_INFO("Can't create debugfs dir frontend.\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(demod_debug_files); i++) {
		entry = debugfs_create_file(demod_debug_files[i].name,
			demod_debug_files[i].mode,
			root_entry, NULL,
			demod_debug_files[i].fops);
		if (!entry)
			PR_INFO("Can't create debugfs seq file.\n");
	}
}

void aml_demod_dbg_exit(void)
{
	struct dentry *root_entry = dtvdd_devp->demod_root;

	if (dtvdd_devp && root_entry)
		debugfs_remove_recursive(root_entry);
}

