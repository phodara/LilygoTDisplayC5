<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# WiFi Security Labels

The WiFi analyzer shows a short security label for each scanned access point.
These labels come from the ESP32 WiFi scan result reported by
`WiFi.encryptionType()`.

The compact labels are used in tight display areas such as the list and RSSI
history views. The BSSID detail view can show a slightly more descriptive label
where there is enough room.

## Label Meanings

| Label | Meaning |
| --- | --- |
| `OPEN` | No WiFi encryption. Anyone nearby can connect if the network does not use another access-control method. |
| `WEP` | WEP encryption. This is very old and should be treated as insecure. |
| `WPA` | WPA Personal using a shared password. |
| `WPA-PSK` | Detail-view form of `WPA`; PSK means pre-shared key, or normal shared-password WiFi. |
| `WPA2` | WPA2 Personal using a shared password. |
| `WPA2-PSK` | Detail-view form of `WPA2`; PSK means pre-shared key. |
| `WPA/WPA2` | Mixed WPA and WPA2 compatibility mode. |
| `WPA2-E` | WPA2 Enterprise. Usually uses per-user credentials or certificates through 802.1X/RADIUS instead of one shared password. |
| `WPA3` | WPA3 Personal. Newer password-based WiFi security. |
| `WPA2/3` | Mixed WPA2 and WPA3 transition mode. WPA2 clients can still connect, while WPA3-capable clients can use WPA3. |
| `SEC` | Unknown or unrecognized security type. |

## Personal vs Enterprise

Most home and small-office networks use Personal security. These are the labels
ending in `PSK`, such as `WPA2-PSK`. The password is shared by everyone who uses
the network.

Enterprise security is different. `WPA2-E` usually means the network uses
802.1X authentication with a RADIUS server. Instead of one shared WiFi password,
each user or device can have its own login, certificate, or managed credential.
This is common in businesses, schools, hospitals, and larger organizations.

## Mixed-Mode Labels

Mixed-mode labels mean the access point supports more than one security mode for
compatibility:

- `WPA/WPA2` allows older WPA clients and newer WPA2 clients.
- `WPA2/3` allows WPA2 clients and WPA3 clients.

Mixed modes are useful during transitions, but the weakest allowed mode still
matters. For example, a `WPA2/3` network can be convenient because older WPA2
devices still work, but it is not the same as a WPA3-only network.

## Scanner Limits

The scanner reports the security mode advertised by the access point. It does
not join the network, inspect passwords, validate certificates, or prove that a
network is safe.

Use the labels as quick radio-environment clues:

- `OPEN` and `WEP` are weak or unencrypted.
- `WPA2-PSK` is the common modern home-network baseline.
- `WPA2/3` suggests a network in WPA3 transition mode.
- `WPA2-E` suggests an organization-managed network.
