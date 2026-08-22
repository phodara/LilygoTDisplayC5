<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# WiFi Region Behavior

## What Is The ESP32 Arduino / ESP-IDF WiFi Stack Default Region?

For the ESP32-C5 framework used by this PlatformIO project, Espressif documents the default WiFi country as:

```text
"01" world safe mode
```

The documented default country structure is:

```text
{.cc="01", .schan=1, .nchan=11, .policy=WIFI_COUNTRY_POLICY_AUTO}
```

That means:

| Field | Meaning |
| --- | --- |
| `cc="01"` | Generic world-safe regulatory domain. |
| `schan=1` | The first allowed 2.4 GHz channel is channel 1. |
| `nchan=11` | Eleven 2.4 GHz channels are allowed, so channels 1 through 11. |
| `WIFI_COUNTRY_POLICY_AUTO` | The driver may adopt country information from an AP when connected. |

This firmware does not connect to WiFi access points during scanning, so in normal scanner use it should behave like the default world-safe country unless something else configures the WiFi country.

## Is The Region Hardwired In This Project?

No. The firmware does not currently call:

```cpp
WiFi.setCountry(...);
esp_wifi_set_country(...);
esp_wifi_set_country_code(...);
```

The WiFi setup is intentionally simple:

```cpp
WiFi.mode(WIFI_STA);
WiFi.scanNetworks(true, true);
```

So the sketch asks the ESP32 WiFi driver to scan, and the driver applies its current country, band, and regulatory rules internally.

## How Does WiFi Scan Completion Work?

The firmware starts an asynchronous scan:

```cpp
WiFi.scanNetworks(true, true);
```

Then it checks:

```cpp
WiFi.scanComplete();
```

The scan is considered complete when the ESP32 WiFi driver reports that its channel sweep is done. It is not based on waiting until no new access points appear.

A useful mental model is:

```text
for each allowed WiFi channel:
    tune the radio to that channel
    listen/probe for APs
    record APs found
done
return number of APs
```

## What About 5 GHz?

The ESP32-C5 supports both 2.4 GHz and 5 GHz WiFi. This project does not force the scan to 2.4 GHz only or 5 GHz only.

The firmware does not explicitly configure a 5 GHz country mask or band mode. If 5 GHz networks appear in scan results, they are coming from the ESP32-C5 driver scan results under its current default/regulatory behavior.

So this statement can sound odd but is accurate:

```text
The code does not set a 5 GHz region, but the driver still has regulatory behavior.
```

The sketch does not say "scan these exact 5 GHz channels." It says "scan WiFi networks," and the ESP32 WiFi stack decides which channels are allowed under its current configuration.

## If This Binary Runs Overseas, Is It US-Only?

No. The firmware does not hardcode `US`.

It uses the driver default, documented as `"01"` world-safe mode. For 2.4 GHz, that means channels 1 through 11 by default.

Practical result:

- It should not behave as a US-specific build.
- It may miss 2.4 GHz APs that only use channels 12 or 13 in countries where those channels are allowed.
- It should still see 2.4 GHz APs on channels 1 through 11.
- 5 GHz behavior is handled by the ESP32 WiFi driver's current regulatory rules.

## Does Compiling Overseas Change The Region?

No. Compiling the project in another country does not change the WiFi regulatory region.

PlatformIO downloads the same framework and library packages regardless of where the computer is located:

- Arduino ESP32 framework.
- ESP-IDF libraries.
- LovyanGFX.
- NimBLE-Arduino.

Region behavior is runtime driver configuration, not compile-location behavior.

## Is There One Universal Driver?

Mostly, yes. PlatformIO installs one ESP32 Arduino / ESP-IDF framework package for the selected platform version, not separate US, EU, or Japan driver packages.

That driver contains regulatory logic and configuration APIs. The model is:

```text
same driver/framework
+ runtime country/region setting
= allowed channels and behavior
```

## Can The PlatformIO Driver Source Be Modified?

Technically yes. PlatformIO downloads local framework sources under paths such as:

```text
~/.platformio/packages/framework-arduinoespressif32
~/.platformio/packages/framework-arduinoespressif32-libs
```

But modifying those package files directly is not recommended:

- PlatformIO can overwrite them during updates.
- Changes are not tracked in this project repository.
- Other machines will not get the modifications.
- Future debugging becomes harder.

Better options are:

- Set the country/region explicitly in firmware code.
- Use project-controlled PlatformIO build configuration where possible.
- Pin framework versions in `platformio.ini` for reproducible builds.

## Recommendation For This Project

Leave the default behavior alone unless there is a clear need to scan channels outside the world-safe default.

If the project later needs explicit regional behavior, add a small firmware-level country setting rather than editing PlatformIO's downloaded driver files.
