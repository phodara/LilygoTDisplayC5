<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Project Notes

## Wi-Fi Analyzer Feature Ideas

1. [ ] Channel Congestion View
   Show how many APs are on each channel, split between 2.4 GHz and 5 GHz. Useful for picking a cleaner channel.

2. [x] Signal History Graph
   Track the selected network's RSSI over time as a scrolling mini graph. Great for walking around and finding dead spots.

3. [ ] Best Network / Best Band Hint
   Highlight the strongest 2.4 GHz and strongest 5 GHz AP, plus a simple best-candidate indicator.

4. [ ] SSID Grouping
   Group networks with the same SSID, so mesh networks like MeshNet show as one SSID with multiple BSSIDs/APs.

5. [x] BSSID / MAC Detail Page
   Show the AP MAC address, useful for distinguishing mesh nodes or extenders.

6. [ ] Security Audit View
   Count open, WPA2, WPA3, and mixed WPA/WPA2 networks. Flag open or legacy security.

7. [ ] Hidden Network Count
   Show how many hidden SSIDs are nearby and which channels they are on.

8. [ ] Band Summary
   Count 2.4 GHz vs 5 GHz networks and show average RSSI per band.

9. [ ] Auto-Refresh Rate Control
   Use the buttons to toggle scan interval: fast, normal, and slow. Fast for walking around, slow for desk mode.

10. [ ] Selected Network Lock
    Lock onto one SSID/BSSID and track only that AP's signal over time.

Top pick: Signal History Graph, because it makes the device feel like an actual handheld analyzer instead of just a scan list.
