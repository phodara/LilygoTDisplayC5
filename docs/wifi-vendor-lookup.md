<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# WiFi Vendor Lookup Notes

WiFi vendor lookup is **not currently enabled in the firmware**.

The feature was tried in the WiFi details panel and serial scan output, then
removed because it was not useful enough for the display space it consumed.

## How It Worked

WiFi does not advertise a Bluetooth-style manufacturer data field. The only
available clue is usually the first three bytes of the AP BSSID/MAC address.
Those bytes are the OUI prefix, which may be assigned to the AP vendor, radio
module vendor, chipset vendor, or ISP gateway hardware supplier.

Example:

```text
BSSID F0:9F:C2:12:34:56
OUI   F0:9F:C2
VEND  Ubiquiti
```

For locally administered BSSIDs, the lookup displayed:

```text
VEND local
```

That means the BSSID is not a normal globally assigned vendor prefix. It may be
generated for a mesh node, guest network, virtual AP, hotspot, or randomized
address.

## Why It Was Removed

The lookup was often a weak signal:

- ISP gateways often resolved to module vendors such as WNC, Arcadyan, or
  CommScope instead of the consumer-facing router or ISP brand.
- Guest, mesh, and hidden networks often used locally administered BSSIDs.
- Unknown prefixes still needed to fall back to hex.
- The extra text competed with more useful scan details on the small display.

## Observed Useful Prefixes

These prefixes were seen during local testing and verified against IEEE registry
data when practical:

| Prefix | Short Name |
| --- | --- |
| `78:67:0E` | WNC |
| `DC:4B:A1` | WNC |
| `8C:8B:5B` | WNC |
| `24:41:FE` | WNC |
| `58:96:71` | WNC |
| `AC:91:9B` | WNC |
| `8C:76:3F` | CommScope |
| `1C:93:7C` | CommScope |
| `BC:F8:7E` | Arcadyan |
| `00:0F:92` | Microhard |

## Re-Adding Later

If this feature is ever useful again, keep it lightweight:

- Use a curated table, not the full IEEE registry.
- Prefer short display names.
- Show `local` for locally administered BSSIDs instead of guessing.
- Consider keeping it in serial/debug output only, rather than on the main
  display.

The official IEEE OUI registry is here:

```text
https://standards-oui.ieee.org/oui/oui.txt
https://standards-oui.ieee.org/oui/oui.csv
```
