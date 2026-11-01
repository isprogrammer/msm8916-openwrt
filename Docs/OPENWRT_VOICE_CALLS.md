# Voice calls on MSM8916 OpenWrt sticks: what is needed for a dialer

A standalone note for the msm8916-openwrt project. It summarises what we
learned making voice calls work on an HMUF02-V05 (HiMI MSM8916 stick) under
Android 13 with the downstream 3.10 kernel, and what an OpenWrt dialer would
need on top of ModemManager. Status: 2026-11-01.

Short version:

- **Call control is solved by ModemManager.** Dial, answer, hang up, hold,
  call waiting and DTMF all work through its QMI voice support.
- **Call audio is the missing part on OpenWrt.** On MSM8916 the voice
  vocoder runs in the Q6 DSP inside the modem subsystem; the audio never
  passes through Linux unless you ask for it. Linux only has to start the
  DSP's voice session and choose where it goes. The msm8916-openwrt kernel
  has none of that yet (`CONFIG_QCOM_APR` is off, no QDSP6 ASoC drivers, no
  q6voice).
- **A stick has no microphone or speaker,** so the real question is where
  the call audio should go: a Bluetooth headset, a PC over USB, a SIP phone,
  a USB sound card, or files. Each needs different work (§4).
- **VoLTE is a separate problem** and depends on the modem firmware (§6).

## 1. What we tested

Hardware: HMUF02-V05 (MSM8916, WTR1605 RF, modem `MPSS.DPM.1.0.c7-00193`),
a SIM on an LTE network that uses CSFB to WCDMA for voice.

| Feature | Result on the downstream 3.10 kernel |
|---|---|
| Outgoing call | works (CSFB from LTE to WCDMA) |
| Incoming call, caller hears ringback, answer | works, **only with a QMI voice client registered** (see §2) |
| Call waiting, hold, swap | works |
| Two-way call audio on a Bluetooth headset | works (internal BT SCO port, CVSD 8 kHz), but audio starts a few seconds after the call connects (§3.1) |
| Record the call (downlink, uplink, both) to WAV | works (DSP in-call record taps) |
| Play a WAV file to the other party | works (DSP in-call music mix) |
| 10-minute call | works, but see the power notes in §7 |
| VoLTE | not working on this modem (see §6) |

On OpenWrt the user had earlier made and answered 3G calls on the same stick
with ModemManager (`mmcli`), with no audio.

## 2. Call control: ModemManager is enough

ModemManager's QMI voice support covers what a dialer needs:

```sh
mmcli -m any --voice-list-calls
mmcli -m any --voice-create-call="number=+15551234567"   # prints the call path
mmcli -o <call> --start            # dial
mmcli -o <call> --accept           # answer an incoming call
mmcli -o <call> --hangup
mmcli -o <call> --send-dtmf=1
mmcli -m any --voice-hold-and-accept      # hold the active call, take the waiting one
mmcli -m any --voice-hangup-and-accept
mmcli -m any --voice-hangup-all
mmcli -o <call>                    # state: ringing-in, active, held, waiting, terminated…
```

Things we learned that matter here:

- **Incoming calls need a QMI voice client.** A 3GPP incoming call first
  reaches the modem's call manager in a SETUP state. The modem confirms it
  (and the network plays ringback to the caller) only while a QMI client is
  registered for voice call indications. The AT command interpreter does not
  do this: with AT only, the call shows up in `AT+CLCC`, no `RING` is sent,
  `ATA` fails, and the caller hears silence until the network gives up after
  about 10 s. ModemManager registers for these indications, so it is fine;
  a home-grown AT-only dialer is not.
- **Hang up with `AT+CHUP` (or QMI), not `ATH`.** On this modem `ATH`
  doesn't end a voice call.
- **Only one process should own the AT ports.** Concurrent open/close of an
  SMD AT port desynchronised the 3.10 `smd_tty` driver and later crashed the
  SoC. On OpenWrt, go through ModemManager (`mmcli --command=…` for raw AT)
  rather than opening the ports from scripts in parallel.
