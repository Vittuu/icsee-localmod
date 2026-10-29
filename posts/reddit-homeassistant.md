I wanted local Home Assistant control of an iCSee camera without depending on the app, while keeping its working Wi-Fi, PTZ motors, audio, LEDs and SD recorder. I am sharing the source and what was actually validated on my own unit.

**Hardware:** XM530V200 / IPG-X2-WQ-BL, stock V5.01.R02.A64807D7.10010.346732.0000010 firmware. This is one tested unit, not a compatibility claim for every camera with the same enclosure.

**The result:** a small C web panel/native DVRIP bridge, key-only SSH and Home Assistant viewing/control. Stock drivers and firmware remain in place. This is not OpenIPC.

What worked on the hardware:

- Original 2304 × 1296 H.265 video at 12 fps; G.711 microphone audio.
- Horizontal/vertical PTZ through bounded 250 ms API movements followed by stop.
- White LEDs using native OPWhiteLightControl; physically confirmed.
- Speaker beeps and incoming microphone capture tested separately.
- Native WFS SD recording resumed after two independent power cycles; an earlier video remained byte-identical and new recording data grew.
- Panel/SSH now boot from internal storage, leaving the SD card for the native recorder.
- HA uses the ICSee PTZ integration plus local REST controls. An optional additional H.264/Opus WebRTC viewing entity transcodes on the HA server, leaving original H.265 SD recording unchanged. Resolution is preserved; re-encoding is not lossless. Exact before/after latency has not been measured.

Useful findings: config-write/read IDs were 1040/1042; white-light hardware uses message 1450 / OPWhiteLightControl; native SD recordings used .h264 filenames even when the contents were HEVC. Recording interfered with earlier SD bootstrap experiments, which is why the extension was moved to internal storage.

**Limitations:** not a universal flashable release. No proprietary firmware, dumps, keys, credentials or private camera images are published. The repository has source, build notes, allowlisted API, HA examples, tests and a narrowly scoped patch for the tested ICSee PTZ version. It is not necessary to modify firmware just to try the stock camera's RTSP/PTZ integration. SD playback/download/delete buttons, MQTT and new AI detection are not implemented.

Known vendor cloud settings were disabled, but the route-removal guard is **not a firewall**. Stock vendor code remains; use network isolation/firewall rules for enforceable egress control. The panel uses HTTP Basic, so it belongs behind a trusted isolated LAN/SSH tunnel, never a public port forward.

Code and write-up (MIT for original additions; dependency notices retained):
https://github.com/Vittuu/icsee-localmod

Credit to dbuezas/icsee-ptz, go2rtc, Home Assistant, Dropbear and the other upstream libraries. Development and writing used AI assistance; the hardware observations above were checked on my physical camera. Feedback with redacted model/SoC/firmware metadata is welcome, especially differences between XM variants. Please do not attach firmware dumps or secrets.
