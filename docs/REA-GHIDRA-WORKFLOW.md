# REA + Ghidra workflow: stock XM530 camera, local interoperability

This is a report of an owner-authorized analysis, using [REA](https://github.com/morluto/rea) and Ghidra with Codex assistance, followed by separate runtime/hardware validation. It is not a claim that static analysis alone proved a working device modification, a novel exploit, or a complete DVRIP implementation.

## Recorded analysis environment

- WSL2, Ubuntu 24.04, isolated analysis distribution.
- REA 6.8.0.
- Ghidra 12.1.4, Java 21.0.13+9.
- Node 24.22.2.
- Target family: XM530V200 / IPG-X2-WQ-BL, stock firmware V5.01.R02.A64807D7.10010.346732.0000010.

The scoped Ghidra provider diagnostic passed. The broader REA diagnostic also checks other providers; Hopper was not required for this Ghidra-based investigation. These are the versions used here, not a recommendation to pin every future analysis to them.

## What REA/Ghidra helped establish

We investigated the stock native application (`Sofia`) from the owner's private backup, looking for command strings, references and their handlers. Retained local MCP evidence includes string-context/xref queries, selected decompilation results, a final snapshot and an Evidence export.

Selected anchors recorded for this exact binary:

| Anchor | Recorded location / handler | Interpretation to verify at runtime |
|---|---|---|
| `OPWhiteLightControl` | String `0x430027`; dispatcher `FUN_0018ca20` | Dedicated native white-light command, distinct from merely changing a configuration flag |
| `OPPTZControl` | Handler `FUN_001a376c` | Native motor command path and PTZ-related parameters/permission check |
| `OPSetDoubleled`, `WhiteLightState`, `DayNightState` | Additional lighting-related paths were found | Not every advertised setting is a proven physical feature; do not merge distinct paths into one assumed LED control |
| `Talk`, `SupportTwoWayVoiceTalk`, `SupportCameraWhiteLight` | Native capability strings | Useful capability hints, not proof of a complete intercom implementation |

Addresses and auto-generated function names are specific to the input binary, image base and analysis. They are not portable offsets or a patch recipe. Decompilation excerpts, firmware binaries and private Evidence bundles are intentionally not published here; this report contains our factual summary and links to original bridge source/tests.

## Separate static findings from observed behavior

1. Inspect the relevant string/context and follow xrefs into the dispatcher/handler.
2. Form a narrowly scoped protocol hypothesis; reuse and credit existing DVRIP/ICSee integration work rather than claiming the whole protocol as new research.
3. Authenticate locally with the owner's existing device account. Verify native responses and read back configuration where applicable.
4. Check physical outcomes: a response code or advertised capability does not establish that a motor moved or a light lit.
5. Keep a working stock-driver path. Camera Wi-Fi, sensor, audio, PTZ and LEDs depended on the existing firmware/drivers; replacing the OS was not necessary for the selected controls.
6. Treat boot persistence and SD recording as separate experiments. QEMU/fake-backend protocol tests do not establish that a filesystem image is safe to flash or that recording survives a power cycle.

The later local C bridge uses tested native commands: configuration read/write 1042/1040, PTZ 1400, white-light 1450 and SD listing 1440. Its [source](../src/localmod-web.c) and [fake-backend test](../tests/test_web.py) are published. Those tests exercise fragmented replies, authentication rejection, allowed operations and PTZ start/stop sequencing without touching a real camera.

## What ultimately worked, and the limits

The owner checked video, both PTZ axes, white LEDs and speaker beeps. Microphone audio was decoded separately. The final device-specific extension boots internally and native WFS recording resumed after independent restarts, with an earlier recording preserved. Home Assistant gained local viewing and controls; an additional server-side H.264/Opus WebRTC stream leaves original H.265 SD recording unchanged.

This is **stock firmware with additions, not OpenIPC**. Only one hardware unit was validated. There is no universal image/flash installer, complete intercom UI, MQTT implementation or new AI detector. The route-removal guard is not a firewall, and exact before/after video latency was not measured. See [findings](FINDINGS.md), [validation](VALIDATION.md), [security](SECURITY.md) and [build notes](BUILD.md) for details.

## Attribution and sharing

REA/Ghidra supported analysis; Codex assisted with development and writing. Existing upstream work in ICSee PTZ, go2rtc, Dropbear and the other dependencies is credited in the [README](../README.md). No independent maintainer audit or cross-model compatibility is claimed. Feedback about a matching platform should omit device passwords, account/Wi-Fi data, MAC/serial numbers, camera footage and private firmware dumps.
