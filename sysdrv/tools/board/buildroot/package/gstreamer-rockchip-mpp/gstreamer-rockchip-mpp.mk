################################################################################
#
# gstreamer-rockchip-mpp
#
################################################################################

GSTREAMER_ROCKCHIP_MPP_VERSION = rk3588/bookworm
GSTREAMER_ROCKCHIP_MPP_SITE = https://github.com/luka177/gstreamer-rockchip-rv1106.git
GSTREAMER_ROCKCHIP_MPP_SITE_METHOD = git

#GSTREAMER_ROCKCHIP_MPP_DEPENDENCIES = gstreamer1 gstreamer1-plugins-base host-pkgconf
GSTREAMER_ROCKCHIP_MPP_DEPENDENCIES = gst1-plugins-base

# Set SDK path manually
LUCKFOX_PICO_SDK_DIR = /home/stranger/luckfox-pico

GSTREAMER_ROCKCHIP_MPP_MAKE_ENV = \
    LUCKFOX_PICO_SDK_DIR=$(LUCKFOX_PICO_SDK_DIR) \
    CC="$(TARGET_CC)" \
    LD="$(TARGET_LD)" \
    PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)"

define GSTREAMER_ROCKCHIP_MPP_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(GSTREAMER_ROCKCHIP_MPP_MAKE_ENV) \
		$(MAKE) -C $(@D)/gst/rockchipmpp
endef

define GSTREAMER_ROCKCHIP_MPP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/gst/rockchipmpp/libgstrockchipmpp.so \
		$(TARGET_DIR)/usr/lib/gstreamer-1.0/libgstrockchipmpp.so
endef

$(eval $(generic-package))