- **The HiMI firmware's NV 2500 check** (fatal error about 15 min after each
  modem boot unless NV 2500 holds `HiMI_OK`) also applies to calls. The
  project's `himi-ok-guard` already handles it.

## 3. How call audio works on MSM8916

```
            ModemManager (QMI voice: call control)
                     │
 modem subsystem ────┴──────────────────────────────────────────────┐
 │  Q6 DSP: CS voice session (vocoder, AMR/CVSD, echo control)       │
 │     │ AFE ports                                                    │
 │     ├── PRI_MI2S_RX / TERT_MI2S_TX ──▶ PM8916 codec (earpiece, mic)│
 │     ├── INT_BT_SCO_RX / _TX ─────────▶ WCNSS/Pronto ─▶ BT headset  │
 │     ├── VOC_REC_DL / VOC_REC_UL ─────▶ MultiMedia FE ─▶ Linux (rec)│
 │     └── Incall_Music ◀────────────── MultiMedia FE ◀── Linux (play)│
 └────────────────────────────────────────────────────────────────────┘
```

- The Q6 DSP is part of the modem subsystem on MSM8916. Once the modem
  runs, it owns LPASS, so all audio has to go through the DSP (the
  DSP-bypass "LPASS only" audio that works without the modem stops working).
- **The voice session runs while a "hostless" voice front end is open.** On
  the downstream kernel that is the `CS-Voice` PCM device (00-02):
  `tinyhostless -D 0 -P 2 -C 2 -p 512 -n 2 -c 1 -r 8000` opens playback and
  capture without moving any data. The mixer then routes the session to a
  back end. Minimum period: 512 frames. `cat /proc/asound/pcm` lists the
  front ends (`00-02 CS-Voice`, `00-15 VoLTE`, …).
- **We always routed the session to a back end:** the BT SCO port when a
  headset was connected, otherwise the PM8916 codec, even with nothing
  attached to it, so recording and playback into the call still worked. We
  never tried a voice session with no back end routed.
- **Routes we used** (control names from the stock Android
  `mixer_paths_mtp.xml`, downstream kernel):

  ```sh
  # Bluetooth headset (SCO link opened by the BT stack, CVSD 8 kHz)
  tinymix 'Internal BTSCO SampleRate' 8000
  tinymix 'INTERNAL_BT_SCO_RX_Voice Mixer CSVoice' 1
  tinymix 'Voice_Tx Mixer INTERNAL_BT_SCO_TX_Voice' 1
  # PM8916 codec (earpiece out, ADC1 in)
  tinymix 'PRI_MI2S_RX_Voice Mixer CSVoice' 1
  tinymix 'Voice_Tx Mixer TERT_MI2S_TX_Voice' 1
  tinymix 'RX1 MIX1 INP1' RX1; tinymix 'RDAC2 MUX' RX1; tinymix 'EAR_S' Switch
  tinymix 'DEC1 MUX' ADC1
  # start the voice session (keep running for the whole call)
  tinyhostless -D 0 -P 2 -C 2 -p 512 -n 2 -c 1 -r 8000 &
  # record both directions (MultiMedia1, PCM 0) and play into the call (MultiMedia2, PCM 1)
  tinymix 'MultiMedia1 Mixer VOC_REC_DL' 1; tinymix 'MultiMedia1 Mixer VOC_REC_UL' 1
  tinycap rec.wav -D 0 -d 0 -c 2 -r 8000 -b 16
  tinymix 'Incall_Music Audio Mixer MultiMedia2' 1
  tinyplay prompt-8k.wav -D 0 -d 1
  ```
  Undo by setting the same controls to 0 and killing `tinyhostless`.
  `VOC_REC_DL` is what the local user hears, `VOC_REC_UL` what they send.
  The in-call music stream is mixed into the uplink, so the other party
  hears it.
- **Where the control names come from:** the stock Android 4.4 firmware's
  `/system/etc/mixer_paths_mtp.xml` (paths "voice-call",
  "voice-call bt-sco", "incall-rec-*", "incall-music"). Mainline q6routing
  and q6voice use other names.
- No ACDB calibration files were loaded; voice worked anyway with default
  DSP settings.
