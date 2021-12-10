/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_CRYPTO_DMA_H_
#define _AML_CRYPTO_DMA_H_
#include <linux/io.h>
#include <crypto/hash.h>
#include <crypto/algapi.h>

#define MAX_NUM_TABLES (16)
#define DMA_IRQ_MODE	(0)

#define DMA_BLOCK_MODE_SIZE (512)

enum GXL_DMA_REG_OFFSETS {
	GXL_DMA_T0   = 0x00,
	GXL_DMA_T1   = 0x01,
	GXL_DMA_T2   = 0x02,
	GXL_DMA_T3   = 0x03,
	GXL_DMA_STS0 = 0x04,
	GXL_DMA_STS1 = 0x05,
	GXL_DMA_STS2 = 0x06,
	GXL_DMA_STS3 = 0x07,
	GXL_DMA_CFG  = 0x08,
};

enum TXLX_DMA_REG_OFFSETS {
	TXLX_DMA_T0   = 0x00,
	TXLX_DMA_T1   = 0x01,
	TXLX_DMA_T2   = 0x02,
	TXLX_DMA_T3   = 0x03,
	TXLX_DMA_T4   = 0x04,
	TXLX_DMA_T5   = 0x05,

	TXLX_DMA_STS0 = 0x08,
	TXLX_DMA_STS1 = 0x09,
	TXLX_DMA_STS2 = 0x0a,
	TXLX_DMA_STS3 = 0x0b,
	TXLX_DMA_STS4 = 0x0c,
	TXLX_DMA_STS5 = 0x0d,

	TXLX_DMA_CFG  = 0x10,
	TXLX_DMA_SEC  = 0x11,
};

enum CRYPTO_ALGO_CAPABILITY {
	CAP_AES = 0x1,
	CAP_DES = 0x2,
	CAP_TDES = 0x4,
	CAP_S17 = 0x8,
};

#define aml_write_reg(addr, data) \
	writel(data, (int *)addr)

#define aml_read_reg(addr) \
	readl(addr)

/* driver logic flags */
#define TDES_KEY_LENGTH 32
#define TDES_MIN_BLOCK_SIZE 8

#define OP_MODE_ECB 0
#define OP_MODE_CBC 1
#define OP_MODE_CTR 2

#define OP_MODE_SHA    0
#define OP_MODE_HMAC_I 1
#define OP_MODE_HMAC_O 2

#define MODE_DMA     0x0
#define MODE_KEY     0x1
#define MODE_MEMSET  0x2
/* 0x3 is skipped */
/* 0x4 is skipped */
#define MODE_SHA1    0x5
#define MODE_SHA256  0x6
#define MODE_SHA224  0x7
#define MODE_AES128  0x8
#define MODE_AES192  0x9
#define MODE_AES256  0xa
#define MODE_S17     0xb
#define MODE_DES     0xc
/* 0xd is skipped */
#define MODE_TDES_2K 0xe
#define MODE_TDES_3K 0xf

struct dma_dsc {
	union {
		u32 d32;
		struct {
			unsigned length:17;
			unsigned irq:1;
			unsigned eoc:1;
			unsigned loop:1;
			unsigned mode:4;
			unsigned begin:1;
			unsigned end:1;
			unsigned op_mode:2;
			unsigned enc_sha_only:1;
			unsigned block:1;
			unsigned link_error:1;
			unsigned owner:1;
		} b;
	} dsc_cfg;
	u32 src_addr;
	u32 tgt_addr;
} __packed;

struct dma_sg_dsc {
	union {
		u32 d32;
		struct {
			unsigned valid:1;
			unsigned eoc:1;
			unsigned intr:1;
			unsigned act:3;
			unsigned length:26;
		} b;
	} dsc_cfg;
	u32 addr;
} __packed;

#define DMA_FLAG_MAY_OCCUPY    BIT(0)
#define DMA_FLAG_TDES_IN_USE   BIT(1)
#define DMA_FLAG_AES_IN_USE    BIT(2)
#define DMA_FLAG_SHA_IN_USE    BIT(3)

#define DMA_STATUS_KEY_ERROR   BIT(1)

#define DMA_KEY_IV_BUF_SIZE (48)
#define DMA_KEY_IV_BUF_SIZE_64B (64)
struct aml_dma_dev {
	spinlock_t dma_lock; /* spinlock for dma */
	u32 thread;
	u32 status;
	int	irq;
	u8 dma_busy;
	u8 link_mode;
	unsigned long irq_flags;
	struct task_struct *kthread;
	struct crypto_queue	queue;
	spinlock_t queue_lock; /* spinlock for queue */
};

u32 swap_ulong32(u32 val);
void aml_write_crypto_reg(u32 addr, u32 data);
u32 aml_read_crypto_reg(u32 addr);
void aml_dma_debug(struct dma_dsc *dsc, u32 nents, const char *msg,
		   u32 thread, u32 status);
void aml_dma_link_debug(struct dma_sg_dsc *dsc, dma_addr_t dma_dsc,
			u32 nents, const char *msg);

u8 aml_dma_do_hw_crypto(struct aml_dma_dev *dd,
			  struct dma_dsc *dsc,
			  u32 dsc_len,
			  dma_addr_t dsc_addr,
			  u8 polling, u8 dma_flags);

void aml_dma_finish_hw_crypto(struct aml_dma_dev *dd, u8 dma_flags);

/* queue utilities */
int aml_dma_crypto_enqueue_req(struct aml_dma_dev *dd,
			       struct crypto_async_request *req);

u32 get_dma_t0_offset(void);
u32 get_dma_sts0_offset(void);

extern void __iomem *cryptoreg;

extern int debug;
#define dbgp(level, fmt, arg...)                 \
	do {                                            \
		if (unlikely(level >= debug))                         \
			pr_warn("%s: " fmt, __func__, ## arg);\
	} while (0)

#endif

extern int disable_link_mode;

int __init aml_sha_driver_init(void);
void aml_sha_driver_exit(void);

int __init aml_tdes_driver_init(void);
void aml_tdes_driver_exit(void);

int __init aml_aes_driver_init(void);
void aml_aes_driver_exit(void);

int __init aml_crypto_device_driver_init(void);
void aml_crypto_device_driver_exit(void);
