.. -*- coding: utf-8; mode: rst -*-

.. _KEY_CONFIG:

==============
KEY_CONFIG
==============

Name
----

KEY_CONFIG


Synopsis
--------
.. code-block:: c

    int ioctl( int fd, KEY_CONFIG, struct key_config *config)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key.

``config``

    Pointer to structure containing key config.


Description
-----------

This ioctl call config used which descramble modue & algo.


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