- **Bluetooth detail:** WCNSS rejects HCI "Enhanced Setup Synchronous
  Connection"; the host stack must use the legacy eSCO/SCO setup (on Android:
  `bluetooth.sco.disable_enhanced_connection=true`). The SCO samples go from
  Pronto straight to the DSP's internal BT SCO port; the host never sees
  them.
- **The SCO link needs no telephony call on the BT side.** On Android we
  opened it with `BluetoothHeadset.startScoUsingVirtualVoiceCall()`, the
  "virtual voice call" VoIP apps use: the audio gateway tells the headset
  a call is up and opens SCO. A BlueZ-based HFP audio gateway can do the
  same.
- **After a modem crash** (subsystem restart) the modem came back and
  registered, but the DSP voice session (APR MVM/CVS) failed until the
  next reboot (downstream kernel; not checked on mainline).

### 3.1 Audio starts late, and how to avoid it

In our PoC the call audio reaches the headset a few seconds after the call
connects. We did not measure it precisely. The delay adds up from three
places:

1. **Polling.** The daemon polled `AT+CLCC` every 2 s and only then started
   the routing: up to 2 s, plus the AT round trip. It started at `dialing`
   for outgoing calls, but only at `active` for incoming ones.
2. **Sequential start.** The SCO link was requested only at that point.
   Opening SCO (the AG signals a call to the headset, then the baseband sets
   up the link) takes a noticeable part of a second or more, depending on
   the headset. Some headsets also play a tone first.
3. **DSP session start.** Opening the hostless FE starts the voice session
   (MVM/CVS attach); this is the shortest part.

How to have the audio path ready when the call connects:

- **React to events, don't poll.** ModemManager emits `CallAdded` and the
  call's `StateChanged` D-Bus signals (they come from QMI voice
  indications); use them directly. Also run the audio hook from the dialer's
  own `accept`/`dial` actions, before waiting for the state change.
- **Start early.** For outgoing calls, start the routing at `dialing`, not at
  `active`; the user then also hears in-band ringback and network
  announcements. For incoming calls, open the SCO link while it is still
  `ringing-in` (HFP in-band ringing), so the link is up when the user
  answers. Mixer routes can be set at any time; they carry nothing until the
  session runs.
- **Do the steps in parallel.** Set the mixer routes, open the hostless FE
  and request SCO at the same time. Don't wait for one step before starting
  the next.
- **Keep the HFP service-level connection up** while the stick is idle, so
  only the SCO link (not RFCOMM/SLC) has to be created per call. Under
  BlueZ, `Connect` the headset at boot and reconnect on drop.
- **Optional, a fixed cost in power:** keep the SCO link open for the whole
  time the dialer is in use (headset in "call" mode). That removes the SCO
  setup time entirely, but the headset battery drains faster and other
  audio on the headset stops.

## 4. What OpenWrt needs for call audio

### 4.1 Kernel

Mainline MSM8916 voice audio exists but is out of tree:

- **QDSP6 audio stack**: `CONFIG_QCOM_APR`, `CONFIG_SND_SOC_QDSP6` (q6core,
  q6afe, q6adm, q6asm, q6routing), the MSM8916 sound card
  (`CONFIG_SND_SOC_APQ8016_SBC`, `qcom,msm8916-qdsp6-sndcard`), the PM8916
  codec (`CONFIG_SND_SOC_MSM8916_WCD_ANALOG`/`_DIGITAL`), and a DT sound node
  with the QDSP6 DAI links. All in mainline.
- **q6voice**: the voice-call patches (shared session management, q6mvm,
  q6cvp, q6cvs, voice call support, the Q6 Voice DAI driver, rx/tx port
  controls) live in the msm8916-mainline kernel tree
  (`ASoC: qdsp6: … q6voice` commits, rebased as recently as April 2026).
  Config: `CONFIG_SND_SOC_QDSP6_Q6VOICE`, `CONFIG_SND_SOC_QDSP6_Q6VOICE_DAI`.
  They also implement the "hostless FE" trick. postmarketOS funded Linaro in
  2026 to rework them for upstream (codec-to-codec links, no userspace
  daemon).
