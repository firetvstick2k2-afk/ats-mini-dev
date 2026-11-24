# JS8Call Receive-Only Decoder – Development Plan

Status: research and architecture baseline  
Target repository: `firetvstick2k2-afk/ats-mini-dev`  
Target branch: `development` only  
Hardware: ATS Mini V4 / ESP32-S3 / SI473x  
Scope: receive and decode JS8 messages only. No transmit path, no encoder, no TX controls, no transmit hardware.

## 1. Goal

Add a native JS8Call receive-only decoder to the ATS Mini. It should detect JS8 signals in the receiver's demodulated USB audio, decode complete JS8 frames, parse their message payloads and show received text on the display. The first milestone is reliable reception of JS8 NORMAL; further JS8 submodes can follow after CPU/RAM and sensitivity testing. The finished receiver should offer an explicit MODE selection with AUTO plus individual speed choices. AUTO is intended to recognize and decode whichever supported JS8 speed is active, using validated mode-specific synchronization rather than guessing from signal strength.

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

The four established modes use 8 tones and 79 symbols per waveform. JS8 60 is experimental and its specifications/performance may change; it is not part of the initial implementation. JS8Call supports MULTI reception, which decodes multiple speed modes at once; the ATS Mini should provide an equivalent AUTO receive option if the ESP32-S3 has sufficient CPU and memory headroom. NORMAL is first. SLOW and FAST follow only after NORMAL is validated; JS8 40 is later and JS8 60 last. The guide states that JS8 40/60 do not participate in the Heartbeat network.

Synchronization is mode-specific. NORMAL retains the three 7-symbol Costas blocks used for backwards compatibility; SLOW, FAST and JS8 40 use three unique 7×7 Costas arrays. Do not reuse FT8's Costas sequence or assume that an FT8 decoder will synchronize JS8. The JS8 frame contains 79 symbols and its protocol layer classifies decoded frames as heartbeat, compound callsign partial, compound callsign directed command, directed command, data Huffman, or data dictionary. A 2-bit transmission flag distinguishes default, first, last and reserved frame roles. Data frames carry a 75-bit variable-encoded payload; callsigns, prefixes/suffixes, signal reports and Maidenhead grids have dedicated encodings. Multi-frame text must be assembled with the first/last semantics respected.

USB is required. The official guide warns that clock error greater than about two seconds can prevent decoding; timing is relative to the transmission-window start (15/10/6/30 seconds), not merely a cosmetic timestamp. The decoder must therefore provide an explicit, persistent **TIME SYNC: ONLINE/OFFLINE** setting, matching the established FT8 user experience:

- **ONLINE** uses valid system UTC/NTP time to align JS8 receive windows.
- **OFFLINE** must not require Wi-Fi, must not set or fabricate wall-clock UTC, and must acquire/maintain a JS8-specific frame phase from received JS8 signals or another validated signal-derived method. Until phase is acquired, show a searching/unlocked state and do not claim synchronized decoding.
- Store the selection in ESP32 Preferences/NVS so it survives decoder exit and power cycles. Default to ONLINE when no saved preference exists.
- The JS8 timing implementation must be independent of FT8 decoder internals; only the UX concept and safe shared infrastructure may be reused. OFFLINE must be validated against JS8 signals, not inferred from FT8 lock.

## 2a. Verified physical-layer frame dimensions and sample-rate implications

The current JS8Call source confirms the common channel dimensions:

- 79 transmitted symbols per frame: 21 synchronization symbols (three 7-symbol Costas blocks) and 58 data symbols.
- The channel code is LDPC(174,87): 87 information bits (75 payload bits plus a 12-bit CRC) and 87 parity bits.
- CRC-12 uses polynomial 0xC06 with the JS8-specific XOR value 42. CRC validation is mandatory before a decoded payload is passed to the message parser.
- NORMAL uses the original Costas sequence; the other established speeds use modified mode-specific synchronization arrays. Do not accept a mode based on tone energy or a sync-like pattern alone.

The upstream implementation processes 12 kHz audio, while the ATS Mini shared capture currently provides 4 kHz. Do not copy upstream FFT/sample-count constants into the ATS Mini. At 4 kHz, the nominal samples per symbol are exactly 640 (NORMAL), 1,280 (SLOW), 400 (FAST), and 200 (JS8 40). These are arithmetic planning values, not proof that the receiver audio chain preserves the required tone spacing, phase, frequency stability or symbol edges. Validate the actual 4 kHz capture path with recorded reference signals before choosing filters, FFT lengths, downsampling or synchronization windows. JS8 60 remains excluded because its published parameters are experimental/unspecified.

Source provenance: these frame dimensions, code parameters and CRC parameters were checked against the public JS8Call source and user guide. The ATS Mini implementation must be independently written; do not copy GPL-licensed source code or source-derived tables into this MIT repository without an explicit licensing decision.

## 2b. Receive-chain constraints and validation gates

### Audio passband

The ATS Mini capture is 4 kHz, so its Nyquist limit is 2 kHz. The upstream JS8 decoder's baseline-estimation window reaches 2.5 kHz; that full upstream window cannot be copied to this device. The 4 kHz stream must be treated as a distinct receive front end, not as a drop-in 12 kHz stream.

