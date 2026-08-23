<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Scan Data Export

## Future Idea

Store WiFi and Bluetooth scan data on the LilyGO T-Display C5 and transfer it to
an iPhone for later review, sharing, or archiving.

## Candidate Transport

The preferred first approach is BLE export because the project already uses
NimBLE-Arduino for Bluetooth scanning. A custom BLE service could expose stored
scan records to an iPhone app, Shortcut-compatible helper, or generic BLE tool.

Other possible transports:

- WiFi SoftAP plus a small web page or HTTP endpoint for bulk downloads.
- USB serial export for desktop use.
- BLE live streaming through notifications as scan records are discovered.

BLE chunked export is probably the best first version because it fits the
existing firmware without turning the scanner into a web server.

## Data To Store

WiFi scan records should include:

- Timestamp or scan sequence number.
- SSID.
- BSSID.
- RSSI.
- Channel.
- Band.
- Security mode.
- Hidden flag.
- Same-channel BSSID count.

Bluetooth scan records should include:

- Timestamp or scan sequence number.
- Advertised name or fallback label.
- BLE address.
- RSSI.
- Connectable/beacon status.
- Passive or active scan mode.
- Last-seen age or last-seen timestamp.

## Storage Model

Use a bounded ring buffer so logging cannot consume memory forever.

Possible stages:

- Start with RAM-only storage for the latest scan records.
- Add flash-backed persistence later if battery-powered field use needs logs to
  survive reboot.
- Consider separate WiFi and BLE buffers so one noisy scan mode does not evict
  all records from the other mode.

## Export Format

Use a compact, phone-friendly text format:

- Newline-delimited JSON for structured records and streaming.
- CSV for easy spreadsheet import.
- A compact binary format only if BLE throughput becomes a real limitation.

Newline-delimited JSON is the most flexible first choice because each record can
be transferred and parsed independently.

## BLE Service Sketch

Use a custom BLE service with characteristics such as:

| Characteristic | Direction | Purpose |
| --- | --- | --- |
| Control | iPhone writes | Start export, choose WiFi/BLE/all records, clear log |
| Data | iPhone reads or subscribes | Chunked scan data payloads |
| Status | iPhone reads | Record count, export progress, error state |

BLE payloads are small, so export should be chunked. Include sequence numbers or
chunk indexes so the receiver can detect missing chunks.

## Open Questions

- Should export include only current scan state or a historical log across many
  scans?
- Should records be kept only until reboot, or persisted to flash?
- Should the device continue scanning during export or pause scanning to avoid
  radio contention?
- What iPhone receiver should be targeted first: a generic BLE app, a small
  native app, or a Shortcut-assisted workflow?
