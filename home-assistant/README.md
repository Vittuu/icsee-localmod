# Home Assistant setup

Tested with HA 2026.10.4, ICSee PTZ 5.1.12 and HA-managed go2rtc 1.9.14. This is a local example, not an upstream integration release.

1. Start with [ICSee PTZ](https://github.com/dbuezas/icsee-ptz), native device credentials and enabled RTSP. Do not expose the camera to the internet. Check the native video entities first; firmware changes are not required just to try stock RTSP/PTZ.
2. If you already have the LocalMod panel/API working, enable HA packages and merge `package.example.yaml`. Put your own device credentials and full local endpoint URLs into **your HA secrets.yaml**, following `secrets.example.yaml`. Do not replace your existing configuration. Use **Check configuration** before a reload/restart. These portable examples use generic names and response validation; adapt and validate them on your installation.
3. For an H.264 viewing entity on **ICSee PTZ 5.1.12 only**, back up the integration and inspect `icsee-ptz-5.1.12-low-latency.patch`. It changes camera.py only and leaves the original main/sub entities intact. From the HA configuration directory, `git apply --check /path/to/patch` should succeed before applying it. If it does not, stop: your source differs. The diff was generated against the tested installation, which already had local RTSP fixes; compatibility with a clean upstream install is not guaranteed.
4. Restart HA to load Python code. Find the new **Ao vivo baixa latência** camera entity. Use its actual ID in `dashboard.example.yaml`. It uses `ffmpeg:<native RTSP URL>#video=h264#audio=opus` through built-in go2rtc. No URL/credentials should be posted in public.
5. Native main/sub RTSP and SD remain H.265. Transcoding runs on the HA server while the stream is consumed. Expect additional CPU use and some re-encoding loss. Prefer the original entity if you do not need H.264 compatibility. A successful WebRTC session is not a measured end-to-end latency benchmark.

Keep a native **PTZ stop** control available from the ICSee device page. Movement through this example uses the on-camera API's 250 ms pulse; direct native integration buttons may start continuous movement until stopped.

The bridge and the native integration can use different LED state caches. Commands through LocalMod use native OPWhiteLightControl; stale HA switch state does not prove the physical LED state. Refresh/query the camera when necessary.

**Updating ICSee PTZ may overwrite this local patch.** Review against the new version rather than applying an old diff blindly. To roll back, restore camera.py from its backup, select the original main-stream entity in your dashboard and restart HA. Remove only the package/scripts you added; leave other configuration intact.

Some browsers cannot negotiate the original camera HEVC via WebRTC and may fall back to buffered HLS. H.264/Opus improves compatibility, but network topology, relays, Wi-Fi, decoding and firmware buffering still affect delay. No numeric improvement was measured in this project.
