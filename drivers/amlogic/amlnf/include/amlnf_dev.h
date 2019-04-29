/*
 * drivers/amlogic/amlnf/include/amlnf_dev.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_NFTL_BLOCK_H
#define __AML_NFTL_BLOCK_H

#include "amlnf_type.h"
#include "hw_ctrl.h"
#include "amlnf_ctrl.h"
#include "amlnf_cfg.h"

#include <linux/types.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>

#define aml_nftl_dbg	aml_nand_dbg

#define aml_nftl_malloc	aml_nand_malloc
#define aml_nftl_free	aml_nand_free


#define	MAX_DEVICE_NUM		4
#define	MAX_DEVICE_NAME_LEN	16


#define AMLNF_DEV_MAJOR		250
#define TIMESTAMP_LENGTH	15
#define MAX_TIMESTAMP_NUM	((1<<(TIMESTAMP_LENGTH-1))-1)
#define AML_NFTL_BOUNCE_SIZE	0x40000

#define NFTL_MAX_SCHEDULE_TIMEOUT	1000
#define NFTL_FLUSH_DATA_TIME		1
#define NFTL_CACHE_FORCE_WRITE_LEN	16


#define	FACTORY_BAD_BLOCK_ERROR		2
#define	BYTES_PER_SECTOR		512
#define	SHIFT_PER_SECTOR		9
#define	BYTES_OF_USER_PER_PAGE		16
#define	MIN_BYTES_OF_USER_PER_PAGE	16

#define	AMLNF_DEV_RO_MODE	(1<<0)

#define RET_YES		1
#define RET_NO		0

#define  PRINT aml_nftl_dbg

#define	NAND_BLOCK_GOOD		0
#define	NAND_BLOCK_USED_BAD	1
#define	NAND_BLOCK_FACTORY_BAD	2


#define	MAX_NAND_PART_NUM	16
#define	MAX_NAND_PART_NAME_LEN	16

#define	NAND_PART_SIZE_FULL	-1

#define	NAND_BLOCK_GOOD		0
#define	NAND_BLOCK_USED_BAD	1
#define	NAND_BLOCK_FACTORY_BAD	2

/***nand device name***/
#define	NAND_BOOT_NAME		"nfboot"
#define	NAND_CACHE_NAME		"nfcache"
#define	NAND_CODE_NAME		"nfcode"
#define	NAND_DATA_NAME		"nfdata"
#define	NAND_RESERVED_NAME	"nfrevd"

#define	PHY_DEV_NUM	3


/***boot device flag***/
#define SPI_BOOT_FLAG		0
#define NAND_BOOT_FLAG		1
#define EMMC_BOOT_FLAG		2
#define CARD_BOOT_FLAG		3
#define SPI_NAND_FLAG		4
#define SPI_EMMC_FLAG		5

#define STORAGE_DEV_NOSET       (0)
#define STORAGE_DEV_EMMC        (1)
#define STORAGE_DEV_NAND        (2)
#define STORAGE_DEV_SPI         (3)
#define STORAGE_DEV_SDCARD      (4)
#define STORAGE_DEV_USB         (5)


/***nand BOOT flags***/
#define NAND_BOOT_NORMAL					0
#define NAND_BOOT_UPGRATE					1
#define NAND_BOOT_ERASE_PROTECT_CACHE       2
#define NAND_BOOT_ERASE_ALL					3
#define NAND_BOOT_SCRUB_ALL					4
#define NAND_SCAN_ID_INIT					5

/****nand debug flag info******/
#define NAND_WRITE_VERIFY	1

#define DRV_AMLNFDEV_NAME	"aml_nand"
#define DRV_AMLNFDEV_AUTHOR	"AMLOGIC SZ NAND TEAM"
#define DRV_AMLNFDEV_DESC	"Amlogic Nand Flash driver"


struct amlnf_partition {
	/* identifier string */
	char name[MAX_NAND_PART_NAME_LEN];
	/* partition size */
	uint64_t size;
	/* offset within the master space */
	uint64_t offset;
	/* master flags to mask out for this partition */
	unsigned mask_flags;
	void *priv;
};

