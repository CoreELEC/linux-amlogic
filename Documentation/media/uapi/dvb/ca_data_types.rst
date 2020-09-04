.. -*- coding: utf-8; mode: rst -*-

.. _ca_data_types:

*************
CA Data Types
*************


ca_slot_info_t
==============

.. c:type:: ca_slot_info

.. code-block:: c

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

ca_descr_info_t
===============

.. c:type:: ca_descr_info

.. code-block:: c

    typedef struct ca_descr_info {
	unsigned int num;  /* number of available descramblers (keys) */
	unsigned int type; /* type of supported scrambling system */
    #define CA_ECD           1
    #define CA_NDS           2
    #define CA_DSS           4
    } ca_descr_info_t;

ca_caps_t
=========

.. c:type:: ca_caps

.. code-block:: c

    typedef struct ca_caps {
	unsigned int slot_num;  /* total number of CA card and module slots */
	unsigned int slot_type; /* OR of all supported types */
	unsigned int descr_num; /* total number of descrambler slots (keys) */
	unsigned int descr_type;/* OR of all supported types */
     } ca_cap_t;


ca_msg_t
========

.. c:type:: ca_msg

.. code-block:: c

    /* a message to/from a CI-CAM */
    typedef struct ca_msg {
	unsigned int index;
	unsigned int type;
	unsigned int length;
	unsigned char msg[256];
    } ca_msg_t;

ca_descr_t
==========

.. c:type:: ca_descr

.. code-block:: c

    typedef struct ca_descr {
	unsigned int index;
	unsigned int parity;
	unsigned char cw[8];
    } ca_descr_t;


ca_pid_t
========

.. c:type:: ca_pid

.. code-block:: c

    typedef struct ca_pid {
	unsigned int pid;
	int index;      /* -1 == disable*/
    } ca_pid_t;

ca_sc2_algo_type
================

.. c:type:: ca_sc2_algo_type

.. code-block:: c

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
	    CA_ALGO_UNKNOWN
    };

ca_sc2_dsc_type
===============

.. c:type:: ca_sc2_dsc_type

.. code-block:: c

    /*Hardware descrmalber module type.*/
    enum ca_sc2_dsc_type {
        CA_DSC_COMMON_TYPE, /*TSN*/
        CA_DSC_TSD_TYPE,    /*TSD*/
        CA_DSC_TSE_TYPE     /*TSE*/
    };

ca_sc2_alloc
============

.. c:type:: ca_sc2_alloc

.. code-block:: c

    /*Descrambler allocation parameters.*/
    struct ca_sc2_alloc {
        unsigned int pid;              /*PID*/
        enum ca_sc2_algo_type algo;    /*Crypto algorithm*/
        enum ca_sc2_dsc_type dsc_type; /*Hardware module*/
        unsigned int ca_index;         /*Return the descrambler slot's index*/
    };

ca_sc2_free
===========

.. c:type:: ca_sc2_free

.. code-block:: c

    /*Descrambler free parameters.*/
    struct ca_sc2_free {
        unsigned int ca_index; /*The descramlber slot's index to be freed*/
    };

ca_sc2_key_type
===============

.. c:type:: ca_sc2_key_type

.. code-block:: c

    /*Key type*/
    enum ca_sc2_key_type {
        CA_KEY_EVEN_TYPE,    /*Even key*/
        CA_KEY_EVEN_IV_TYPE, /*IV data for even key*/
        CA_KEY_ODD_TYPE,     /*Odd key*/
        CA_KEY_ODD_IV_TYPE,  /*IV data for odd key*/
        CA_KEY_00_TYPE,      /*Key for packets scrambling control == 0*/
        CA_KEY_00_IV_TYPE    /*IV data for CA_KEY_00_TYPE*/
    };

ca_sc2_key
==========

.. c:type:: ca_sc2_key

.. code-block:: c

    /*Key setting parameters.*/
    struct ca_sc2_key {
        unsigned int ca_index;       /*The descrambler slot's index*/
        enum ca_sc2_key_type parity; /*The key's type*/
        unsigned int key_index;      /*The key's index in the key table*/
    };

ca_sc2_cmd_type
===========
.. c:type:: ca_sc2_cmd_type

.. code-block:: c

    /* add for support sc2 ca*/
    enum ca_sc2_cmd_type {
        CA_ALLOC, /*Allocate a new descrambler slot.*/
        CA_FREE,  /*Free a descrambler slot*/
        CA_KEY,    /*Set a descrambler slot's key*/
	    CA_GET_STATUS, /*get ca pid status*/
    	CA_SET_SCB,   /*control the scb bit*/
	    CA_SET_ALGO   /*control algo independently*/
    };

ca_sc2_scb
===========

.. c:type:: ca_sc2_scb

.. code-block:: c

    struct ca_sc2_scb {
	    unsigned int ca_index;
    	unsigned char ca_scb; /*ca_scb (2bit)*/
	    unsigned char ca_scb_as_is; /*if 1, scb use orignal; if 0, use ca_scb*/
    };

ca_sc2_algo
===========

.. c:type:: ca_sc2_algo

.. code-block:: c

    struct ca_sc2_algo {
    	unsigned int ca_index;
    	enum ca_sc2_algo_type algo;
    };

ca_sc2_descr_ex
===========

.. c:type:: ca_sc2_descr_ex

.. code-block:: c

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