For NORMAL, the symbol interval is 160 ms and the 4 kHz capture contains 640 samples per symbol. The eight orthogonal tone bins are separated by 6.25 Hz. A practical first experiment should place the received JS8 signal well inside the actual ATS Mini audio passband (for example, around a 1 kHz audio center), then measure tone-bin separation, audio filter response, frequency drift, clipping, and symbol-edge stability on the GPIO11 capture. The chosen audio center must be confirmed on the real radio; it is not a hard-coded assumption.

### Processing and memory

- Keep the 15-second NORMAL receive window bounded. At 4 kHz mono signed 16-bit, a full 15-second raw window is 120,000 bytes; use PSRAM where available and avoid copying the whole window for every candidate.
- The 24-sample AudioCaptureService callback is a producer only: enqueue/copy into a bounded ring buffer and return quickly. FFT/Goertzel, sync search, LDPC and message parsing must run in a decoder worker, never inside the ADC sampling task.
- Add explicit overrun counters and recover cleanly after dropped audio. A partial or discontinuous frame must never be reported as a valid decode.
- Keep JS8 independent of the FT8 decoder and parser. Share only the established audio capture service and small generic helpers where their contracts are genuinely identical.

### Validation gates before firmware integration

1. Generate deterministic NORMAL 8-FSK reference frames independently from the decoder and retain exact symbol, tone-frequency, timing and payload metadata. Do not use a JS8Call GPL implementation as an embedded dependency.
2. Test the receive DSP offline against synthetic 4 kHz PCM with known carrier offset, start-time offset, gain, noise and small sample-clock error. Verify sync acquisition and soft tone decisions before implementing LDPC.
3. Verify the LDPC(174,87) bit order, interleaving/permutation, CRC-12 augmentation and XOR convention against independently generated golden bit vectors. A CRC pass alone is not enough: the recovered 75-bit payload must match the exact test vector.
4. Test the complete NORMAL decode path on host before moving it to the ESP32-S3. Record CPU time, peak RAM and worst-case candidate count.
5. Only then integrate the shared audio subscriber and run live receiver tests. No build/Actions run is started by the assistant unless the user explicitly asks.

### Timing modes

NORMAL is the only first target. Its signal starts 500 ms after the 15-second slot boundary and lasts 12.64 seconds. Offline mode must acquire the phase from real JS8 sync candidates and establish a monotonic 15-second grid; it must not infer a UTC timestamp from the capture callback's wall-clock field. Do not claim OFFLINE LOCK until a candidate passes the physical-layer validation threshold. SLOW/FAST/JS8 40 follow after NORMAL passes the same validation gates.

## 3. JS8 frequency presets

Add a dedicated **JS8 PRESETS** submenu inside the JS8 decoder Actions menu. It is separate from the general `Decoder Frequencies` browser and should use the same preview/accept interaction as the existing FT8 preset submenu.

The JS8Call-improved guide lists these suggested HF dial frequencies. Include only bands supported by the ATS Mini V4 receive range (150 kHz–30 MHz):

| Band | JS8 dial frequency | Receiver mode |
|---|---:|---|
| 160 m | 1.842 MHz | USB |
| 80 m | 3.578 MHz | USB |
| 40 m | 7.078 MHz | USB |
| 30 m | 10.130 MHz | USB |
| 20 m | 14.078 MHz | USB |
| 17 m | 18.104 MHz | USB |
| 15 m | 21.078 MHz | USB |
| 12 m | 24.922 MHz | USB |
| 10 m | 28.078 MHz | USB |

The guide's 6 m (50.318 MHz) and 2 m (144.178 MHz) suggestions are outside the ATS Mini's supported receive range and must not be offered. The guide does not recommend an 11 m/CB calling frequency; the following two entries are user-requested experimental receive presets only:

| Label | Frequency | Receiver mode |
|---|---:|---|
| CB CH 24 (experimental) | 27.235 MHz | USB |
| CB CH 25 (experimental) | 27.245 MHz | USB |

Keep these JS8-specific CB entries local to the JS8 preset list. Do not change the shared CB receiver presets in `DecoderFrequencyList.h`, which remain AM. All JS8 presets must select USB.

## 4. Existing ATS Mini infrastructure

Inspected on `development`:

- `ats-mini/src/audio/AudioCaptureService.h/.cpp`: shared GPIO11 ADC capture at 4,000 samples/s, blocks of 24 samples, up to four subscribers.
- `ats-mini/src/ftx/FtxDecoder.h/.cpp`: isolated DSP/slot engine fed by caller-provided audio samples; currently FT8/FT4 only.
- `ats-mini/src/ftx/FtxDecoderService.h/.cpp`: worker task, queued audio blocks, slot processing, result publication and existing offline FT8 timing work.
- `ats-mini/src/ftx/FtxUi.cpp`: FT8/FT4 UI, audio capture subscription, network-mode handling and frequency preset integration.
- `ats-mini/src/radio/DecoderFrequencyList.h`: shared decoder preset catalogue.
- `ats-mini/ats-mini.ino`: top-level decoder UI integration.
- The current `LILYGO_SI473X` build profile deliberately disables GPIO11-based audio decoding because GPIO11 is used as SPI MOSI and the board lacks the ATS Mini audio input. Preserve that guard.

The shared 4 kHz capture is a possible input source for JS8. It is not proof that the current FT8 DSP engine can decode JS8 directly: symbol timing, tone spacing, Costas synchronization, soft decisions and JS8 payload framing differ. Validate the ADC audio passband and sample-rate stability on real signals before selecting FFT sizes or claiming sensitivity.

