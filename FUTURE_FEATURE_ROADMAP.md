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

## Hardware-unmodified decoder port candidates (2026-11-22)

- **PSK31** — candidate for software-only receive decoding using the existing internal audio capture path; feasibility and CPU/memory impact still require validation. No microphone or hardware modifications permitted.
- **WEFAX / weather fax** — add to the port-candidate list, conditional on validating receive-only decoding on stock ATS Mini V4 with its internal audio path and no microphone or hardware modification. The current shared capture is 4 kHz (Nyquist 2 kHz), while conventional weather-fax audio tones can extend above 2 kHz. Investigate a compatible software-only capture-rate/BFO strategy without breaking existing shared-audio decoders. CPU, memory, display rendering and on-air decoding remain unverified; do not claim working support yet.
- **SSTV** — excluded from this user's port list due to the hardware-modification requirement in their intended setup.
- **Web Control** — already implemented; do not list as a new porting priority.
- **All-band spectrum analyzer** — already present per user; do not list as a new porting priority.

Do not implement or build these candidates without explicit instruction. Preserve `main` and all existing decoder behavior.
