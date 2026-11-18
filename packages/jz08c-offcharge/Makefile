include $(TOPDIR)/rules.mk

PKG_NAME:=jz08c-offcharge
PKG_VERSION:=0.2
PKG_RELEASE:=1
PKGARCH:=all
include $(INCLUDE_DIR)/package.mk

define Package/jz08c-offcharge
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=JZ08C early off-charge display
  DEPENDS:=+kmod-drm-panel-mipi-dbi +jz08c-st7735s-firmware
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(PKG_BUILD_DIR)/jz08c-power-ui $(CURDIR)/src/jz08c-power-ui.c
endef

define Package/jz08c-offcharge/install
	$(INSTALL_DIR) $(1)/lib/preinit $(1)/usr/sbin $(1)/usr/share/jz08c-offcharge $(1)/usr/share/jz08c-offcharge/images
	$(INSTALL_BIN) ./files/lib/preinit/80_jz08c_offcharge $(1)/lib/preinit/
	$(INSTALL_DATA) ./files/usr/share/jz08c-offcharge/jz08c-board $(1)/usr/share/jz08c-offcharge/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/jz08c-power-ui $(1)/usr/sbin/
	$(INSTALL_DATA) ./files/usr/share/jz08c-offcharge/images/*.rgb565 $(1)/usr/share/jz08c-offcharge/images/
endef

$(eval $(call BuildPackage,jz08c-offcharge))
