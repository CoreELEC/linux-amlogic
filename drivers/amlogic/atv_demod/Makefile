obj-$(CONFIG_AMLOGIC_ATV_DEMOD) += atvdemod_fe.o

atvdemod_fe-objs = atvdemod_func.o \
		atvauddemod_func.o \
		atv_demod_v4l2.o \
		atv_demod_driver.o \
		atv_demod_ops.o \
		atv_demod_afc.o \
		atv_demod_monitor.o \
		atv_demod_access.o \
		atv_demod_debug.o \
		atv_demod_isr.o \
		atv_demod_ext.o \

ccflags-y += -I.
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -I$(srctree)/drivers/media/tuners
ccflags-y += -I$(srctree)/include/media
ccflags-y += -I$(srctree)/sound/soc/amlogic/auge
ccflags-y += -I$(srctree)/sound/soc/amlogic/meson
ccflags-y += -I$(srctree)/drivers/amlogic/media/vin/tvin/tvafe