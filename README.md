# iCSee LocalMod — stock firmware, local controls, Home Assistant

Experimental, source-only project for **one tested Xiongmai XM530V200 / IPG-X2-WQ-BL camera**. Keeps the stock firmware, sensor/Wi-Fi/PTZ/audio/LED drivers and native SD recorder, while adding a small local web API and key-only SSH. **This is not OpenIPC and is not a universal firmware image.**

Analysis used [REA + Ghidra](docs/REA-GHIDRA-WORKFLOW.md) with Codex assistance. Development and documentation used AI assistance. Hardware observations below were checked on the owner's physical camera; portability has not been established.

## What worked on the tested unit

| Feature | Observed result |
|---|---|
| Main video | HEVC/H.265, 2304 × 1296, 12 fps; audio G.711 at 8 kHz |
| Secondary video | HEVC/H.265, 800 × 448, 12 fps |
| Wi-Fi | Stock driver retained; reconnects after restart |
| PTZ | Both axes; local API starts a 250 ms movement and sends stop |
| White LEDs | Native `OPWhiteLightControl`; on/off physically confirmed |
| Microphone / speaker | PCM decoded from incoming audio; two test beeps heard by owner |
| SD recording | Native WFS recording resumed after two independent power cycles; earlier recording stayed byte-identical |
| Local panel | Device-password authentication, status, SD list by date, recording schedule, PTZ/presets and lighting |
| SSH | Dropbear with authorized public keys; password authentication disabled |
| Home Assistant | Local ICSee PTZ integration plus bounded REST controls and a separate H.264/Opus WebRTC viewing stream |

**The H.264 viewing stream is transcoded on the HA server, not on the camera.** It preserves 2304 × 1296 resolution, but re-encoding is not bit-identical to the original. SD recording stays H.265. Numeric before/after latency was not measured; no sub-second guarantee is made.

The final hardware installation placed additions in the unit's `user` and `custom` partitions. Bootloader, kernel and root filesystem were not written. Original files in the two rebuilt filesystems were verified as preserved. Panel/SSH boot independently of the SD card, leaving the card to the native recorder.

## Contents

- `src/`, `web/`: local C HTTP/DVRIP bridge and panel (Portuguese UI).
- `runtime/`: **hardware-specific** persistent boot and route-removal scripts.
- `ssh/`: Dropbear options and patches for the tested read-only cramfs root ownership.
- `home-assistant/`: generic examples and a patch for ICSee PTZ 5.1.12.
- `docs/`: findings, API, build notes, threat model and hardware validation.
- `tests/`: ARM/QEMU fake-backend tests and root-ownership checks.
- `vendor/`, `licenses/`: cJSON source and dependency notices.

No flash dumps, proprietary firmware images, device keys, Wi-Fi settings, native account database, personal screenshots or raw device logs are distributed. There is **no public flash installer**. The original development images are device-specific and must not be used on another camera.

## Start with the stock local protocols

Check whether your own camera already offers RTSP/ONVIF/DVRIP. Home Assistant video/PTZ may work without changing firmware at all: start with [dbuezas/icsee-ptz](https://github.com/dbuezas/icsee-ptz). The custom on-camera extension is only relevant if you need its panel/API/boot behavior.

Read [findings](docs/FINDINGS.md), [security and limitations](docs/SECURITY.md), [build notes](docs/BUILD.md) and [Home Assistant setup](home-assistant/README.md) before experimenting. The source-only publication is not a claim that another unit can be safely flashed.

## Quick build and isolated test

See `docs/BUILD.md` for prerequisites. With a compatible ARM static toolchain and the matching built Dropbear crypto library:

```sh
CC=/path/to/arm-linux-musleabi-gcc DROPBEAR_SOURCE=/path/to/dropbear-2026.94 sh build.sh
python3 tests/test_web.py  # requires qemu-arm; fake backend on loopback only
```

Tests do not connect to a real camera or write flash. Do not run the runtime scripts on your workstation: they manipulate routing and expect the camera filesystem layout.

## Security boundaries

The bridge authenticates against the camera's **existing device account**; it is not an authentication bypass. HTTP Basic over plain HTTP is not transport encryption. Use a trusted isolated LAN or an SSH tunnel, never public port forwarding. See `docs/SECURITY.md` for the additional limitations of stock services and the route guard. Removing default routes is **not** an egress firewall or proof that every vendor connection is impossible.

## Credits and licensing

Original LocalMod additions: MIT (see `LICENSE`). Third-party files keep their own notices. Credit to [ICSee PTZ](https://github.com/dbuezas/icsee-ptz), [go2rtc](https://github.com/AlexxIT/go2rtc), [Home Assistant](https://github.com/home-assistant/core), [Dropbear](https://github.com/mkj/dropbear), [cJSON](https://github.com/DaveGamble/cJSON), LibTomCrypt, LibTomMath and musl. The project is not affiliated with those projects, the camera vendor or iCSee. Stock camera firmware/drivers remain proprietary and are not relicensed here.

Validated on 2026-10-29. Feedback with **redacted** model/SoC/firmware/partition metadata is welcome. Do not attach complete firmware dumps or credentials to issues.
