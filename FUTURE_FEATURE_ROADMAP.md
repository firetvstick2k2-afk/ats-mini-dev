# ATS Mini – Future Feature Roadmap

Last updated: 2026-11-21  
Active repository: `firetvstick2k2-afk/ats-mini-dev`  
Active branch: `development`

This is a living backlog, not a promise that every item will be implemented in a fixed numbered release. New bugs and stability work always take priority over roadmap features. The active JS8Call receive-only decoder remains the current feature priority.

## Current priority

1. **JS8Call receive-only decoder**
   - Start with JS8 NORMAL.
   - Provide persistent ONLINE/OFFLINE timing selection.
   - Add a dedicated JS8 PRESETS submenu with supported HF dial frequencies and the user-requested experimental CB channel 24/25 USB entries.
   - Preserve FT8/FT4, CW, RTTY and all existing radio features.
   - No transmit path, encoder, PTT or send controls.

## Approved future ideas

Items below are intentionally deferred until the active decoder work and any intervening bug fixes are complete. Their eventual position (7, 8, 9, 10, or later) will be decided when priorities are reviewed again.

- **NAVTEX / SITOR-B receive decoder** – standalone text-first receiver with message framing, CCIR-476/FEC handling and persistent message archive. Previously approved for future work; do not start during JS8 development.
- **All-band Signal Analyzer / automatic signal identification** – while tuning across the supported LW/SW range, the user can press the encoder to analyze the currently received audio and identify likely digital modes. Initial candidates: CW, RTTY, FT8 and FT4; later candidates may include JS8, SSTV and NAVTEX. When a protocol is positively validated, offer or automatically open its matching receive decoder.
  - Analyze the existing demodulated audio stream; do not assume the ATS Mini provides full SDR IQ data.
  - Distinguish modulation recognition from protocol identification. For example, detecting two-tone FSK alone is not sufficient to confirm RTTY.
  - Use confidence/validation checks and show multiple likely candidates when evidence is ambiguous; avoid silently launching the wrong decoder.
  - CB identification may use band/channel context, but ordinary voice audio alone cannot reliably prove that a signal is CB.
  - This feature is exploratory and depends on DSP, CPU, audio bandwidth and false-positive testing. It is not part of the current JS8 implementation.

## Working rules

- Fix newly reported bugs before resuming roadmap features.
- Work only on `development`; do not modify `main` unless explicitly requested.
- Do not start firmware builds, GitHub Actions or hardware tests unless the user explicitly asks.
- Preserve existing decoder behavior, layouts, LittleFS data and the LILYGO GPIO11/SPI exclusion.
