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

When LCD rendering is enabled, the init script unbinds fbcon and starts
lcdstatusd. Before its refresh loop, the daemon renders the configured initial
frame and writes the panel's ASCII DCS sequence 36 E8, a full frame, 36 C8,
then a second full frame. It logs command or frame errors and continues; this
panel's debugfs command can report a nonzero status even when accepted. Procd
forwards daemon stdout/stderr to the system log.

When LCD rendering is disabled in UCI, fbcon stays bound and the init script
runs a one-shot shell orientation sequence at boot, preserving the current
framebuffer image when it can read a complete frame. On stop, after the daemon
has exited and fbcon is rebound, the init script runs that same shell sequence
again because rebinding can reset panel orientation.

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
