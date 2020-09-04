.. -*- coding: utf-8; mode: rst -*-

.. _key_types:

****************
Key Types
****************

user_id
===========

.. c:type:: user_id

.. code-block:: c

    enum user_id {
	    DSC_LOC_DEC,    /* local descramble module */
    	DSC_NETWORK,    /* network descramble module */
	    DSC_LOC_ENC,    /* local cramble module */

    	CRYPTO_T0 = 0x100,  /* m2m en/decrypt */
    };


key_algo
==============

.. c:type:: key_algo


.. code-block:: c

    enum key_algo {
	    KEY_ALGO_AES,
    	KEY_ALGO_TDES,
    	KEY_ALGO_DES,
    	KEY_ALGO_CSA2,
    	KEY_ALGO_CSA3,
    	KEY_ALGO_NDL,
    	KEY_ALGO_ND
    };

struct key_descr
=================

.. c:type:: key_descr

.. code-block:: c

    struct key_descr {
	    unsigned int key_index; /*key_index, from the key_alloc*/
    	unsigned int key_len;   /*key_len, 8/16/32 in general*/
    	unsigned char key[32];
    };


struct key_config
============================

.. c:type:: key_config

.. code-block:: c

    struct key_config {
	    unsigned int key_index; /* key_index, from the key_alloc*/
    	int key_userid;         /* use descramble/enscramble module*/
    	int key_algo;           /* algo*/
    };

struct key_alloc
==============

.. c:type:: key_alloc

.. code-block:: c

    struct key_alloc {
	    int is_iv;              /*1: iv, 0: even/odd key */
    	unsigned int key_index; /*return key_index */
    };