enum amlnf_error_t {
	NAND_SUCCESS = 0,
	NAND_CMD_FAILURE = 1,
	NAND_BUSY_FAILURE = 2,
	NAND_ID_FAILURE = 3,
	NAND_DMA_FAILURE = 4,
	NAND_ECC_FAILURE = 5,
	NAND_BITFLIP_FAILURE = 6,
	NAND_MALLOC_FAILURE = 7,
	NAND_ARGUMENT_FAILURE = 8,
	NAND_STATUS_FAILURE = 9,
	NAND_WP_FAILURE = 10,
	NAND_SELECT_CHIP_FAILURE = 12,
	NAND_ERASE_FAILED = 13,
	NAND_WRITE_FAILED = 14,
	NAND_FAILED = 15,
	NAND_READ_FAILED = 16,
	NAND_BAD_BLCOK_FAILURE = 17,
	NAND_SHIPPED_BAD_FAILURE = 18,
	NAND_CONFIGS_FAILED	= 19,
	NAND_SHIPPED_BADBLOCK_FAILED = 20,
};

struct _nftl_cfg {
	ushort nftl_use_cache;
	ushort nftl_support_gc_read_reclaim;
	ushort nftl_support_wear_leveling;
	ushort nftl_need_erase;
	ushort nftl_part_reserved_block_ratio;
	ushort nftl_part_adjust_block_num;
	ushort nftl_min_free_block_num;
	ushort nftl_gc_threshold_free_block_num;
	ushort nftl_min_free_block;
	ushort nftl_gc_threshold_ratio_numerator;
	ushort nftl_gc_threshold_ratio_denominator;
	ushort nftl_max_cache_write_num;
};

/*
 * Constants for ECC_MODES
 */
enum oob_modes_t {
	NAND_HW_ECC,
	NAND_SOFT_ECC,
};

/**
 * struct phydev_ops - oob operation operands
 * @mode:	operation mode, for ecc none or hw mode
 * @len:	number of data bytes to write/read
 * @retlen:	number of data bytes written/read
 * @ooblen:	number of oob bytes to write/read
 * @oobretlen:	number of oob bytes written/read
 * @datbuf:	data buffer - if NULL only oob data are read/written
 * @oobbuf:	oob data buffer- if NULL only data are read/written
 *
 * Note, it is not allowed to read/write more than one OOB area
 * at one go when oob operation.
 * That is, the interface assumes that the OOB write requests
 * program/read within only one page's
 * OOB area.
 */
struct phydev_ops {
	enum oob_modes_t mode;
	uint64_t addr;
	/* one operation less than 2GB data len */
	uint64_t len;
	unsigned int retlen;
	unsigned int ooblen;
	unsigned char *datbuf;
	unsigned char *oobbuf;
};

/* #include "../phy/phynand.h" */
/*
 * API for NFTL driver.
 * Provide nand basic information and common operation function.
 * Must meet all the requirement of NFTL driver,
 * and also consider the fulture extensions
 */

/**
 * struct amlnand_phydev - nand phy device
 * @name:
 * @type: used for fulture, differ from SLC, MLC and TLC
 * @retlen:	number of data bytes written/read
 * @ooblen:	number of oob bytes to write/read
 * @oobretlen:	number of oob bytes written/read
 * @datbuf:	data buffer - if NULL only oob data are read/written
 * @oobbuf:	oob data buffer- if NULL only data are read/written
 *
 * Note, it is not allowed to read/write more than one OOB area
 * at one go when oob operation.
 * That is, the interface assumes that the OOB write requests
 * program/read within only one page's
 * OOB area.
 */
struct amlnand_phydev {
	const char name[MAX_DEVICE_NAME_LEN];

	/*****nand flash chip type, maybe SLC/MLC/TLC ******/
	unsigned char type;

	unsigned char ecc_failed;
	unsigned char bit_flip;

	/*** used for Read-Only, or other feature ***/
	unsigned int flags;

	/*** used for other feature setting***/
	unsigned int option;

	/*** struct for whole nand chip***/
	/* struct amlnand_chip *aml_chip; */
	void *priv;

	struct list_head list;

	struct device dev;
	struct class cls;

	struct cdev uboot_cdev;

	/*** offset value of the whole nand device***/
	uint64_t offset;

	/*** Total size of the cunrrent nand device***/
	uint64_t size;

	unsigned char chipnr;

	/*** "Major" erase size for the device. ***/
	unsigned int erasesize;

	/* Minimal writable flash unit size. In case of NOR flash it is 1 (even
	 * though individual bits can be cleared), in case of NAND flash it is
	 * one NAND page (or half, or one-fourths of it), in case of ECC-ed NOR
	 * it is of ECC block size, etc. It is illegal to have writesize = 0.
	 * Any driver registering a struct mtd_info must ensure a writesize of
	 * 1 or larger.
	 */
	unsigned int writesize;

