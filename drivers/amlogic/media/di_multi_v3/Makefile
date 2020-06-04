# # Makefile for the Post Process Manager device #
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -D DEBUG_SUPPORT
ccflags-y := -DDEBUG
else
ccflags-y := -DDEBUG
endif
ccflags-y += -I.
CFLAGS_dimv3.o := -I$(src)
obj-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += dimv3.o
dimv3-objs += deinterlace.o
dimv3-objs += deinterlace_hw.o
#dimv3-objs += deinterlace_mtn.o
dimv3-objs += deinterlace_dbg.o
#dimv3-objs += nr_drv.o
#dimv3-objs += pulldown_drv.o
#dimv3-objs += detect3d.o
dimv3-objs += nr_downscale.o
dimv3-objs += di_pps.o
dimv3-objs += di_vframe.o
dimv3-objs += di_prc.o
dimv3-objs += di_pre.o
dimv3-objs += di_pres.o
dimv3-objs += di_pre_hw.o
dimv3-objs += di_post.o
dimv3-objs += di_reg_tab.o
dimv3-objs += di_dbg.o
dimv3-objs += di_que.o
dimv3-objs += di_sys.o
dimv3-objs += di_task.o
dimv3-objs += di_interface.o
dimv3-objs += di_api.o
#dimv3-objs += di_que_buf.o
#dimv3-objs += film_mode_fmw/vof_soft_top.o
#dimv3-objs += film_mode_fmw/flm_mod_xx.o
#dimv3-objs += film_mode_fmw/film_fw1.o
ccflags-y += -Idrivers/amlogic/media/common/rdma/
ccflags-y += -I$(src)