## 5. Proposed architecture

Create a separate JS8 subsystem, initially under `ats-mini/src/js8/`:

- `Js8Decoder.h/.cpp`: physical-layer signal processing, synchronization, 8-tone symbol decisions, soft information, error correction and CRC validation.
- `Js8DecoderService.h/.cpp`: FreeRTOS worker and bounded audio queue; subscribe to `sharedAudioCapture()` rather than creating a second ADC sampler.
- `Js8Protocol.h/.cpp`: parse decoded frame type/flags, callsigns, grids, directed commands and text/data payloads; reassemble multi-frame messages.
- `Js8Ui.h/.cpp`: receive-only screen, decoded message history, signal/audio status and controls limited to RX options.

The UI must be text-first on the 320×170 display. Incoming messages are appended line-by-line; in live-follow mode, new lines appear at the bottom and older lines move upward automatically. Provide a pause/resume control and manual history scrolling so the user can stop the automatic movement and read older messages. Keep the capture/logging pipeline independent from the visible scroll position: pausing or leaving the live-follow view must never pause reception or lose messages. Logging is a separate opt-in action and must persist decoded messages independently of the UI refresh cycle.

- Add the new files to `ats-mini/Makefile` SRC/HEADERS lists as required by repository development instructions.
- Keep the Actions menu explicit and compact: JS8 PRESETS, MODE AUTO/NORMAL/SLOW/FAST/JS8 40/JS8 60, TIME SYNC ONLINE/OFFLINE, speaker/audio option if appropriate, message history/log controls, BACK and EXIT.
- AUTO should identify the active JS8 speed by trying the mode-specific symbol timing, tone spacing and Costas/synchronization patterns. A speed is accepted only after the JS8 frame decoder validates it; signal strength or bandwidth alone is not enough.
- Prefer simultaneous multi-speed decoding only if measured CPU, RAM, queue latency and sensitivity remain acceptable. If all-mode parallel decoding is too expensive, implement an AUTO candidate scheduler that tests likely speeds efficiently and reports the detected speed only after frame validation. JS8 60 remains experimental and may be excluded if its specification or resource cost prevents reliable operation.

The UI must not contain a transmit text box, Send button, TX enable, PTT action, encoder/modulator or any call into transmit code. Do not add TX capability as a hidden future option.

The service should follow the existing worker/queue pattern and protect decoder/result state with appropriate mutexes. Avoid doing FFT/LDPC work inside the ADC sampling callback. Use PSRAM for larger rolling buffers where appropriate; keep time-critical capture and small working state bounded.

Network behavior should match the selected timing mode and existing digital decoder convention: ONLINE may use the existing valid UTC/NTP clock; OFFLINE must work without Wi-Fi and must not invent UTC. Disable Wi-Fi while decoding if needed to protect audio/DSP timing, and restore the user's previous network mode on exit. Do not alter the FT8/FT4 service lifecycle.

## 6. Implementation sequence

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

4. **Receive-only UI, timing and history**
   - Show frequency, USB, JS8 mode, selected ONLINE/OFFLINE timing mode and actual synchronization state.
   - ONLINE must use valid UTC; OFFLINE must show searching/unlocked until JS8-specific signal-derived phase is validated.
   - Append decoded messages line-by-line and auto-scroll older lines upward in live-follow mode.
   - Add pause/resume and manual back-scroll/history navigation; reception and logging continue while the display is paused or scrolled back.
   - Add the dedicated JS8 PRESETS submenu with the HF presets above and the two explicitly experimental CB presets.
   - Add a separate logging toggle and bounded LittleFS message archive only after valid live decoding is stable.

5. **Additional submodes and automatic mode detection**
   - Add SLOW and FAST after NORMAL is stable.
   - Evaluate JS8 40 and experimental JS8 60 last; only include if ESP32-S3 timing and memory allow reliable decode.
   - Add AUTO/MULTI receive mode after the individual mode decoders are validated. It should detect the active speed and decode supported speeds without requiring the user to select one manually.
   - Benchmark true parallel decoding against a lower-cost candidate scheduler. Keep AUTO disabled or limit its supported modes if it overloads the ESP32-S3 or degrades decode sensitivity.

6. **Validation**
   - Host-side/reference-vector tests for synchronization, payload/CRC, callsign/grid parsing and multi-frame assembly.
   - Repository checks only when explicitly requested.
   - Do not start GitHub Actions builds or compile firmware unless the user asks. Hardware reception and real-world sensitivity must be verified by the user.

## 7. Licensing and source-use boundary

The owner explicitly approved adapting the JS8Call GPLv3 implementation for the JS8 receive-only decoder and changed the repository to private for development. This supersedes the earlier clean-room-only restriction in the initial planning text. Keep the JS8 component's GPLv3 notices, COPYING and NOTICE files intact. The repository's private visibility does not remove GPL obligations if the combined firmware/source is later conveyed or distributed; review the applicable corresponding-source and installation-information requirements before any external distribution.


## 8. Acceptance criteria for the first milestone

