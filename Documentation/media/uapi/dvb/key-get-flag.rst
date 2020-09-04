.. -*- coding: utf-8; mode: rst -*-

.. _KEY_GET_FLAG:

==============
KEY_GET_FLAG
==============

Name
----

KEY_GET_FLAG


Synopsis
--------
.. code-block:: c

    int ioctl( int fd, KEY_GET_FLAG, struct key_desc *params)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key.

``params``

    Pointer to structure containing key desc parameters.
    key[0] will return the flag


Description
-----------

This ioctl call return the key status for debug
the key status return from the key[0] in the struct key_descr

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
