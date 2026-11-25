# WLAN and EiBi centered overlays restored

- Restored the read-only WLAN information panel as a compact centered overlay over the active radio layout.
- Added a WLAN Info selection to the Wi-Fi mode menu in English, German and Italian; clicking the panel returns to the Wi-Fi selection.
- Restored centered, text-sized status pop-ups for status messages including EiBi, preserving the visible background around the window.
- Added WLAN Info to the inactivity-timeout exclusions so it remains open until the user clicks back.
- Volume, brightness and other settings pop-ups were not changed.
- FT8/FT4 decoder, audio capture and EIBI.cpp were not changed.
- Source-level review only. No compile, GitHub Actions run, firmware flash or hardware test was performed.
