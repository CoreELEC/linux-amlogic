/*
 * ca.h
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Lesser Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _DVBCA_H_
#define _DVBCA_H_

/* slot interface types and info */

typedef struct ca_slot_info {
	int num;               /* slot number */

	int type;              /* CA interface this slot supports */
#define CA_CI            1     /* CI high level interface */
#define CA_CI_LINK       2     /* CI link layer level interface */
#define CA_CI_PHYS       4     /* CI physical layer level interface */
#define CA_DESCR         8     /* built-in descrambler */
#define CA_SC          128     /* simple smart card interface */

	unsigned int flags;
#define CA_CI_MODULE_PRESENT 1 /* module (or card) inserted */
#define CA_CI_MODULE_READY   2
} ca_slot_info_t;


/* descrambler types and info */

typedef struct ca_descr_info {
	unsigned int num;          /* number of available descramblers (keys) */
	unsigned int type;         /* type of supported scrambling system */
#define CA_ECD           1
#define CA_NDS           2
#define CA_DSS           4
} ca_descr_info_t;

typedef struct ca_caps {
	unsigned int slot_num;     /* total number of CA card and module slots */
	unsigned int slot_type;    /* OR of all supported types */
	unsigned int descr_num;    /* total number of descrambler slots (keys) */
	unsigned int descr_type;   /* OR of all supported types */
} ca_caps_t;

/* a message to/from a CI-CAM */
typedef struct ca_msg {
	unsigned int index;
	unsigned int type;
	unsigned int length;
	unsigned char msg[256];
} ca_msg_t;

typedef struct ca_descr {
	unsigned int index;
	unsigned int parity;	/* 0 == even, 1 == odd */
	unsigned char cw[8];
} ca_descr_t;

#ifdef CONFIG_AMLOGIC_DVB_COMPAT

/* CW type. */
enum ca_cw_type {
	CA_CW_DVB_CSA_EVEN,
	CA_CW_DVB_CSA_ODD,
	CA_CW_AES_EVEN,
	CA_CW_AES_ODD,
	CA_CW_AES_EVEN_IV,
	CA_CW_AES_ODD_IV,
	CA_CW_DES_EVEN,
	CA_CW_DES_ODD,
	CA_CW_SM4_EVEN,
	CA_CW_SM4_ODD,
	CA_CW_SM4_EVEN_IV,
	CA_CW_SM4_ODD_IV,
	CA_CW_TYPE_MAX
};

enum ca_dsc_mode {
	CA_DSC_CBC = 1,
	CA_DSC_ECB,
	CA_DSC_IDSA
};

struct ca_descr_ex {
	unsigned int index;
	enum ca_cw_type type;
	enum ca_dsc_mode mode;
	int          flags;
#define CA_CW_FROM_KL 1
	unsigned char cw[16];
};

/* add for support sc2 ca*/
enum ca_sc2_cmd_type {
	CA_ALLOC,
	CA_FREE,
	CA_KEY,
	CA_GET_STATUS,
	CA_SET_SCB,
	CA_SET_ALGO
};

enum ca_sc2_algo_type {
	CA_ALGO_AES_ECB_CLR_END,
	CA_ALGO_AES_ECB_CLR_FRONT,
	CA_ALGO_AES_CBC_CLR_END,
	CA_ALGO_AES_CBC_IDSA,
	CA_ALGO_CSA2,
	CA_ALGO_DES_SCTE41,
	CA_ALGO_DES_SCTE52,
	CA_ALGO_TDES_ECB_CLR_END,
	CA_ALGO_CPCM_LSA_MDI_CBC,
	CA_ALGO_CPCM_LSA_MDD_CBC,
	CA_ALGO_CSA3,
	CA_ALGO_ASA,
	CA_ALGO_ASA_LIGHT,
	CA_ALGO_S17_ECB_CLR_END,
	CA_ALGO_S17_ECB_CTS,
	CA_ALGO_UNKNOWN
};

