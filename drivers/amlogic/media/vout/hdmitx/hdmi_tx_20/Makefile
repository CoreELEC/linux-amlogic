obj-$(CONFIG_AMLOGIC_HDMITX)		+= hdmitx20.o

ifdef CONFIG_DRM_MESON_HDMI
hdmitx20-objs := hdmi_tx_main_drm.o hdmi_tx_video.o hdmi_tx_audio.o hdmi_tx_edid.o \
                 hdmi_tx_audio.o hdmi_tx_hdcp.o hdmi_tx_scdc.o dv_emp_vsem.o
else
hdmitx20-objs := hdmi_tx_main.o hdmi_tx_video.o hdmi_tx_audio.o hdmi_tx_edid.o \
		 hdmi_tx_audio.o hdmi_tx_hdcp.o hdmi_tx_scdc.o dv_emp_vsem.o
endif

obj-$(CONFIG_AMLOGIC_HDMITX) += hw/


#EXTRA_CFLAGS += -O2
