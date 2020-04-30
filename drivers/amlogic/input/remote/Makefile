#
# Makefile for the remote control drivers
#

# Each configuration option enables a list of files.

obj-$(CONFIG_IR_XMP_DECODER)			+= remote_decoder_xmp.o
obj-$(CONFIG_AMLOGIC_MESON_REMOTE)		+= meson-remote.o

meson-remote-objs := remote_core.o remote_meson.o sysfs.o remote_cdev.o remote_raw.o remote_regmap.o
