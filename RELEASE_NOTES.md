# Release Notes

> 📖 **Full build & flashing guide →** <https://github.com/akbar-npj/msm8916-openwrt/wiki/Build-and-Flashing-Reference>

### v25.12.6-r4 (2026-10-31) — built from `main`

> 🆕 **Kernel-module repository expanded** — ~80 previously-missing `kmod-*` packages are now built and published, so packages that depend on them install cleanly. Closes issue #24 (NFQUEUE for zapret / zapret-manager / steer on HMUF02-V5).

- 📦 **Kmod feed gap closed (fix, issue #24)** — the upstream base / packages / luci / routing / telephony feeds reference ~85 `kmod-*` packages our targets feed never carried, because our kernel config never enabled the drivers (no `.ko`, no package). Enabled the buildable ones on **all eight boards** — QoS / shapers, tunnels, netfilter, storage, USB, legacy `iptables`/`xtables`, audio, and **NFQUEUE** (`kmod-nft-queue` was already on; added `kmod-ipt-nfqueue` for the iptables path zapret/steer use) — all as `=m`, so image size is unchanged. `luci-app-ledtrig-usbport`, `openvswitch`, its `keepalived` deps, and the `iptables-mod-*` set now `apk add` successfully.
- 🔧 **Kmod hotfix workflow (new)** — `publish-kmod.yml` rebuilds an existing kernel module against the released kernel config and re-publishes the feed, so a driver fix ships without a full reflash. It deliberately refuses to *add* a new module (that changes the kernel vermagic and would be rejected by released devices); new modules still need a firmware release.
- 📶 **UF896 SIM-slot UI (new)** — `luci-app-uf896-simsel` web UI + watchdog for the UF896-only SIM-slot switch (`AT+SIMSEL`).

**⚠️ Upgrade note**: this release adds kernel modules, so the kernel vermagic changed. Upgrade via the *System → Firmware Upgrade* page (full image), not a bare `apk upgrade`, so the new kernel and its modules stay in sync.

**Packages**: kmod feed expanded across all boards (incl. NFQUEUE for issue #24); `luci-app-uf896-simsel` added (UF896).
