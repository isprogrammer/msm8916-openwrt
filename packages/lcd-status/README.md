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

The panel driver's kernel enable path applies the JZ08C orientation sequence
before enabling the backlight, so each probe/re-enable starts in the correct
orientation. The daemon unbinds fbcon while it owns /dev/fb0 and writes each
complete frame to trigger DRM fbdev deferred I/O.

During procd shutdown, procd exports whether the system is rebooting or powering
off. The init hook signals the daemon, which draws REBOOTING... or the
POWERING OFF... image, turns on the backlight, then forks a small holder that
keeps fb0 open until procd's final system shutdown signal. This avoids a later
fbcon rebind or daemon stop replacing the shutdown frame. If the LCD is disabled,
the hook starts the daemon in shutdown-only mode to display the same message.

Example line:

```uci
config line
        option label 'BAT'
        option source 'estimated_remaining'
        option color 'FFFFFF'
```

Custom command output is trimmed to the display width and the command has a
2.5 second timeout. Treat configured commands as trusted root commands.
