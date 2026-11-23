# ATS Mini USB Diagnostics

## Purpose

The Settings item is named **USB-LOG** (DE), **USB Log** (EN), and **Log USB** (IT). It controls only the additional diagnostic stream. The standard USB log is independent and remains available when this switch is OFF.

The logger is intended for a phone acting as USB host, including a Samsung Galaxy S22 Ultra, with a USB serial terminal application. It does not create new diagnostic files in LittleFS and does not use Wi-Fi for log transport.

## Connection

1. Connect the ATS Mini's native USB data port to the phone with a data-capable USB-C cable/OTG connection.
2. Open a USB serial terminal app and select the ATS Mini USB serial device.
3. Use 115200 baud, 8 data bits, no parity, 1 stop bit (8N1) if the app asks.
4. The standard USB log is available without changing any setting. Enable **Settings → USB-LOG** only when additional diagnostics are needed.
5. Start the terminal app's own recording/log-to-file feature if a copy on the phone is wanted.

## Safety and transport design

- USB CDC TX timeout is set to zero so serial output cannot wait indefinitely for a disconnected or non-reading terminal.
- Standard messages use `atsStandardLogf("...")`; they are independent of the Settings switch and are intended for deliberately selected, low-volume events.
- Additional diagnostic producers use `atsDebugLogf("...")`; they enqueue fixed-size records in a bounded 48-record RAM queue and are suppressed while USB-LOG is OFF.
- The main radio loop drains the additional-diagnostics queue in small chunks only when USB-LOG is ON and USB has room; no additional FreeRTOS logging worker is created.
- ESP-IDF warning/error output remains at the normal standard log level while USB-LOG is OFF. Verbose ESP-IDF output is enabled only while USB-LOG is ON.
- If the queue fills while the phone is disconnected or not reading, the oldest records are overwritten and an overflow counter is reported when output resumes.\n- `usb_host` reports the USB bus/host link, not necessarily whether the terminal app is currently open; `usb_tx_free` and `log_queue` help identify a terminal that has stopped reading while the cable remains connected.
- The queue is RAM-only. A full power loss/reset clears buffered records; reset cause and boot memory information are logged after the saved Settings state is loaded.
- Use `atsStandardLogf("...")` for a normal message that should remain visible with USB-LOG OFF. Use `atsDebugLogf("...")` for extra diagnostics that should appear only with USB-LOG ON. Do not add blocking serial writes, file writes, or high-rate per-sample logging to time-critical radio/audio paths.

## Diagnostic coverage

- Reset cause, chip/CPU information, heap, PSRAM, task count and stack watermark
- Main-loop stalls and slow display redraws
- USB host presence, TX free space, queued records, dropped records and incoming USB serial bytes
- Radio command, mode, frequency, BFO, layout and periodic state snapshots
- Display sleep/wake requests and their sources (encoder, timeout, USB remote command)
- Audio mute transitions and hardware mute/amplifier GPIO state
- Audio capture subscriber count, task stack/heap and sample overruns
- HF Band Monitor, News, Weather and audio worker activity, memory and stack reports
- ESP-IDF verbose log output routed through the same bounded queue

## Investigation of display/audio blackout

For a black display and muted audio after the terminal app disconnects, correlate:
- `[USB][RX]` and `[USB][REMOTE]` — whether a remote command arrived (notably sleep/wake)
- `[DISPLAY][SLEEP_SOURCE]`, `[DISPLAY][SLEEP_ENTER_REQUEST]`, `[DISPLAY][WAKE_*]` — why the display entered or left sleep
- `[AUDIO][MUTE_CHANGE]` and `mute_gpio` — whether firmware asserted hardware mute
- `[RADIO][STATE_CHANGE]` — current radio/layout/sleep/mute state
- `[USB][LINK]`, `usb_tx_free`, `log_queue` — USB link/backpressure
- `[LOOP][SLOW]`, `[USB][SERIAL_LOOP_SLOW]`, `[DISPLAY][DRAW_SLOW]` — stalls in the main loop, serial handling or rendering
- `[BOOT]` reset reason — whether the ESP32 actually restarted

No firmware build or hardware test is implied by this document; verify with the normal manual build and a real S22 Ultra connection.
