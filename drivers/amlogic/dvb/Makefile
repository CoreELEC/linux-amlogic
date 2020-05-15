obj-$(CONFIG_AMLOGIC_DVB_COMPAT) += aml_dvb_common_fe.o

aml_dvb_common_fe-objs = \
		aml_demod_common.o \
		aml_tuner.o \
		aml_atvdemod.o \
		aml_dtvdemod.o

ccflags-y += -I.
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -I$(srctree)/include/media