enum ca_sc2_dsc_type {
	CA_DSC_COMMON_TYPE,
	CA_DSC_TSD_TYPE,	/*just support AES descramble.*/
	CA_DSC_TSE_TYPE		/*just support AES enscramble.*/
};

/**
 * struct ca_alloc - malloc ca slot index by params
 *
 * @pid:	slot use pid.
 * @algo:	use the algorithm
 * @dsc_type:	CA_DSC_COMMON_TYPE:support all ca_algo_type
 *		CA_DSC_TSD_TYPE & CA_DSC_TSE_TYPE just support AES
 * @ca_index:	return slot index.
 */
struct ca_sc2_alloc {
	unsigned int pid;
	enum ca_sc2_algo_type algo;
	enum ca_sc2_dsc_type dsc_type;
	unsigned int ca_index;
};

/**
 * struct ca_sc2_free - free slot index
 *
 * @ca_index:	need free slot index.
 */
struct ca_sc2_free {
	unsigned int ca_index;
};

enum ca_sc2_key_type {
	CA_KEY_EVEN_TYPE,
	CA_KEY_EVEN_IV_TYPE,
	CA_KEY_ODD_TYPE,
	CA_KEY_ODD_IV_TYPE,
	CA_KEY_00_TYPE,
	CA_KEY_00_IV_TYPE
};

/**
 * struct ca_sc2_key - set key slot index
 *
 * @ca_index:	use slot index.
 * @parity:	key type (odd/even/key00)
 * @key_index: key store index.
 */
struct ca_sc2_key {
	unsigned int ca_index;
	enum ca_sc2_key_type parity;
	unsigned int key_index;
};

/**
 * struct ca_sc2_scb - set scb
 *
 * @ca_index:	use slot index.
 * @ca_scb:	ca_scb (2bit)
 * @ca_scb_as_is:if 1, scb use orignal
 *				 if 0, use ca_scb
 */
struct ca_sc2_scb {
	unsigned int ca_index;
	unsigned char ca_scb;
	unsigned char ca_scb_as_is;
};

/**
 * struct ca_sc2_algo - set algo
 *
 * @ca_index:	use slot index.
 * @algo:	algo
 */
struct ca_sc2_algo {
	unsigned int ca_index;
	enum ca_sc2_algo_type algo;
};

/**
 * struct ca_sc2_descr_ex - ca externd descriptor
 *
 * @params:	command resource params
 */
struct ca_sc2_descr_ex {
	enum ca_sc2_cmd_type cmd;
	union {
		struct ca_sc2_alloc alloc_params;
		struct ca_sc2_free free_params;
		struct ca_sc2_key key_params;
		struct ca_sc2_scb scb_params;
		struct ca_sc2_algo algo_params;
	} params;
};

#endif /*CONFIG_AMLOGIC_DVB_COMPAT*/
typedef struct ca_pid {
	unsigned int pid;
	int index;		/* -1 == disable*/
} ca_pid_t;
#define CA_RESET          _IO('o', 128)
#define CA_GET_CAP        _IOR('o', 129, ca_caps_t)
#define CA_GET_SLOT_INFO  _IOR('o', 130, ca_slot_info_t)
#define CA_GET_DESCR_INFO _IOR('o', 131, ca_descr_info_t)
#define CA_GET_MSG        _IOR('o', 132, ca_msg_t)
#define CA_SEND_MSG       _IOW('o', 133, ca_msg_t)
#define CA_SET_DESCR      _IOW('o', 134, ca_descr_t)
#define CA_SET_PID        _IOW('o', 135, ca_pid_t)

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define CA_SET_DESCR_EX   _IOW('o', 200, struct ca_descr_ex)
#define CA_SC2_SET_DESCR_EX   _IOWR('o', 201, struct ca_sc2_descr_ex)
#endif

#endif
