# SPDX-License-Identifier: GPL-2.0-only
include $(TOPDIR)/rules.mk

PKG_NAME:=lcd-status
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=OpenWrt contributors

include $(INCLUDE_DIR)/package.mk

define Package/lcd-status
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Framebuffer LCD status display
  DEPENDS:=+uci
endef

define Package/lcd-status/description
Small configurable RGB565 framebuffer status display daemon.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/lcdstatusd.c $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) -std=gnu99 -Wall -Wextra \
		-o $(PKG_BUILD_DIR)/lcdstatusd $(PKG_BUILD_DIR)/lcdstatusd.c
endef

define Package/lcd-status/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lcdstatusd $(1)/usr/sbin/lcdstatusd
	$(INSTALL_BIN) ./files/lcdstatusd.init $(1)/etc/init.d/lcdstatusd
	$(INSTALL_CONF) ./files/lcdstatus $(1)/etc/config/lcdstatus
endef

$(eval $(call BuildPackage,lcd-status))