- JS8 NORMAL signals can be synchronized and decoded from the ATS Mini's existing audio capture.
- Valid frames pass error/CRC checks; invalid frames do not appear as text.
- Directed and free-text messages are displayed as received, with sensible multi-frame assembly.
- ONLINE and OFFLINE are explicit persistent choices; OFFLINE does not require Wi-Fi or fabricate UTC and only reports lock after JS8-specific timing acquisition.
- JS8 PRESETS contains the supported HF dial frequencies plus only CB channels 24 and 25 as experimental USB receive presets.
- MODE offers AUTO and individual speeds. AUTO identifies a speed only after valid JS8 frame synchronization/validation and does not rely on signal strength alone.
- No transmit UI, transmit routine, PTT path or encoder exists in the JS8 feature.
- FT8/FT4, CW, RTTY, radio layouts and existing shared presets continue to work.
- The LILYGO GPIO11/SPI guard remains intact.
- CPU load, heap/PSRAM use, queue drops and decoder latency are observable during tests.
- Firmware is not claimed as built or hardware-tested until the user performs those steps.

## References

- JS8Call-improved User Guide (timing, USB requirement, mode timing and suggested HF dial frequencies): https://js8call.com/JS8Call-improved/d6/d14/md_docs_2user__guide_2JS8Call__User__Guide.html
- JS8Call-improved source file index: https://js8call.com/JS8Call-improved/files.html
- JS8Call source and license notice: https://github.com/JS8Call-improved/JS8Call-improved
- Original JS8Call repository: https://github.com/js8call/js8call


## 2c. Source audit of the actual ATS Mini capture scheduler (2026-11-21)

The live AudioCaptureService.cpp was inspected rather than treating its declared 4 kHz rate as a hardware-clock guarantee:

- GPIO11 is sampled by repeated software calls to analogRead(); this is not an I2S/DMA audio stream. The task schedules reads against esp_timer_get_time() and a nominal 4,000-sample/s target.
- Samples are delivered in blocks of 24 (nominally 6 ms). The callback timestamp is derived from gettimeofday() at block delivery and back-calculated from the block length; it is not a per-sample monotonic hardware timestamp.
- Every 2,000 samples the capture task calls vTaskDelay(1) and resets its scheduling base. The cost/jitter of analogRead(), task scheduling and this periodic yield therefore need to be measured; kSampleRate = 4000 alone does not prove an exact or low-jitter sample clock.
- The ADC values are passed as raw signed 16-bit integers representing the ADC reading; the existing FTX consumer removes a slowly varying DC estimate and scales the result. JS8 must independently validate DC offset, usable amplitude, clipping, noise and sample-clock behavior.
- The existing LILYGO guard is explicit: AudioCaptureService::begin() returns false for LILYGO_SI473X because GPIO11 is SPI MOSI on that profile. JS8 must preserve this restriction.

### Consequence for the first implementation milestone

Do not begin by adding the JS8 LDPC decoder or a UI. First add a non-invasive, JS8-owned capture diagnostic path that subscribes to the existing service and measures, without changing the FT8/CW/RTTY consumers:

1. callback cadence and long-term sample-clock drift using monotonic esp_timer_get_time() observations and the known sample count per block;
2. queue overruns, missing blocks and discontinuities;
3. ADC minimum/maximum, mean/DC, RMS/AC level and clipping frequency over bounded windows;
4. spectral response and tone-bin separation using controlled 4 kHz test signals, followed by legally usable real JS8 NORMAL recordings.

The callback must only copy/enqueue a bounded block and timestamp it. All window statistics and spectral analysis belong in the JS8 worker. A measured callback cadence is only an estimate of the capture scheduler output timing; it cannot expose every intra-block ADC interval. If the diagnostic shows unacceptable jitter or drift, evaluate a narrowly scoped timing improvement to the shared capture service separately and regression-test existing digital decoders before adopting it.

Do not change the shared sampler just to suit JS8 before this measurement. In particular, do not create a second ADC sampler, do not alter FT8/CW/RTTY behavior as a side effect, and do not claim that the 4 kHz stream is suitable until the real capture path passes these checks.


## 2d. First diagnostic subscriber implementation (2026-11-21)

A JS8-owned diagnostic subscriber has now been added under ats-mini/src/js8/:

- Js8AudioMonitor.h/.cpp subscribes to the existing AudioCaptureService; it never calls analogRead() and does not create or change an ADC sampler.
- The callback captures a monotonic esp_timer_get_time() observation, copies the bounded 24-sample block into a 96-block queue, increments minimal callback/queue-drop counters, and returns. It does not perform sample statistics, filtering or spectral analysis.
- A separate low-priority worker estimates delivered block cadence (interval min/max/mean and effective output rate), counts long gaps/missing-block estimates, measures ADC min/max/DC/AC-RMS/near-rail samples, and retains the newest 4,096 raw samples in a PSRAM-first ring buffer (internal-heap fallback) for later spectral analysis.
- The wall-clock callback timestamp is intentionally ignored for cadence measurement. The reported rate is an estimate from callback delivery timestamps and known block sample counts; it is not a measurement of individual analogRead() intervals or intra-block jitter.
- The service exposes begin(), stop(), copyMetrics() and copyRecentSamples(). A separate JS8 RX Monitor menu item (immediately after FT8 / FT4) opens a dedicated diagnostic page and starts/stops only this JS8-owned subscriber. The page reports callback-derived rate/interval and ADC level metrics; it does not yet demodulate or decode JS8.
- The LILYGO GPIO11/SPI guard remains intact. Makefile wildcard lists include the new src/js8 files.
- Next gate: source-review the menu/UI/lifecycle integration, then collect controlled-tone and lawful JS8 NORMAL measurements on hardware. Do not implement NORMAL DSP until the actual capture evidence is acceptable.
- No build, repository check, GitHub Actions run, flash or hardware test has been performed.