- **Internal BT SCO port: missing in mainline.** Neither torvalds/linux nor
  msm8916-mainline defines an internal BT SCO AFE port in
  `qcom,q6dsp-lpass-ports.h` (downstream had `INT_BT_SCO_RX`/`_TX`, AFE
  ports 0x3000/0x3001). Headset audio as we did it downstream needs that
  port added to q6afe, plus whatever WCNSS needs to route SCO to the DSP.
  As far as we can tell `btqcomsmd` only carries HCI commands/events and
  ACL, so SCO over HCI to BlueZ is not an alternative. Unverified on our
  side.

### 4.2 Userspace

- `alsa-lib` + `alsa-utils` (or tinyalsa) for routing and recording; see
  §4.4 for where the tools come from.
- **q6voiced** (postmarketOS, `0_git20210408`, D-Bus + tinyalsa): watches
  ModemManager/oFono over D-Bus and opens/closes the hostless voice PCM when
  a call becomes active/ends. Configured with the card and device numbers.
  On OpenWrt the same can be a small script on ModemManager call events.
- Mixer routes per output (an ALSA UCM profile or a shell script like §3).

### 4.3 Where the audio goes (the stick has no mic or speaker)

| Output | How | Work needed on OpenWrt |
|---|---|---|
| Bluetooth headset (HFP) | DSP ↔ internal BT SCO ↔ Pronto | kernel: internal BT SCO AFE port; BlueZ HFP audio gateway role (oFono or a small HFP AG); SCO setup without the enhanced command. Most work |
| USB sound card plugged into the stick (USB host mode). **Not verified**; others have used a USB mouse on these sticks in host mode | in-call record taps → Linux → card's output; card's mic → in-call music → uplink | `kmod-usb-audio` (snd-usb-audio), the USB port in host mode with external VBUS (OTG adapter with power, or a powered hub, since the stick is normally powered through that port), a loop moving PCM between the two cards (`alsaloop`, or `arecord \| aplay` both ways) with 8↔48 kHz resampling. Needs the record/inject routes in mainline q6routing (unverified). The DSP's echo canceller doesn't see this path: use a headset, not open speakers. USB networking/adb over the gadget is lost while in host mode |
| PC over USB (the stick appears as a USB sound card) | in-call record taps → Linux → USB audio gadget (UAC2), and the gadget's mic → in-call music | `kmod-usb-gadget` + f_uac2 in the existing USB gadget config; a loop moving 8/16 kHz PCM both ways. Needs the record/inject routes in mainline q6routing (unverified) |
| SIP phone / softphone on the LAN | same taps → baresip/Asterisk | a GSM↔SIP bridge (see `selvakn/gsm-sip-bridge`, `asterisk-chan-quectel` for the idea). Latency ~40–80 ms extra |
| Files (voicemail, announcements, recording) | same taps → WAV | simplest; proven on downstream |
| PM8916 codec pins | PRI_MI2S / TERT_MI2S | only useful if the board brings out earpiece/mic lines (most sticks don't) |

### 4.4 Where the tiny\* tools come from

`tinymix`, `tinyplay`, `tinycap`, `tinyhostless` and `tinypcminfo` are the
tinyalsa utilities (BSD licence). We used the copies built from the
LineageOS 20 tree (AOSP `external/tinyalsa`); the same tools are in
upstream https://github.com/tinyalsa/tinyalsa (`utils/`, builds with make or
meson). OpenWrt has no tinyalsa package, but `alsa-utils` covers everything
except the hostless start:

| tinyalsa | alsa-utils |
|---|---|
| `tinymix '<control>' 1` | `amixer -c 0 cset name='<control>' 1` |
| `tinymix` (list) | `amixer -c 0 controls`, `amixer -c 0 contents` |
| `tinycap rec.wav -D 0 -d 0 -c 2 -r 8000 -b 16` | `arecord -D hw:0,0 -f S16_LE -c 2 -r 8000 rec.wav` |
| `tinyplay prompt-8k.wav -D 0 -d 1` | `aplay -D hw:0,1 prompt-8k.wav` |
| `tinyhostless -D 0 -P 2 -C 2 …` | none; package upstream tinyalsa, or use the small alsa-lib program in the appendix |

