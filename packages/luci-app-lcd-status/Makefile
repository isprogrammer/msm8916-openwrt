# SPDX-License-Identifier: GPL-2.0-only
include $(TOPDIR)/rules.mk

LUCI_TITLE:=LCD status display configuration
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
LUCI_DEPENDS:=+luci-base +lcd-status
LUCI_PKGARCH:=all
PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=OpenWrt contributors

# call BuildPackage is emitted by luci.mk; keep this marker for package scanning
include $(TOPDIR)/feeds/luci/luci.mk
