<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Battery Recommendations

The LilyGO T-Display C5 uses an AXP2602 PMU/fuel gauge for battery voltage,
current, and state-of-charge readings. The displayed battery percentage is a
useful estimate, but voltage is often the more reliable signal when checking
whether the battery is actually charging or nearly full.

## Full Charge Voltage

A single-cell LiPo is typically full near 4.20V. This firmware treats 4.18V as
the practical full threshold:

```cpp
static constexpr float BATTERY_FULL_VOLTAGE = 4.18f;
```

Approximate voltage guide:

| Battery voltage | Meaning |
| --- | --- |
| 4.18V to 4.20V | Effectively full |
| 4.16V to 4.17V | Very close to full |
| 4.10V | High, but still charging |
| 4.04V to 4.05V | Not full; often around the upper-middle charge range |
| 3.40V to 3.50V | Low enough for a learning cycle discharge target |

These values are approximate because voltage changes with load, cell age,
temperature, and recent charge/discharge history.

## Why Percent Can Look Stuck

The firmware reads several PMU samples, averages voltage/current, uses the
median state-of-charge sample, and then stabilizes the displayed percentage so
the header does not flicker. Small one-percent changes are intentionally ignored.

That means the display can sit at a value such as 94% even while the voltage is
rising. Near full charge, LiPo charging also slows down while the charger tapers
current, so the reported percentage may lag behind the real battery voltage.

When checking charge state, trust the voltage more than the displayed percentage:

- If voltage is rising toward 4.18V, the battery is charging.
- If voltage is around 4.17V while plugged in, the battery is nearly full.
- If voltage stays around 4.04V to 4.05V for 20 to 30 minutes while plugged in,
  try a different USB-C cable, laptop port, wall charger, or avoid using a hub.
- If voltage drops while plugged in, the board may be drawing more current than
  the USB source is providing.

## Charge Indicator Behavior

The lightning bolt appears only when measured charge current is above the
firmware threshold. Near full charge, current can taper below that threshold even
though the board is still plugged in and the battery is finishing its charge.

The display and WiFi/Bluetooth scanning also consume power while charging, so a
laptop USB port may charge more slowly than a dedicated wall charger.

## Fuel Gauge Learning Cycle

If the percentage and voltage disagree, run a simple learning cycle:

1. Charge until the battery reaches about 4.18V to 4.20V.
2. Leave it plugged in for another 20 to 30 minutes so charge current can taper.
3. Unplug and use the device until the battery is low, around 3.40V to 3.50V.
4. Charge it back to full without interruptions.

One or two cycles may help the AXP2602 fuel gauge relearn the battery range.
