.. -*- coding: utf-8; mode: rst -*-

.. _KEY_ALLOC:

==============
KEY_ALLOC
==============

Name
----

KEY_ALLOC


Synopsis
--------
.. code-block:: c

     int ioctl(int fd, KEY_ALLOC, struct key_alloc *params)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key device.

``params``

    Pointer to structure containing filter parameters.


Description
-----------

This ioctl call alloc the key index according to is_iv
parameters provided. the key index return from parameter .


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
