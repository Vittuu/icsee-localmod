# LocalMod HTTP API

This page documents the HTTP API added by LocalMod on TCP port 18080. It is
separate from the camera's stock DVRIP service on TCP port 34567. For the
tested native command IDs and fields, see
[Useful protocol details](FINDINGS.md#useful-protocol-details).
The HTTP routes below are translated into native requests by the
[LocalMod bridge](../src/localmod-web.c); they are not stock DVRIP endpoints.

Base URL: `http://CAMERA_LAN_IP:18080`. POST JSON, native HTTP Basic credentials, `Content-Type: application/json`, `X-Localmod: 1`. Controls require the `--enable-controls` startup flag. The executable defaults to loopback unless `--bind` is supplied.

| Path | Body | Purpose |
|---|---|---|
| `/api/read` | `{"name":"StorageInfo"}` | Allowlisted native configuration/status reads |
| `/api/health` | `{}` | Extension diagnostics (review before sharing) |
| `/api/ptz` | `{"direction":"DirectionLeft"}` | Up/Down/Left/Right, 250 ms then stop |
| `/api/preset` | `{"action":"SetPreset","number":1}` | `SetPreset` / `GotoPreset`, slots 1–16 |
| `/api/light` | `{"action":"KeepOpen"}` | `KeepOpen`, `Close`, `Auto`, `Intelligent` |
| `/api/files` | `{"begin":"2026-01-01 00:00:00","end":"2026-01-01 23:59:59"}` | Native SD listing; pagination handled by panel |
| `/api/patch` | `{"name":"Record","field":"RecordMode","value":"ConfigRecord"}` | Narrow field allowlist, readback verification |
| `/api/schedule` | `{"start":"00:00","end":"24:00","motion":false,"days":[true,true,true,true,true,true,true]}` | Replace seven-day recording schedule |

HTTP 200 alone is not success. Check the JSON `erro` field and native `Ret` (normal command success is 100; some list/status operations use other native codes). Do not loop rapidly on refused authentication or assume every advertised native feature is implemented by your hardware.
