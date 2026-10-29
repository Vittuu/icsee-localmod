# Security and limitations

Research prototype for an owner-controlled camera on an isolated network. No professional security audit was performed.

- The C bridge connects only to the native loopback backend. API endpoints are allowlisted; there is no arbitrary shell/file-write/MTD/format/reboot endpoint.
- API calls require native device credentials and the `X-Localmod: 1` header. The header is a simple-request guard, not a secret and not authorization by itself.
- Basic authentication over HTTP exposes credentials to an on-path LAN observer. Use isolation and/or an SSH local forward. Do not expose HTTP, RTSP, DVRIP or SSH to the public internet.
- SSH uses public keys; each installation needs its own host key and authorized keys. This repository contains no keys and no factory passwords.
- Redaction is best effort. Diagnostics can expose process/mount/network details. Never publish raw `/api/health`, native configuration, archives or logs without reviewing them.
- Stock firmware remains present, including any undiscovered vulnerabilities. Disabling its known P2P/cloud settings does not remove vendor code.
- The route guard periodically removes default routes. DHCP/RA can recreate routes between checks. Same-subnet proxies, static/specific routes, IPv6 and other network paths require independent review. Use router/VLAN firewall rules for enforceable egress blocking.
- PTZ stop can fail if the process crashes, networking fails or native firmware hangs. A 250 ms delay followed by stop is a best-effort mechanism, not a physical guarantee.
- Recording schedule changes replace the seven-day schedule with the selected daily period. Native storage management, overwrite behavior and media health remain vendor responsibilities.
- SD playback, download/delete buttons, MQTT discovery/control, external AI detection, timelapse and two-way conversation UI are not implemented. Speaker beeps and microphone capture were tested separately, not a complete intercom product.
- Source publication is not a flashable release. Any flash work needs your own verified full backup, exact partition map, image-size/metadata checks and independent recovery path. Power loss can require hardware recovery.

Report issues with minimal redacted reproductions. Do not attach account databases, Wi-Fi settings, firmware dumps, private camera images or access tokens.
