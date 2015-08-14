imx-drm-y := imx-drm-core.o

imx-drm-$(CONFIG_DRM_IMX_PARALLEL_DISPLAY) += parallel-display.o
imx-drm-$(CONFIG_DRM_IMX_TVE) += imx-tve.o
imx-drm-$(CONFIG_DRM_IMX_LDB) += imx-ldb.o
imx-drm-$(CONFIG_DRM_IMX_IPUV3) += ipuv3-crtc.o ipuv3-plane.o
imx-drm-$(CONFIG_DRM_IMX_HDMI) += dw_hdmi-imx.o

obj-$(CONFIG_DRM_IMX) += imx-drm.o
