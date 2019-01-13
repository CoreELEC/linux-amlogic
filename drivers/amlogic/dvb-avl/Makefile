
#	AVL6862 DVB-Sx/Tx/C frontend driver

obj-m += aml.o aml_fe.o avl6862.o r848a.o ftm4862_tuner.o
#obj-m += mxl608.o rda5815m.o
ftm4862_tuner-objs = mxl608.o rda5815m.o tuner_ftm4862.o
aml-objs=aml_dvb.o aml_dmx.o 

EXTRA_CFLAGS += -I. -DDUAL_TUNER
EXTRA_CFLAGS += -Idrivers/media/dvb-core -Idrivers/media/usb/dvb-usb -Idrivers/media/dvb-frontends -Idrivers/media/tuners

