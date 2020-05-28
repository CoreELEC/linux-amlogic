obj-$(CONFIG_AMLOGIC_DVB_EXTERN) += aml_dvb_extern.o

aml_dvb_extern-objs = \
		aml_demod_common.o \
		aml_tuner.o \
		aml_atvdemod.o \
		aml_dtvdemod.o

ccflags-y += -I.
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -I$(srctree)/include/media