## 8. Deterministic 8-FSK playback harness (implemented; not a JS8 decode vector)
The JS8 RX Monitor now supports a built-in repeatable synthetic signal source, activated by short click and stopped by another short click. It temporarily replaces only the JS8 monitor's live GPIO11 subscription, generates a nominal 4 kHz 8-FSK sequence with 79 symbols × 640 samples (160 ms per symbol), 6.25 Hz tone spacing and an audio center near 1 kHz, then feeds blocks through the monitor worker. Live capture resumes automatically when playback ends or is stopped.

Important: this first waveform is deliberately a generic deterministic 8-FSK stimulus, not a protocol-valid JS8 NORMAL frame. It does not yet include verified JS8 Costas sync, LDPC(174,87), JS8 CRC-12 or a known message payload. It can exercise the monitor's block processing, timing and level metrics, but cannot prove successful JS8 synchronization or decoding. The next validation artifact must be an independently generated and verified protocol-valid NORMAL golden frame with expected decoded payload and metadata. No GPL JS8Call source is embedded.

The playback path currently terminates at the monitor worker; a dedicated JS8 decoder sample-consumer interface remains a next implementation step. No build or hardware test has been performed.


## 8.1 First build and ATS Mini monitor test procedure
This is the first device validation gate for the current monitor-only milestone. It is not yet a JS8 decoder build. After the user starts the manual OSPI build on development and flashes the resulting firmware:

1. Open MENU → JS8 RX MONITOR. The page should appear without changing the tuned frequency, receiver mode, network mode or other decoder state.
2. Leave the page on LIVE AUDIO MONITOR for 5–10 seconds. The monitor should begin publishing metrics after roughly one second. For a live signal, the best case is a callback-derived rate near 4,000 Hz, a mean 24-sample block interval near 6,000 µs, and zero queue drops, gaps and clipping. ADC min/max, DC mean and AC RMS depend on the actual GPIO11 input signal; a quiet input can be nearly flat.
3. Short-click the encoder once. The display should change to TEST PLAYBACK: 8-FSK and identify the source as SYNTH / NOMINAL 4 kHz. Watch the metrics during the approximately 12.64-second sequence.
4. For the generated sine, expect approximately: rate 4,000 Hz; block interval 6,000 µs; ADC min/max 848/3248; DC mean 2048; AC RMS 849; DROP=0, GAP=0, CLIP=0. Treat these as indicative, not exact limits; scheduler jitter can change interval min/max and measured RMS slightly.
5. When the sequence ends, the page should return to LIVE AUDIO MONITOR automatically. A second short click should start another run; a long press exits the monitor.

The synthetic sequence is not audible, is not a valid JS8 NORMAL waveform and cannot produce a decoded message. This test only checks the monitor's deterministic 8-FSK block generation, queue/worker path and level metrics. Live GPIO11 measurements are also callback-derived and do not measure individual ADC sample intervals or intra-block jitter. For a real input check, manually tune the receiver to a JS8 HF dial frequency in USB (e.g. 7.078 MHz); the monitor itself does not tune the receiver. No JS8 sync, LDPC, CRC, message decode or sensitivity claim is possible at this stage.

If JS8 audio monitor failed appears, metrics remain invalid, or synthetic metrics are far from the indicative values, stop and report the exact screen/counters before proceeding. Do not interpret a successful monitor test as proof of JS8 decoding. The protocol-valid NORMAL golden vector and decoder sample-consumer interface remain separate prerequisites for the next milestone.

- Lifecycle hardening (commits a9ed0983ed0cbe25f4d5d3a8e2676cbc44bad4bd and 917491b5a01dd728d47cc3c0084a6b008dbefd66): each queued audio block carries a source-generation tag. Source switches invalidate in-flight/stale callbacks so a GPIO11 block cannot be mistaken for synthetic playback or leak into the resumed live stream. This is source-reviewed only; no concurrency stress test or build has been run.

- Follow-up to the source-generation guard: generation reads/increments are now serialized with the monitor's portMUX critical section so the UI stop action and worker auto-completion cannot lose a concurrent increment. Commits b707dc93a9b195a934576154d486655e361cf747 and df014139b7e3311072749fdced86d2e8c6ebbfd9. Still source-reviewed only; compile/runtime validation remains pending.


## 9. JS8 display cleanup and next validation milestone (2026-11-21)

- Replaced the blue diagnostic page with a pure-black, high-contrast text-first layout: white primary text, muted secondary text and restrained green status/counter accents.
- Reduced diagnostics to three compact lines (callback-derived rate/block interval; RMS/ADC range; DROP/GAP/CLIP). The lower portion is reserved for decoded messages and currently shows an explicit waiting state.
- The message area must remain empty until a candidate passes JS8-specific synchronization, LDPC decoding and CRC validation. Never show a fabricated sample message as if it were received.
- The existing short-click signal remains only a generic audio-pipeline test. It must be relabeled/kept distinct from a protocol-valid JS8 test; it does not yet feed a JS8 decoder.
- Next milestone is a protocol-valid, independently generated JS8 NORMAL golden frame with a fixed known payload and exact metadata (79 symbols, sync positions, tone sequence, 4 kHz PCM, symbol/frame timing, expected 75-bit payload and CRC result). Validate the fixture independently on a host before adding it to firmware.
- After the golden vector is validated, implement the JS8-only sample consumer and NORMAL sync/soft-decision/LDPC/CRC path. Add a clearly labeled on-device self-test that routes the known vector through the same decoder path and displays the expected text only when the decoder actually recovers it.
- Do not call the generic 8-FSK monitor stimulus a JS8 decode test. Do not copy GPLv3 JS8Call source/tables into this MIT repository. Do not run builds/Actions; user handles manual builds.
- UI-only change commit: 959e58308b4982fa96e3a754c7bc4f0249b55597. Source-only; not compiled or hardware-tested.


