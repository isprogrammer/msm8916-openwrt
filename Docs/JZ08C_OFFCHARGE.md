# JZ08C off-charge display

This describes the JZ08C-specific early charger UI and the per-device lk2nd
charger-mode setting. It complements [The power button](POWER_BUTTON.md).

This package is selected only by the generic-jz08c-v51 DEVICE_PACKAGES stanza.
It handles charger-origin boots only when lk2nd has placed the exact token
androidboot.mode=charger in /proc/cmdline.

## One-time bootloader setup

On each JZ08C device, enable the persistent lk2nd charger-screen flag once:

    fastboot oem off-mode-charge 1

Check the stored flag with fastboot getvar off-mode-charge (expect 1). To
disable the feature, use:

    fastboot oem off-mode-charge 0

With the flag disabled or absent, lk2nd does not add
androidboot.mode=charger; the package preinit hook is skipped and the device
performs its normal OpenWrt boot while on the charger. Hardware charging is
separate from displaying the charge UI.

The cached lk2nd source registers this command and stores
charger_screen_enabled in device-info. The JZ08C stock GPT has no separate
devinfo partition; lk2nd falls back to reserved space at the end of aboot.
If device-info has no valid magic, lk2nd initializes this flag to disabled.
No per-image lk2nd change or USER_BUILD_VARIANT build is needed.

The package does not infer charger origin from current VBUS. On charger boot,
its preinit hook loads the panel and draws the charge screen before normal
services. Unplug requests poweroff; KEY_POWER returns to ordinary boot.

## Alternatives reviewed

USER_BUILD_VARIANT=true changes verified-boot fastboot policy and other shared
lk2nd behavior; the project builds a common msm8916 lk2nd artifact. It is not
used for this package. Reading PON_REASON1 from Linux regmap debugfs is future work only: it would
require debugfs and confirmation that the PMIC reason remains readable after
lk2nd has read it. This package does not use or rely on that alternative; no
Linux-side PON reader is implemented.

The GPIO backlight node uses the existing MPP4 sink configuration. Its 40 mA
electrical behavior still requires hardware validation.