	/**** Available OOB bytes per page ***/
	unsigned int oobavail;

	/*
	 * If erasesize is a power of 2 then the shift is stored in
	 * erasesize_shift otherwise erasesize_shift is zero. Ditto writesize.
	 */
	unsigned int erasesize_shift;
	unsigned int writesize_shift;

	/*** Masks based on writesize_shift ***/
	unsigned int writesize_mask;

	/***partitions info***/
	unsigned char nr_partitions;
	struct amlnf_partition *partitions;

	struct phydev_ops ops;
	/*
	 * Erase is an asynchronous operation.  Device drivers are supposed
	 * to call instr->callback() whenever the operation completes, even
	 * if it completes with a failure.
	 * Callers are supposed to pass a callback function and wait for it
	 * to be called before writing to the block.
	 */
	int (*erase)(struct amlnand_phydev *phydev);

	/***basic data operation and included oob data****/
	int (*read)(struct amlnand_phydev *phydev);
	int (*write)(struct amlnand_phydev *phydev);

	/* In blackbox flight recorder like scenarios we want to make successful
	 * writes in interrupt context. panic_write() is only intended to be
	 * called when its known the kernel is about to panic and we need the
	 * write to succeed. Since the kernel is not going to be running for
	 * much longer, this function can break locks and delay to ensure the
	 * write succeeds (but not sleep).
	 */
	int (*panic_write)(struct amlnand_phydev *phydev);

	int (*read_oob)(struct amlnand_phydev *phydev);

	/********not support yet**********/
	int (*write_oob)(struct amlnand_phydev *phydev);

	/*
	 * support read data for sect_uint(512bytes in genreal),
	 * not just writesize unit, to improve read data speed.
	 * Not spport yet.
	 */
	int (*read_sect)(struct amlnand_phydev *phydev);

	/*
	Sync to nand device, used for TLC nand to finish the current write ops
	*/
	void (*sync)(struct amlnand_phydev *phydev);

	/* Chip-supported device locking */
	int (*lock)(struct amlnand_phydev *phydev);
	int (*unlock)(struct amlnand_phydev *phydev);
	int (*is_locked)(struct amlnand_phydev *phydev);

	/* Power Management functions */
	int (*suspend)(struct amlnand_phydev *phydev);
	void (*resume)(struct amlnand_phydev *phydev);

	/* Bad block management functions, maybe managed by NFTL layer*/
	int (*block_isbad)(struct amlnand_phydev *phydev);
	int (*block_markbad)(struct amlnand_phydev *phydev);

	int (*block_modifybbt)(struct amlnand_phydev *phydev, int value);
	int (*update_bbt)(struct amlnand_phydev *phydev);
	int (*phydev_test_block)(struct amlnand_phydev *phydev);
	/* default mode before reboot */
	/* struct notifier_block reboot_notifier; */
};

struct amlnf_logicdev_t {

	struct mutex lock;
	struct timespec ts_write_start;
	spinlock_t thread_lock;
	struct notifier_block nb;
	struct task_struct *thread;
	struct class cls;

	/* struct aml_nftl_part_t* aml_nftl_part; */
	void *priv;

	struct amlnand_phydev *nand_dev;
	/* amlnf_dev list, since several dev can share one logicdev. */
	struct list_head nfdev_list;

	int (*read_data)(struct amlnf_logicdev_t *amlnf_logicdev,
		unsigned long start_sector,
		unsigned long len,
		unsigned char *buf);
	int (*write_data)(struct amlnf_logicdev_t *amlnf_logicdev,
		unsigned long start_sector,
		unsigned long len,
		unsigned char *buf);
	int (*flush)(struct amlnf_logicdev_t *amlnf_logicdev);
	int (*shutdown)(struct amlnf_logicdev_t *amlnf_logicdev);

	struct _nftl_cfg nftl_cfg;
};

struct amlnf_dev {
	/* identifier string */
	char name[MAX_NAND_PART_NAME_LEN];
	struct amlnf_logicdev_t *logicdev;
	struct amlnand_phydev *nand_dev;
	uint64_t size_sector;
	uint64_t offset_sector;
	unsigned int mask_flags;