## 10. MIT clean-room implementation decision (2026-11-21)

The project owner confirmed that the repository must remain MIT and that the JS8 implementation must be developed independently. This excludes copying JS8Call GPL source, porting its implementation, or importing parity-check/generator/permutation tables copied or mechanically extracted from that source.

A fresh protocol-documentation review confirms the public NORMAL frame dimensions and the existence of the JS8 LDPC(174,87) channel code. The current public user guide explicitly says implementation details remain subject to change and points to source as the implementation truth; it does not publish the complete LDPC construction data. A third-party clean-room write-up was located, but it explicitly says its numeric LDPC tables were machine-generated from the JS8Call reference source. Those tables are therefore not an acceptable independent input under this project's agreed boundary and will not be copied.

Important interoperability constraint: generating a different PEG matrix with the same dimensions/rate would create a different code and would not decode ordinary JS8Call transmissions. We must not substitute an arbitrary LDPC(174,87) matrix or label a self-consistent private test as JS8Call-compatible.

Implementation path:

1. Keep the exact JS8 protocol facts sourced to public documentation (NORMAL symbol count/rate/tone spacing, sync placement, CRC definition and payload structure) and record provenance per item.
2. Develop the receiver's audio front end independently: 4 kHz sample windowing, candidate frequency/time search, NORMAL Costas correlation, eight-tone soft metrics and explicit rejection of discontinuous audio. These stages can be validated with a synthetic sync-plus-data waveform without claiming a valid encoded JS8 message.
3. Keep LDPC integration blocked until a legally independent source for the exact interoperable code construction is established, or until a formally documented licensing exception is approved. Do not invent a substitute matrix.
4. Once the exact code construction is lawfully established, create independently generated bit-level golden vectors and verify encoder/decoder round trips plus cross-implementation interoperability before exposing decoded text.

This is a deliberate quality gate, not a reduction in the target: the goal remains real JS8Call-compatible receive decoding. No firmware build, GitHub Actions run, flash or hardware test was started as part of this research.

References reviewed:
- JS8Call User Guide / current improved guide: https://js8call.com/JS8Call-improved/d6/d14/md_docs_2user__guide_2JS8Call__User__Guide.html
- JS8Call source was consulted only to understand the provenance boundary; no source or tables were copied into the repository.
- Third-party technical write-up was rejected as a table source because it states that its LDPC tables were generated from the JS8Call reference source: https://patrick.lidstone.net/js8-native-keyboard-keyboard-weak-signal-messaging-mode/


## 11. Independent NORMAL Costas pilot metric — first DSP code (2026-11-21)

Added two standalone source files under `ats-mini/src/js8/`:
- `Js8NormalSync.h`
- `Js8NormalSync.cpp`

The new `Js8NormalSync::evaluate()` function evaluates one caller-proposed NORMAL frame start and one caller-proposed lowest-tone frequency/common frequency offset. It uses a Goertzel energy measurement over each 640-sample symbol, removes each symbol's DC mean, and evaluates the 21 known NORMAL pilot symbols at positions 0–6, 36–42 and 72–78. The documented NORMAL Costas sequence is repeated in each block. It reports mean expected-tone/best-tone ratio, mean expected-tone/total-tone ratio, minimum pilot ratio, mean expected pilot energy and a count of pilots whose expected tone is at least half the strongest measured tone.

Scope limits are explicit in the API: this is a pilot metric only. It does not search candidate start times or frequencies, demodulate the 58 data symbols, generate soft bits, decode LDPC, validate CRC or produce message text. It must not be used alone as a valid-frame acceptance decision. The public guide supports the 79-symbol NORMAL frame, 6.25 baud / 6.25 Hz spacing and repeated 7×7 Costas synchronization description; the numeric NORMAL Costas sequence was cross-checked against public technical documentation. No JS8Call source or table was copied.

A separate numerical sanity check generated a 4 kHz, 79-symbol continuous-phase 8-FSK signal with the NORMAL pilot tones and arbitrary data tones. The independent Goertzel metric returned 21/21 pilot matches and 1.0 mean expected-to-best ratio in the clean, exactly aligned, zero-offset case. This validates only the mathematical metric in an ideal synthetic case; it is not a firmware compile, protocol-valid message vector, noisy-signal sensitivity test or JS8Call interoperability test.

Next engineering gates:
1. Add host-side C++ tests for start-time offset, common frequency offset, gain variation, additive noise and wrong Costas/noise-only rejection; determine thresholds empirically rather than hard-coding an optimistic threshold.
2. Add bounded frame-window ownership to the JS8 worker. The existing 4,096-sample diagnostic snapshot is only about 1.024 seconds and cannot hold a 12.64-second NORMAL waveform (50,560 samples, about 101 kB at int16); do not try to decode from that short ring.
3. Implement coarse candidate search around proposed frame start/frequency, with CPU-budget measurements before any live scanning integration.
4. Keep LDPC/CRC and message output blocked until the exact interoperable LDPC construction is available from a legally independent source.

