include $(TOPDIR)/rules.mk

PKG_NAME:=jz08c-st7735s-firmware
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

define Package/jz08c-st7735s-firmware
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=JZ08C V51 ST7735S panel initialization firmware
endef

define Build/Compile
endef

define Package/jz08c-st7735s-firmware/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) ./files/sitronix,st7735s-jz08c-v51.bin $(1)/lib/firmware/
endef

$(eval $(call BuildPackage,jz08c-st7735s-firmware))