`tinyhostless` just opens the playback and capture side of the FE, starts
both, and sleeps until it is killed; it never reads or writes data. On
mainline with q6voice, `q6voiced` does the same job.

## 5. A LuCI dialer: proposed shape

There is no LuCI dialer today. Existing pieces:

- ModemManager LuCI apps (`luci-proto-modemmanager`, `luci-app-mmconfig`,
  luci-graphic-modemmanager, luci-app-5gmodem with SMS/USSD): status, bands,
  SMS, USSD, no calls.
- `luci-app-bluetooth` (sbwml, third party; not in upstream LuCI): adapter
  power/alias, scan, pair (via `expect` + `bluetoothctl`), connect,
  bluez-alsa codec/volume for A2DP. Usable for pairing a headset; it does
  not cover HFP call audio.

Suggested design:

1. **Backend**: an rpcd ucode plugin (`/usr/share/rpcd/ucode/dialer.uc`)
   wrapping `mmcli` (or ModemManager's D-Bus API): `list`, `dial`,
   `accept`, `hangup`, `hold_accept`, `dtmf`, `status`. Return JSON from
   `mmcli -J`.
2. **Events**: a small daemon that listens to ModemManager's `CallAdded`
   / call `StateChanged` signals and republishes them on ubus
   (`ubus send dialer …`), so the page updates without polling and hooks can
   run: start/stop audio routing, auto-answer, ring a LED, call log.
3. **Audio hook**: `/etc/dialer/audio.d/*` scripts run with
   `prepare|start|end` and the chosen output (bt, usb, usbcard, sip, rec).
   `prepare` (incoming call ringing) opens the SCO link early; `start`
   (dialing, or the user pressed accept) sets the mixer routes and holds the
   hostless voice PCM open, what q6voiced does; `end` undoes both. See §3.1
   for why the hooks should not wait for `active`.
4. **Frontend**: a LuCI JS view: keypad, call list with state, accept /
   hang up / hold buttons, DTMF, output selector, recent calls, and
   (optionally) a Bluetooth panel reusing `luci-app-bluetooth`.

Call states worth showing: `ringing-in`, `waiting`, `dialing`,
`ringing-out`, `active`, `held`, `terminated` (with the reason).

## 6. VoLTE

What we found on the HMUF02-V05 (`MPSS.DPM.1.0.c7`):

- The modem contains Qualcomm's modem-side IMS stack (SIP, VoLTE call
  manager, RTP in the modem) but ships **no IMS configuration**: 66 of the
  69 IMS items listed in `/nv/item_files/conf/ims.conf` are missing, and
  every IMS Settings query fails. The modem has no carrier MBN except its
  built-in `ROW_Generic_3GPP`, which has no IMS part.
- Carrier MBNs from the ASUS ZenFone 2 Laser are for **`MPSS.DPM.2.0.2`**
  (WTR4905). Loading one with QMI PDC works; **activating it crash-loops a
  DPM.1.0 modem** (`mcfg_utils.c:186` about 1 s after every boot). Recovery:
  stop the modem, restore `modemst1`/`modemst2` from a backup. Take one first.
- Copying only the MBN's IMS EFS items made the IMS settings readable, but
  the IMS stack still never asked for the `ims` PDN. Our conclusion: there
  is no VoLTE configuration matching the DPM.1.0 firmware.
- In "hybrid" mode (`ims_hybrid_enable`) the modem expects an AP-side QMI
  **IMS data service** (service 770) to bring up the `ims` APN; postmarketOS
  OpenIMSD's `81voltd` implements it for mainline (QRTR + ModemManager),
  and `qcom-imsd` enables IMS through the IMS Settings service on newer
  firmware.
- Operators: the `ims` APN is usually IPv6. Some operators only allow
  VoLTE for approved device models (lists keyed on the IMEI TAC); that would
  show as a rejected SIP REGISTER once IMS registers.