No Makefile changes are required because the existing wildcard includes `src/js8/*.h/.cpp`. No build, GitHub Actions run, flash or hardware test was started. Source has not been compiler-verified on the ESP32 toolchain.


## 12. Host-side test harness for NORMAL Costas metric (2026-11-21)

Added `tests/js8_normal_sync_test.cpp`, a standalone host test that compiles directly with `ats-mini/src/js8/Js8NormalSync.cpp`. It covers:
- Correct NORMAL pilot sequence with a 17-sample frame-start offset and +1.25 Hz common carrier offset (provided as the test hypothesis); all 21 pilots must match.
- Deliberately altered Costas pilot sequence must be rejected by the metric.
- An incomplete 12.64-second frame window must be rejected.

Executed locally with:

```sh
g++ -std=c++17 -Wall -Wextra -Werror \\
  ats-mini/src/js8/Js8NormalSync.cpp tests/js8_normal_sync_test.cpp \\
  -o /tmp/js8_normal_sync_test -lm
/tmp/js8_normal_sync_test
```

Observed result: `PASS: NORMAL Costas metric offset, pilot count, wrong sequence, bounds`.

This is a host compile/test of the standalone metric, not an ESP32 firmware compile. The test fixture has a JS8-shaped NORMAL pilot layout and arbitrary non-pilot data tones; it is not a valid LDPC/CRC-encoded JS8 message. The current metric also evaluates a caller-proposed frame start/frequency; no automatic time/frequency search or receive-window integration exists yet. The build workflow will be configured to run on pushes to `development` as well as manual dispatch, so the next development push can produce the OSPI-DE firmware artifact. It will not create a release for ordinary development-branch pushes.


## 13. Local NORMAL hardware self-test
The audio monitor now retains a complete 79-symbol NORMAL frame (50,560 samples at 4 kHz) and generates the three NORMAL Costas pilot blocks in its internal test waveform. It evaluates that known local waveform at the known frame start and nominal tone frequency; this is a deterministic self-test, not a blind acquisition search. The UI fixture text is labelled **TEST PAYLOAD (LOCAL ONLY)** and is not decoded from the waveform. PSRAM is required for the full-frame ring. LDPC/CRC and live-air text decoding remain unimplemented.


## 13. Local NORMAL hardware self-test
The audio monitor now retains a complete 79-symbol NORMAL frame (50,560 samples at 4 kHz) and generates the three NORMAL Costas pilot blocks in its internal test waveform. It evaluates that known local waveform at the known frame start and nominal tone frequency; this is a deterministic self-test, not a blind acquisition search. The UI fixture text is labelled **TEST PAYLOAD (LOCAL ONLY)** and is not decoded from the waveform. PSRAM is required for the full-frame ring. LDPC/CRC and live-air text decoding remain unimplemented.

## 2026-11-24 milestone — GPL LDPC core added

The owner explicitly approved the JS8Call GPLv3 path and changed the repository to private for development. This supersedes the earlier clean-room-only restriction recorded above. The repository's other files keep their existing notices; do not assume that a future statically linked firmware image is MIT-only. Before conveying firmware/source externally, review the combined-work implications and comply with applicable GPLv3 source and installation-information requirements.

### Upstream reference

- Project: `js8call/js8call`
- Source revision inspected: `a7ff1be0b389d287fdc56e2ea0d06962aa68127d`
- Source file: `JS8.cpp`
- License text copied to `ats-mini/src/js8/COPYING`; provenance/distribution notes in `ats-mini/src/js8/NOTICE.md`.

### Added files

- `ats-mini/src/js8/Js8Ldpc.h`
- `ats-mini/src/js8/Js8Ldpc.cpp`
- `tests/js8_ldpc_test.cpp`

The isolated module implements the JS8Call LDPC(174,87) parity graph and generator matrix, belief-propagation decoding, JS8 75-bit payload layout, augmented CRC-12 (polynomial 0xC06, XOR 42), and 12-character 6-bit alphabet unpacking. The codeword ordering is 87 parity bits followed by 87 message bits, matching the upstream JS8 encoder/decoder layout. Decoder input is 174 soft LLR values; positive values favor bit 1.

The caller-owned BP workspace is approximately 8 KiB. It must not be placed on the small audio/ADC task stack; later service integration must allocate/own it with an appropriate memory strategy (PSRAM preferred where practical). Decoder work must remain on the worker, never in the ADC callback.

### Current limitations / validation

- This is only the channel-code and payload layer. It is not yet connected to the 8-FSK symbol demodulator, timing/frequency search, or live AudioCaptureService.
- The host regression source covers payload CRC, a noiseless encoded frame, a single erased LLR, payload unpacking, and CRC rejection after corruption.
- The host test has **not yet been executed**; the firmware has **not been built** after these commits.
- Build #84 passed on the earlier commit `b84d00b`; it does not validate the new files.
- No FT8/FT4 code or DSP path was changed. Preserve the existing FT8/FT4 decoder exactly.

### Next steps

