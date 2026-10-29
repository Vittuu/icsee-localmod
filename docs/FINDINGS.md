# Findings and development path

This is an owner-authorized, AI-assisted engineering report, checked on one physical unit. It documents interoperability experiments with existing native protocols, not a claim to have invented DVRIP or discovered a new exploit. Upstream protocol/integration work is credited in the repository README. No firmware images or private device data are distributed.

## Tested platform

- SoC reported by stock system: XM530V200.
- Product/hardware family: IPG-X2-WQ-BL, 8 MiB flash.
- Stock firmware: V5.01.R02.A64807D7.10010.346732.0000010, build dated 2025-04-23.
- Local interfaces observed: RTSP 554, ONVIF 8899, DVRIP 34567; extension HTTP 18080 and SSH 2222.

This identifies one unit, not every camera sold under the same enclosure or brand. Sensor, Wi-Fi module, flash layout and boot hooks may differ.

## Why retain the original firmware?

OpenIPC support for a SoC does not establish complete support for that camera's image sensor, wireless chipset, audio path and motors. The chosen approach preserved the working native stack and added narrowly scoped local services. This does not incorporate OpenIPC improvements or eliminate all vendor code.

## Useful protocol details

- Native write-config is DVRIP 1040, read-config 1042. A successful transport response is not proof of an actual setting change: read back configuration and inspect `Ret`.
- White-light hardware responded to message 1450 / `OPWhiteLightControl` with `Action` values `KeepOpen`, `Close`, `Auto`, `Intelligent`. Merely updating a configuration flag was not used as the sole proof of physical light actuation.
- PTZ uses message 1400 / `OPPTZControl`; the tested firmware starts movement with `Preset=65535` and stops with `Preset=-1`. The extension sends stop after 250 ms. This is software best effort, not a safety-certified motor controller.
- Fragmented DVRIP JSON replies must be reassembled. The bridge recognizes the tested protocol versions and authenticates using native device credentials.
- SD file listings use 1440 / `OPFileQuery`. A `.h264` suffix did **not** imply H.264 video: a retrieved recording decoded as HEVC/H.265.
- The native recorder uses WFS. Generic FAT32 assumptions broke extension persistence during recording experiments; persistent boot was moved to internal storage.

## Boot / SSH pitfalls

The board's root was immutable cramfs with factory UID 1108. Dropbear ownership checks needed a narrow exception for **only `/`, only that factory UID, only cramfs, only a read-only root**; key files and mutable parent directories remain checked. This exception is hardware-specific, not a reason to disable ownership checks globally.

The runtime script waits for the camera's `eth3` IPv4 address. The tested web binary intentionally permits loopback or 192.168.0.0/16 bind addresses. Other LAN prefixes and interface names require source review and fresh tests; do not assume DHCP support is universal.

## Persistent installation result, not an installation recipe

On the tested unit, rebuilt user/custom images fit their exact original partition boundaries, preserved 140 user and 57 custom original entries, were read back after writing, and mounted successfully. Two independent restarts verified internal panel/SSH boot and native SD recording. The custom partition had no remaining image space; subsequent additions require rebuilding and rechecking fit.

No bootloader, kernel or root filesystem was flashed. No public images or generic installer are supplied because this successful device-specific result is insufficient evidence of compatibility with another unit.

## Home Assistant video

Native maximum video remained H.265, 2304 Ã— 1296, 12 fps. The UI exposed only H.265 for the secondary stream on this firmware. A separate viewing entity therefore uses go2rtc's supported `ffmpeg:â€¦#video=h264#audio=opus` source. go2rtc's H.264 template tunes for low latency. Re-encoding takes server CPU and can lose detail; recording on SD is unaffected.

The HA-managed go2rtc rejected source strings containing literal spaces. Percent-encoding raw arguments did not make them valid FFmpeg arguments in this test. The final configuration uses the supported standard template, not a bypass of that restriction. H.264/Opus transmission and moving 2304 Ã— 1296 video via WebRTC were confirmed. Exact end-to-end latency reduction remains unmeasured.