Firmware families seen on MSM8916 sticks:

| Board | Modem build | RF | VoLTE MBNs on the modem partition |
|---|---|---|---|
| HMUF02-V05 (HiMI) | MPSS.DPM.1.0.c7-00193 | WTR1605 | none |
| UZ801 | MPSS.DPM.1.0.r6-00002 | WTR1605 | none |
| UFI003 MB V02 | MPSS.DPM.2.0.2.c1-00178 | WTR4905 | 58 MBNs (Gen_3GPP, EE, T-Mobile/AT&T VoLTE, Verizon, Jio, …) |
| ASUS ZenFone 2 Laser | MPSS.DPM.2.0.2.c1-00097 | WTR4905 | 65 MBNs |

So DPM.2.0.2 boards such as the UFI003 are the realistic VoLTE candidates:
their modem and carrier configs match. Modem firmware cannot be swapped
between the two families: RF transceiver and modem memory region differ
(0x86800000 vs 0xC0000000).

## 7. Power and heat

- 3G calls draw current in bursts. On a 0.55 A supply the stick reset
  ~50 s into a call (PMIC: power-on reason SMPL, power-off reason UVLO, no
  kernel panic). On a stronger supply, calls of 4, 4 and 10 minutes ran
  without a reset, but the input still sagged:

  | | Idle | Call (10 min) |
  |---|---|---|
  | `usb_in` | ~4.9–5.0 V | avg 4.82 V, min 4.45 V; ~250 mV dips every few 100 ms (TX bursts) |
  | `vph_pwr` | 3.45 V | min 3.43 V |

  One dip raised the PMIC's USB-in-valid interrupt and USB re-enumerated
  (the USB gadget link dropped). Use a short, thick cable and a supply with
  margin; capping the CPU at 998 MHz adds some more. Run long tests on the
  stick itself (`setsid`), so they survive a USB drop.
- Heat: in a call the modem sensor (tsens0) and the CPU sensors settled at
  70–75 °C with the CPU quiet, and reached 90–95 °C when the call coincided
  with CPU load (for example right after boot), on an uncooled stick.

## 8. Open questions (help welcome)

1. Does mainline q6routing (with the q6voice patches) expose the in-call
   record (`VOC_REC_DL/UL`) and in-call music routes? They are what the USB,
   SIP and file outputs depend on.
2. Internal BT SCO on mainline: q6afe port definitions plus the WCNSS side.
3. Does q6voice on the DPM.1.0 HiMI modem behave like on phones (the
   patches were tested on Samsung/Wileyfox/Motorola MSM8916 phones)?
4. VoLTE on a DPM.2.0.2 board (UFI003): PDC activation of a matching MBN,
   `81voltd`, IMS registration. We plan to try this next.

## 9. References

- msm8916-mainline kernel, q6voice commits:
  https://github.com/msm8916-mainline/linux
- postmarketOS: voice call audio on mainline MSM8916 (pmaports !1233),
  q6voiced package, and the 2026 q6voice(d) upstreaming project
  (postmarketos.org blog, 2026-05-08)
- postmarketOS OpenIMSD (81voltd, qcom-imsd, qcom-baseband-profile-manager):
  https://www.openimsd.de/pages/architecture.html
- tinyalsa: https://github.com/tinyalsa/tinyalsa
- `luci-app-bluetooth`: https://github.com/sbwml/luci-app-bluetooth
- ModemManager LuCI apps: luci-proto-modemmanager,
  koshev-msk/modemfeed (luci-app-mmconfig), fildunsky/luci-app-5gmodem

## Appendix: `hostless`, a minimal alsa-lib replacement for tinyhostless

Written for this note; compiled with `-Wall -Wextra` against alsa-lib
1.2 without warnings, **not yet run on an MSM8916**. Usage on the
downstream card: `hostless -D hw:0,2 -r 8000 -c 1 -p 512 -n 2 &`, and
`kill` it when the call ends. OpenWrt package dependencies: `alsa-lib`.

