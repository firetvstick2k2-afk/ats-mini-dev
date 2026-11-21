# JS8Call Receive-Only Decoder – Development Plan

Status: research and architecture baseline  
Target repository: `firetvstick2k2-afk/ats-mini-dev`  
Target branch: `development` only  
Hardware: ATS Mini V4 / ESP32-S3 / SI473x  
Scope: receive and decode JS8 messages only. No transmit path, no encoder, no TX controls, no transmit hardware.

## 1. Goal

Add a native JS8Call receive-only decoder to the ATS Mini. It should detect JS8 signals in the receiver's demodulated USB audio, decode complete JS8 frames, parse their message payloads and show received text on the display. The first milestone is reliable reception of JS8 NORMAL; further JS8 submodes can follow after CPU/RAM and sensitivity testing.

The existing FT8/FT4 and CW/RTTY implementations must remain functional and unchanged unless a shared, isolated infrastructure improvement is necessary.

## 2. Protocol facts

JS8Call is two layers: the JS8 8-FSK physical transport and a separate directed/free-text messaging protocol. It is not an FT8 text decoder. Some physical-layer ancestry is shared with FT8, but the JS8 synchronization patterns, information-bit layout, error-checking and message layer must be implemented explicitly. FT8 payload parsing cannot be reused as-is.

The current JS8Call-improved guide documents these modes:

| Mode | Frame interval | TX waveform | Symbol rate / tone spacing | Approx. bandwidth |
|---|---:|---:|---:|---:|
| SLOW | 30 s | 25.28 s | 3.125 baud / 3.125 Hz | 25 Hz |
| NORMAL | 15 s | 12.64 s | 6.25 baud / 6.25 Hz | 50 Hz |
| FAST | 10 s | 7.90 s | 10 baud / 10 Hz | 80 Hz |
| JS8 40 | 6 s | 3.95 s | 20 baud / 20 Hz | 160 Hz |
| JS8 60 | 4 s | 3.0 s | experimental | approx. 250 Hz |

The four established modes use 8 tones and 79 symbols per waveform. JS8 60 is experimental and its specifications/performance may change; it is not part of the initial implementation. NORMAL is first. SLOW and FAST follow only after NORMAL is validated; JS8 40 is later and JS8 60 last. The guide states that JS8 40/60 do not participate in the Heartbeat network.

Synchronization is mode-specific. NORMAL retains the three 7-symbol Costas blocks used for backwards compatibility; SLOW, FAST and JS8 40 use three unique 7×7 Costas arrays. Do not reuse FT8's Costas sequence or assume that an FT8 decoder will synchronize JS8. The JS8 frame contains 79 symbols and its protocol layer classifies decoded frames as heartbeat, compound callsign partial, compound callsign directed command, directed command, data Huffman, or data dictionary. A 2-bit transmission flag distinguishes default, first, last and reserved frame roles. Data frames carry a 75-bit variable-encoded payload; callsigns, prefixes/suffixes, signal reports and Maidenhead grids have dedicated encodings. Multi-frame text must be assembled with the first/last semantics respected.

USB is required. The official guide warns that clock error greater than about two seconds can prevent decoding; timing is relative to the transmission-window start (15/10/6/30 seconds), not merely a cosmetic timestamp. Use valid existing UTC when available and expose the timing state. Do not fabricate UTC or silently pretend that arbitrary audio alignment is slot-synchronized. A separate signal-derived/manual timing fallback may be investigated after the synchronized NORMAL path works.

## 3. Existing ATS Mini infrastructure## 3. Existing ATS Mini infrastructure

Inspected on `development`:

- `ats-mini/src/audio/AudioCaptureService.h/.cpp`: shared GPIO11 ADC capture at 4,000 samples/s, blocks of 24 samples, up to four subscribers.
- `ats-mini/src/ftx/FtxDecoder.h/.cpp`: isolated DSP/slot engine fed by caller-provided audio samples; currently FT8/FT4 only.
- `ats-mini/src/ftx/FtxDecoderService.h/.cpp`: worker task, queued audio blocks, slot processing, result publication and existing offline FT8 timing work.
- `ats-mini/src/ftx/FtxUi.cpp`: FT8/FT4 UI, audio capture subscription, network-mode handling and frequency preset integration.
- `ats-mini/src/radio/DecoderFrequencyList.h`: shared decoder preset catalogue (inspect current contents before extending).
- `ats-mini/ats-mini.ino`: top-level decoder UI integration.
- The current `LILYGO_SI473X` build profile deliberately disables GPIO11-based audio decoding because GPIO11 is used as SPI MOSI and the board lacks the ATS Mini audio input. Preserve that guard.

The shared 4 kHz capture is a possible input source for JS8. It is not proof that the current FT8 DSP engine can decode JS8 directly: symbol timing, tone spacing, Costas synchronization, soft decisions and JS8 payload framing differ. Validate the ADC audio passband and sample-rate stability on real signals before selecting FFT sizes or claiming sensitivity.

## 4. Proposed architecture

Create a separate JS8 subsystem, initially under `ats-mini/src/js8/`:

