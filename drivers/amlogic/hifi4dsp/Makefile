# Makefile for hifi4dsp

obj-$(CONFIG_AMLOGIC_HIFI4DSP)    +=	hifi4dsp_module.o \
		hifi4dsp_firmware.o \
		hifi4dsp_dsp.o \
		dsp_top.o

ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif

#obj-$(CONFIG_AMLOGIC_HIFI4DSP)    +=hifi4dsp_test.o

