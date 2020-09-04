.. -*- coding: utf-8; mode: rst -*-

.. _KEY_SET:

==============
KEY_SET
==============

Name
----

KEY_SET


Synopsis
--------
.. code-block:: c

    int ioctl( int fd, KEY_SET, struct key_descr *key)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key.

``key``

    Pointer to structure containing key parameters.


Description
-----------

This ioctl call update the key contents according to the key index.
if update the odd/even key, you can call this ioctl to update key to
key_table, but this just for ree mode, not ta mode.


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
