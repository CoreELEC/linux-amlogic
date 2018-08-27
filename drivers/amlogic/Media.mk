ifeq ($(KERNEL_A32_SUPPORT), true)
KERNEL_ARCH := arm
else
KERNEL_ARCH := arm64
endif

CONFIGS := CONFIG_AMLOGIC_MEDIA_VDEC_MPEG12=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MPEG2_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MPEG4=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MPEG4_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_VC1=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H264=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H264_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H264_MVC=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H265=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_VP9=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MJPEG=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MJPEG_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_REAL=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_AVS=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_AVS2=m \
	CONFIG_AMLOGIC_MEDIA_VENC_H264=m \
	CONFIG_AMLOGIC_MEDIA_VENC_H265=m \
	CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION=y
        CONFIG_AMLOGIC_MEDIA_GE2D=y

define copy-media-modules
$(foreach m, $(shell find $(strip $(1)) -name "*.ko"),\
	$(shell cp $(m) $(strip $(2)) -rfa))
endef

ifneq (,$(TOP))
KDIR := $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ/

MEDIA_DRIVERS := $(TOP)/hardware/amlogic/media_modules/drivers
ifeq (,$(wildcard $(MEDIA_DRIVERS)))
$(error No find the dir of drivers.)
endif

INCLUDE := $(MEDIA_DRIVERS)/include
ifeq (,$(wildcard $(INCLUDE)))
$(error No find the dir of include.)
endif

MEDIA_MODULES := $(shell pwd)/$(PRODUCT_OUT)/obj/media_modules
ifeq (,$(wildcard $(MEDIA_MODULES)))
$(shell mkdir $(MEDIA_MODULES) -p)
endif

MODS_OUT := $(shell pwd)/$(PRODUCT_OUT)/obj/lib_vendor
ifeq (,$(wildcard $(MODS_OUT)))
$(shell mkdir $(MODS_OUT) -p)
endif

UCODE_OUT := $(shell pwd)/$(PRODUCT_OUT)/$(TARGET_COPY_OUT_VENDOR)/lib/firmware/video
ifeq (,$(wildcard $(UCODE_OUT)))
$(shell mkdir $(UCODE_OUT) -p)
endif

$(shell cp $(MEDIA_DRIVERS)/../firmware/* $(UCODE_OUT) -rfa)
$(shell cp $(MEDIA_DRIVERS)/* $(MEDIA_MODULES) -rfa)

define media-modules
	@$(MAKE) -C $(KDIR) M=$(MEDIA_MODULES) ARCH=$(KERNEL_ARCH) \
	CROSS_COMPILE=$(PREFIX_CROSS_COMPILE) $(CONFIGS) \
	EXTRA_CFLAGS+=-I$(INCLUDE) modules; \
	find $(MEDIA_MODULES) -name "*.ko" | xargs -i cp {} $(MODS_OUT)
endef

else
KDIR := $(PWD)/kernel
ifeq (,$(wildcard $(KDIR)))
$(error No find the dir of kernel.)
endif

MEDIA_DRIVERS := $(PWD)/media_modules/drivers
ifeq (,$(wildcard $(MEDIA_DRIVERS)))
$(error No find the dir of drivers.)
endif

INCLUDE := $(MEDIA_DRIVERS)/include
ifeq (,$(wildcard $(INCLUDE)))
$(error No find the dir of include.)
endif

MODS_OUT ?= $(MEDIA_DRIVERS)/../modules
ifeq (,$(wildcard $(MODS_OUT)))
$(shell mkdir $(MODS_OUT) -p)
endif

ifeq ($(KERNEL_A32_SUPPORT), true)
TOOLS := /opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
else
TOOLS := /opt/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

modules:
	@$(MAKE) -C $(KDIR) M=$(MEDIA_DRIVERS) ARCH=$(KERNEL_ARCH) \
	CROSS_COMPILE=$(TOOLS) $(CONFIGS) \
	EXTRA_CFLAGS+=-I$(INCLUDE) -j64

copy-modules:
	@echo "start copying media modules."
	mkdir -p $(MODS_OUT)
	$(call copy-media-modules, $(MEDIA_DRIVERS), $(MODS_OUT))

all: modules copy-modules

clean:
	$(MAKE) -C $(KDIR) M=$(MEDIA_DRIVERS) ARCH=$(KERNEL_ARCH) clean

endif
