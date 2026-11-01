> 📖 **Full build & flashing guide →** <https://github.com/akbar-npj/msm8916-openwrt/wiki/Build-and-Flashing-Reference>

## v25.12.6-r4 — Feature highlights

- 🆕 **UF896 (v1.1) board support** — a new `uf896` target with its own diffconfig, device tree, and `uf896-modem-online` init script.
  > ⚠️ **Please help us test this.** We don't have a UF896 unit on the bench, so v1.1 support is **untested on real hardware**. If you own one, please report: does it boot, does the modem come up and register, and does the SIM-slot switch work? Feedback (good or bad) in the issues is very welcome. Note: UF896 ships **without** the `qcom,a2-pin` modem hold used on the HiMI V1.0 boards — field testing showed this board does not hit the `a2_power.c:1189` quiesce wedge, so the pin is deliberately omitted.

- 📶 **UF896 SIM-slot switch UI** — new `luci-app-uf896-simsel`: a web UI + watchdog for the UF896-only SIM-slot selection (`AT+SIMSEL`).

- 📦 **Kernel-module repository expanded (~80 new `kmod-*`)** — the upstream feeds reference ~85 `kmod-*` packages our targets never carried. Enabled the buildable ones on **all 8 boards** as `=m` (image size unchanged): QoS/shaper, tunnels, netfilter, storage, USB, legacy `iptables`/`xtables`, and audio.
  - **NFQUEUE** for zapret / zapret-manager / steer: `kmod-nft-queue` (nftables) + `kmod-ipt-nfqueue` (iptables) — **closes issue #24**.

- 🔧 **Kmod hotfix workflow** — new `publish-kmod.yml` rebuilds an existing module against the released kernel config and re-publishes the feed, so a driver fix ships without a full reflash. (It won't *add* a new module — that changes the kernel vermagic; new modules still need a firmware release.)

- 💾 **Full-device backup in the installer** — `flash.sh --mode backup` now dumps the whole device via EDL before flashing:
  - **full raw image** (`edl rf` → `./<device>-dump.bin`) or **partition-wise** (`edl rl` → `./<device>-dump/`);
  - dumps are **named after the device**, and the pre-flash check matches the right unit (it won't accept another device's dump);
  - the installer warns and asks for confirmation before flashing if no backup for this device is found.

## ⚠️ Upgrade note

This release adds kernel modules, so the kernel vermagic changed. Upgrade via the **full image** (*System → Firmware Upgrade*), not a bare `apk upgrade`, so the kernel and its modules stay in sync.

**Boards:** hmuf02v5, mf800b, mifib-v12, uf02, uf896, ufi001b, ufi003, uz801.


