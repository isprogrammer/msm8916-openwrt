# LCD status display

`lcd-status` runs `lcdstatusd`, a GPL-2.0-only RGB565 framebuffer renderer for
small SPI LCDs. It validates `/dev/fb0` geometry and pixel format, renders a
complete frame in memory, then writes the full frame on every refresh so DRM
fbdev deferred-I/O panels receive updates. The write loop handles partial
writes and EINTR.

The LuCI page previews unsaved display settings and supports ordered lines,
colors, font scale and 0/180 degree rotation. Dynamic sources include time,
uptime, LAN/WAN IP, Wi-Fi clients, CPU load, free memory, battery percentage,
power source, estimated remaining battery time, and a bounded `exec:` command.
The battery estimate uses the same `pm8916-lbc-chgr` online state, BMS capacity,
and `/etc/battery-session` discharge-rate formula as the Battery LuCI page.

`auto_off_s` defaults to `0` (never). A positive value turns off the backlight
after that many seconds without a power-key press. The shared power-button
handler writes an activity marker and its default short press toggles the LCD,
so a press wakes an automatically blanked panel and resets the timer.

At service startup the init script unbinds fbcon and invokes the daemon's
`--orientation-only` mode even if LCD rendering is disabled. It writes the
panel's ASCII DCS sequence `36 E8`, a full framebuffer frame, `36 C8`, then a
second full frame. The panel debugfs command may return `1` even when the
command is accepted. Normal daemon runs also apply orientation unless init
has already done so. Logs are forwarded to system log by procd stdout/stderr.

Example line:

```uci
config line
        option label 'BAT'
        option source 'estimated_remaining'
        option color 'FFFFFF'
```

Custom command output is trimmed to the display width and the command has a
2.5 second timeout. Treat configured commands as trusted root commands.

The init service reapplies the orientation sequence after unbinding fbcon on start and after rebinding fbcon on stop, because either transition can reset the panel.
