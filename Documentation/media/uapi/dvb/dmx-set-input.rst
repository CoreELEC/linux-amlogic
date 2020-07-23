.. -*- coding: utf-8; mode: rst -*-

.. _DMX_SET_INPUT:

==============
DMX_SET_INPUT
==============

Name
----

DMX_SET_INPUT


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_SET_INPUT, dmx_input_source)
    :name: DMX_SET_INPUT


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``dmx_input_source``
    dmx_input_source,is local/local_sec/demod.


Description
-----------

This ioctl call allows to set dmx input source. from the input source
the demux will switch different sid.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