```c
/* hostless: keep DSP "hostless" PCM devices open and running without moving
 * any data. On Qualcomm QDSP6 sound cards this starts the session behind the
 * front end (e.g. the CS-Voice FE starts the DSP voice call session); the
 * mixer routes decide where the audio goes. Same idea as AOSP tinyhostless,
 * written against alsa-lib.
 *
 *   hostless -D hw:0,2 [-P] [-C] [-r 8000] [-c 1] [-p 512] [-n 2]
 *
 * -P / -C open the playback / capture direction (default: both).
 * Runs until SIGINT/SIGTERM.
 *
 * Build: cc -O2 -o hostless hostless.c -lasound
 */
#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stop;

static void on_signal(int sig) { (void)sig; stop = 1; }

static snd_pcm_t *open_hostless(const char *dev, snd_pcm_stream_t dir,
                                unsigned rate, unsigned channels,
                                snd_pcm_uframes_t period, unsigned periods)
{
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hw;
    snd_pcm_sw_params_t *sw;
    snd_pcm_uframes_t boundary;
    const char *name = dir == SND_PCM_STREAM_PLAYBACK ? "playback" : "capture";
    int err;

    if ((err = snd_pcm_open(&pcm, dev, dir, 0)) < 0) {
        fprintf(stderr, "%s %s: %s\n", dev, name, snd_strerror(err));
        return NULL;
    }
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    if ((err = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0 ||
        (err = snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE)) < 0 ||
        (err = snd_pcm_hw_params_set_channels(pcm, hw, channels)) < 0 ||
        (err = snd_pcm_hw_params_set_rate(pcm, hw, rate, 0)) < 0 ||
        (err = snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, NULL)) < 0 ||
        (err = snd_pcm_hw_params_set_periods_near(pcm, hw, &periods, NULL)) < 0 ||
        (err = snd_pcm_hw_params(pcm, hw)) < 0) {
        fprintf(stderr, "%s %s: hw params: %s\n", dev, name, snd_strerror(err));
        snd_pcm_close(pcm);
        return NULL;
    }
    /* No data will flow: never stop the stream on an xrun. */
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(pcm, sw);
    snd_pcm_sw_params_get_boundary(sw, &boundary);
    snd_pcm_sw_params_set_stop_threshold(pcm, sw, boundary);
    snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
    snd_pcm_sw_params(pcm, sw);

    if ((err = snd_pcm_prepare(pcm)) < 0 || (err = snd_pcm_start(pcm)) < 0) {
        fprintf(stderr, "%s %s: start: %s\n", dev, name, snd_strerror(err));
        snd_pcm_close(pcm);
        return NULL;
    }
    return pcm;
}

int main(int argc, char **argv)
{
    const char *dev = "hw:0,2";
    unsigned rate = 8000, channels = 1, periods = 2;
    snd_pcm_uframes_t period = 512;
    int play = 0, cap = 0, opt;
    snd_pcm_t *p = NULL, *c = NULL;

    while ((opt = getopt(argc, argv, "D:PCr:c:p:n:")) != -1) {
        switch (opt) {
        case 'D': dev = optarg; break;
        case 'P': play = 1; break;
        case 'C': cap = 1; break;
        case 'r': rate = atoi(optarg); break;
        case 'c': channels = atoi(optarg); break;
        case 'p': period = atoi(optarg); break;
        case 'n': periods = atoi(optarg); break;
        default:
            fprintf(stderr, "usage: %s -D hw:CARD,DEV [-P] [-C] [-r rate] [-c ch] [-p period] [-n periods]\n", argv[0]);
            return 2;
        }
    }
    if (!play && !cap)
        play = cap = 1;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (play && !(p = open_hostless(dev, SND_PCM_STREAM_PLAYBACK, rate, channels, period, periods)))
        return 1;
    if (cap && !(c = open_hostless(dev, SND_PCM_STREAM_CAPTURE, rate, channels, period, periods))) {
        if (p)
            snd_pcm_close(p);
        return 1;
    }
    while (!stop)
        pause();

    if (c)
        snd_pcm_drop(c), snd_pcm_close(c);
    if (p)
        snd_pcm_drop(p), snd_pcm_close(p);
    return 0;
}
```

