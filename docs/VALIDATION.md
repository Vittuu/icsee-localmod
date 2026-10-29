# Hardware validation record (redacted)

Date: 2026-10-29. One XM530V200 / IPG-X2-WQ-BL unit. No compatibility matrix beyond this unit.

- Horizontal and vertical motor controls produced visible movement; bounded API tested.
- White LEDs physically lit, confirmed by owner; previous intelligent mode restored.
- Two speaker beeps physically heard by owner; incoming microphone audio decoded separately.
- Main and sub RTSP streams decoded with their advertised dimensions and HEVC codec.
- Flash readback matched the device-specific user/custom images; original filesystem entries preserved.
- Internal HTTP and key-authenticated SSH returned after two independent restarts, without depending on FAT32 bootstrap files on the SD.
- Native WFS recorder reported active storage; new recording data increased after boot.
- A previously retrieved recording had the identical SHA-256 after another restart.
- SD recording sample decoded as HEVC 2304 × 1296 at 12 fps despite a `.h264` filename.
- HA 2026.10.4 / ICSee PTZ 5.1.12: local image, directional controls and LEDs validated.
- Additional H.264/Opus stream confirmed via go2rtc WebRTC and visible moving video at original resolution. Exact before/after delay not measured.

This is a curated summary of private test evidence, not publication of raw logs or a new independent replication. On-camera behavior and simulated backend tests are separate evidence categories. See generated `docs/simulation-validation.json` for the public source build test run.
