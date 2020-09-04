.. -*- coding: utf-8; mode: rst -*-

.. _DMX_GET_FILTER_MEM_INFO:

==============
DMX_GET_FILTER_MEM_INFO
==============

Name
----

DMX_GET_FILTER_MEM_INFO


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_GET_FILTER_MEM_INFO, &mem_info)
    :name: DMX_GET_FILTER_MEM_INFO


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``mem_info``
    mem_info is struct dmx_filter_mem_info that all filter mem info in dmx.


Description
-----------

This ioctl call allows to get all filter mem info in one dmx device.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
