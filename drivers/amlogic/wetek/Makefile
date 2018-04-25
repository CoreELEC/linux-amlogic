#
# Makefile for the Wetek Play driver.
#

obj-$(CONFIG_WETEK) += wetekplay.o wetekdvb.o

wetekplay-objs = nimdetect.o mxl603.o avl6211.o mn88436.o cxd2837.o cxd2841er_wetek.o ascot3.o

wetekdvb-objs = wetek_dvb.o wetek_dmx.o 

EXTRA_CFLAGS += -Idrivers/media/dvb-core -Idrivers/media/tuners
