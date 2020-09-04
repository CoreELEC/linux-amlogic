.. -*- coding: utf-8; mode: rst -*-

.. _DMX_SET_HW_SOURCE:

==============
DMX_SET_HW_SOURCE
==============

Name
----

DMX_SET_HW_SOURCE


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_SET_HW_SOURCE, source)
    :name: DMX_SET_HW_SOURCE


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``source``
    source is DMA0~7/FRONTEND_TS0~7.


Description
-----------

This ioctl call allows to set dmx input source. it connect to one stream.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
