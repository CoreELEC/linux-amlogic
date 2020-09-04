.. -*- coding: utf-8; mode: rst -*-

.. _KEY_FREE:

==============
KEY_FREE
==============

Name
----

KEY_FREE


Synopsis
--------
.. code-block:: c

    int ioctl(int fd, KEY_FREE, int key_index)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key.

``key_index``

    key_index that come from the key_alloc.


Description
-----------
 this ioctl will free the key index

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