	struct kref ref;
	struct request *req;
	struct request_queue *queue;
	spinlock_t queue_lock;
	struct mutex mutex_lock_req;
	struct mutex mutex_lock;
	struct scatterlist *bounce_sg;
	unsigned int bounce_sg_len;
	bool bg_stop;
	struct task_struct *thread;
	struct gendisk *disk;
	struct attribute_group *disk_attributes;
	struct class cls;

	struct list_head list;

	uint (*read_sector)(struct amlnf_dev *dev,
		unsigned long start_sector,
		unsigned long len,
		unsigned char *buf);
	uint (*write_sector)(struct amlnf_dev *dev,
		unsigned long start_sector,
		unsigned long len,
		unsigned char *buf);
	uint (*flush)(struct amlnf_dev *dev);
};

extern struct amlnand_phydev *phydev;
extern struct list_head nphy_dev_list;
extern struct list_head nf_dev_list;

struct amlnf_platform_data {
	void __iomem *poc_cfg_reg;
	void __iomem *nf_reg_base;
	void __iomem *ext_clk_reg;
	void __iomem *pinmux_base;
	unsigned int irq;
};

struct aml_nand_device {
	struct amlnf_platform_data	*platform_data;
	int nandboot;
};

#define amlnf_notifier_to_blk(l) container_of(l, struct amlnf_logicdev_t, nb)

extern struct list_head nphy_dev_list;
extern struct partitions *part_table;
extern struct aml_nand_device *aml_nand_dev;
extern int boot_device_flag;
extern void nand_get_chip(void *aml_chip);
extern void nand_release_chip(void *aml_chip);
extern int check_storage_device(void);

extern unsigned char device_model[20];


extern ssize_t verify_nand_page(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);
extern ssize_t dump_nand_page(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);
extern ssize_t show_nand_info(struct class *class,
	struct class_attribute *attr,
	char *buf);
extern ssize_t show_bbt_table(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);
extern ssize_t nand_ioctl(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);
extern ssize_t show_amlnf_version_info(struct class *class,
	struct class_attribute *attr,
	char *buf);
extern ssize_t nand_page_read(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);
extern ssize_t nand_page_write(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);
/*  */
extern ushort aml_nftl_get_part_write_cache_nums(void *_part);
extern int logicdev_bg_handle(void *priv);
/*  */
extern ssize_t show_part_struct(struct class *class,
	struct class_attribute *attr,
	char *buf);
extern ssize_t show_list(struct class *class,
	struct class_attribute *attr,
	const char *buf);
extern ssize_t discard_page(struct class *class,
	struct class_attribute *attr,
	const char *buf);
extern ssize_t do_gc_all(struct class *class,
	struct class_attribute *attr,
	const char *buf);
extern ssize_t do_gc_one(struct class *class,
	struct class_attribute *attr,
	const char *buf);
extern ssize_t do_test(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count);

extern int amlnf_pdev_register(struct amlnand_phydev *phydev);
extern int amlnf_ldev_register(void);
extern void amlchip_resume(struct amlnand_phydev *phydev);
extern int phydev_suspend(struct amlnand_phydev *phydev);
extern void phydev_resume(struct amlnand_phydev *phydev);
extern int amlphy_prepare(unsigned int flag);

extern int add_ntd_partitions(struct amlnand_phydev *master);
extern int  boot_device_register(struct amlnand_phydev *phydev);

extern void *aml_nand_malloc(uint size);
extern void aml_nand_free(const void *ptr);

extern void *amlnf_dma_malloc(uint size, unsigned char flag);
extern void amlnf_dma_free(const void *ptr, u32 size, unsigned char flag);

extern int amlnf_get_logicdev(struct amlnf_logicdev_t *amlnf_logicdev);
extern int amlnf_free_logicdev(struct amlnf_logicdev_t *amlnf_logicdev);
extern int amlnf_logicdev_mis_init(struct amlnf_logicdev_t *amlnf_logicdev);


extern void pinmux_select_chip(unsigned int ce_enable,
	unsigned int rb_enable,
	unsigned int flag);

extern int amlnf_phy_init(unsigned char flag, struct platform_device *pdev);

extern int amlnf_logic_init(unsigned int flag);
extern int amlnf_dev_init(unsigned int flag);

extern int is_phydev_off_adjust(void);
extern int get_adjust_block_num(void);

extern void amldev_dumpinfo(struct amlnand_phydev *phydev);

#endif