1. Run/validate the host regression test before relying on the LDPC core.
2. Cross-check generated parity/payload vectors against JS8Call reference vectors, not just a self-round-trip.
3. Implement NORMAL 8-FSK soft demodulation for the 58 data symbols, mapping their three-bit tone metrics into the 174 LLRs in upstream order.
4. Feed the LLRs into this isolated decoder from the JS8 worker and require both LDPC convergence and CRC success before emitting a message.
5. Measure CPU time and memory on ESP32-S3 before enabling live decoding.

## Follow-up — NORMAL 8-FSK soft demodulator added

Added `ats-mini/src/js8/Js8NormalDemod.h/.cpp`. For a caller-proposed frame start and lowest-tone frequency, it:
- evaluates the 29 parity and 29 message data symbols (symbol indices 7–35 and 43–71);
- computes Goertzel energy for all eight tones per symbol;
- converts each 3-bit tone metric into three max-log LLRs;
- outputs 174 LLRs in the JS8Call order: parity bits 0–86, then message bits 87–173.

Added `tests/js8_normal_demod_test.cpp`, which synthesizes all 79 NORMAL symbols from the JS8 encoder output, checks the 21 Costas pilots, demodulates the 58 data symbols, runs LDPC/CRC, and checks the decoded 12-character payload and frame type.

### Host validation commands (not yet run)

From the repository root:

```sh
g++ -std=c++17 -O2 tests/js8_ldpc_test.cpp ats-mini/src/js8/Js8Ldpc.cpp -lm -o /tmp/js8_ldpc_test
/tmp/js8_ldpc_test

g++ -std=c++17 -O2 tests/js8_normal_demod_test.cpp \
  ats-mini/src/js8/Js8Ldpc.cpp \
  ats-mini/src/js8/Js8NormalDemod.cpp \
  ats-mini/src/js8/Js8NormalSync.cpp -lm -o /tmp/js8_normal_demod_test
/tmp/js8_normal_demod_test
```

The custom bitwise CRC routine was separately compared with Boost's `augmented_crc<12, 0xc06>` on a sample byte sequence and matched. The full host regression tests above have **not** been executed. The firmware has **not** been built after these commits.

### Still missing

- Timing acquisition and frequency search; both demodulator layers currently require caller-proposed start/frequency.
- A rolling 15-second frame buffer and JS8 NORMAL slot/frame acquisition.
- Integration into a dedicated worker using the existing shared AudioCaptureService (no second ADC sampler; no DSP in callback).
- Live message UI, false-positive rejection under noise/interference, CPU and PSRAM measurement on ESP32-S3.
- Independent JS8Call reference-vector cross-check; the current synthetic end-to-end test shares the imported encoder matrix and is not an external interoperability proof.


## 2026-11-24 — bounded JS8 NORMAL acquisition candidate search

- Added `Js8NormalAcquire.h/.cpp`: bounded coarse search over caller-specified frame-start and common-frequency-offset ranges. Search is allocation-free and capped by `maxCandidates`; it rejects tone grids outside the 4 kHz Nyquist band.
- Candidate ranking uses the 21 NORMAL Costas pilots. The result is explicitly only a synchronization candidate; it is not a valid-frame lock and must still pass the existing LDPC decoder and JS8 CRC before any text is emitted.
- Added `tests/js8_normal_acquire_test.cpp`. Host test compiled with `g++ -std=c++17 -O2 -Wall -Wextra -Werror` and passed: synthetic NORMAL pilot frame found with a 17-sample start displacement and +1.25 Hz offset; silence rejected; candidate budget respected.
- This is a coarse bounded search primitive, not yet a full 15-second rolling-slot scanner. Search bounds/step sizes and thresholds are provisional; CPU cost, noise/interference false positives, timing refinement, and real JS8Call interoperability remain to be measured.
- No firmware build, GitHub Actions run, hardware test, live AudioCaptureService integration, or full JS8 message decode was performed. FT8/FT4 and LILYGO GPIO11 guard remain untouched.
- Licensing state: JS8 LDPC/payload component is adapted from GPL-3.0 JS8Call source and includes its license/provenance files. The repository remains private by user decision. Private visibility does not remove GPL obligations if the combined work is later conveyed/distributed; review those obligations before any external distribution.


## 2026-11-24 — bounded NORMAL timing/frequency refinement

- Extended `Js8NormalAcquire` with a local refinement pass around a coarse candidate. The caller controls the time radius/step, frequency radius/step, and strict candidate budget.
- Default refinement scans a ±40-sample window in 2-sample steps and a ±1.25 Hz window in 0.625 Hz steps, with a hard cap of 256 candidate evaluations. This is a bounded local search, not a full 15-second slot scanner.
- A candidate must meet the configured NORMAL Costas-pilot match thresholds. This still does **not** establish a valid JS8 frame: only successful LDPC convergence plus JS8 CRC validation may authorize decoded text or a true receive lock.
- Added `tests/js8_normal_refine_test.cpp`, using a synthetic frame beginning at sample 17 with a +1.25 Hz carrier offset; it checks sample-level timing refinement, frequency refinement, candidate cap, and silence rejection.
- Test source has been added but **not executed**. No firmware build, GitHub Actions run, hardware test, or live audio integration was performed. FT8/FT4 and the LILYGO GPIO11/SPI guard remain untouched.
- Next: execute the acquisition/refinement host tests, then design a rolling 15-second NORMAL frame scanner and validate it with independent JS8Call reference recordings before live worker integration.