- `Js8Decoder.h/.cpp`: physical-layer signal processing, synchronization, 8-tone symbol decisions, soft information, error correction and CRC validation.
- `Js8DecoderService.h/.cpp`: FreeRTOS worker and bounded audio queue; subscribe to `sharedAudioCapture()` rather than creating a second ADC sampler.
- `Js8Protocol.h/.cpp`: parse decoded frame type/flags, callsigns, grids, directed commands and text/data payloads; reassemble multi-frame messages.
- `Js8Ui.h/.cpp`: receive-only screen, decoded message history, signal/audio status and controls limited to RX options.

The UI must be text-first on the 320×170 display. Incoming messages are appended line-by-line; in live-follow mode, new lines appear at the bottom and older lines move upward automatically. Provide a pause/resume control and manual history scrolling so the user can stop the automatic movement and read older messages. Keep the capture/logging pipeline independent from the visible scroll position: pausing or leaving the live-follow view must never pause reception or lose messages. Logging is a separate opt-in action and must persist decoded messages independently of the UI refresh cycle.
- Add the new files to `ats-mini/Makefile` SRC/HEADERS lists as required by repository development instructions.

The UI must not contain a transmit text box, Send button, TX enable, PTT action, encoder/modulator or any call into transmit code. Do not add TX capability as a hidden future option.

The service should follow the existing worker/queue pattern and protect decoder/result state with appropriate mutexes. Avoid doing FFT/LDPC work inside the ADC sampling callback. Use PSRAM for larger rolling buffers where appropriate; keep time-critical capture and small working state bounded.

Network behavior should match the existing digital decoder convention: disable Wi-Fi while decoding if needed to protect audio/DSP timing, and restore the user's previous network mode on exit. Do not alter the FT8/FT4 service lifecycle.

## 5. Implementation sequence

1. **Baseline and audio validation**
   - Confirm sample stream polarity, DC offset, clipping, audio passband and effective sample rate.
   - Capture representative JS8 NORMAL audio as offline test vectors (licensed/public samples or user-provided recordings).
   - Establish memory, CPU, queue-drop and worker-stack baselines.

2. **NORMAL physical-layer receive**
   - Implement JS8-specific sync/Costas search and 8-FSK symbol timing.
   - Recover soft decisions and decode the JS8 channel code with CRC validation.
   - Do not assume the FT8 Costas sequence or FT8 payload decoder is interchangeable.
   - Verify against known reference vectors before connecting the display.

3. **JS8 protocol parser**
   - Decode the six frame classes and frame flags.
   - Decode callsign/grid encodings and directed command/data payloads.
   - Reassemble fragmented messages and discard invalid/incomplete frames safely.
   - Display unknown or unsupported frame types diagnostically rather than fabricating text.

4. **Receive-only UI and history**
   - Show frequency, USB, JS8 mode, UTC/timing lock and audio/signal state.
   - Append decoded messages line-by-line and auto-scroll older lines upward in live-follow mode.
   - Add pause/resume and manual back-scroll/history navigation; reception and logging continue while the display is paused or scrolled back.
   - Add a separate logging toggle and bounded LittleFS message archive only after valid live decoding is stable.

5. **Additional submodes**
   - Add SLOW and FAST after NORMAL is stable.
   - Evaluate JS8 40 and experimental JS8 60 last; only include if ESP32-S3 timing and memory allow reliable decode.
   - Multi-mode simultaneous decoding is out of scope for the first milestone.

6. **Validation**
   - Host-side/reference-vector tests for synchronization, payload/CRC, callsign/grid parsing and multi-frame assembly.
   - Repository checks only when explicitly requested.
   - Do not start GitHub Actions builds or compile firmware unless the user asks. Hardware reception and real-world sensitivity must be verified by the user.

## 6. Licensing and source-use boundary

The ATS Mini repository is MIT-licensed. The JS8Call reference implementation is GPLv3. Do not copy GPLv3 source files into the MIT project or perform a direct code port without first resolving the license compatibility and attribution implications. Use public protocol documentation and independently written implementation, or obtain an explicit licensing decision before importing upstream code. Record provenance for any algorithm or reference material used.

## 7. Acceptance criteria for the first milestone

- JS8 NORMAL signals can be synchronized and decoded from the ATS Mini's existing audio capture.
- Valid frames pass error/CRC checks; invalid frames do not appear as text.
- Directed and free-text messages are displayed as received, with sensible multi-frame assembly.
- No transmit UI, transmit routine, PTT path or encoder exists in the JS8 feature.
- FT8/FT4, CW, RTTY, radio layouts and existing presets continue to work.
- The LILYGO GPIO11/SPI guard remains intact.
- CPU load, heap/PSRAM use, queue drops and decoder latency are observable during tests.
- Firmware is not claimed as built or hardware-tested until the user performs those steps.

## References

- JS8Call-improved User Guide (mode timing, USB/clock requirements, protocol and message behavior): https://js8call.com/JS8Call-improved/d6/d14/md_docs_2user__guide_2JS8Call__User__Guide.html
- JS8Call-improved source file index: https://js8call.com/JS8Call-improved/files.html
- JS8Call source and license notice: https://github.com/JS8Call-improved/JS8Call-improved
- Original JS8Call repository: https://github.com/js8call/js8call
