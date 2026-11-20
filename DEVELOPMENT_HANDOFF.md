# ATS Mini Development Handoff

Last updated: 2026-10-30 (V1.31 release preparation; Web ALL Band scanner range validation and click-to-tune behavior documented)
Repository: https://github.com/13HN2709/ats-mini-dev (private development workspace); public release repository: https://github.com/13HN2709/ats-mini
Active work branch: `development` (never modify `main` unless explicitly requested)

## Working agreement
- Keep this file current after every meaningful feature or code change. Use it as the handoff reference when starting a new chat.
- New features must be isolated from existing features wherever practical. If implementing a feature would require changing shared feature-specific logic, duplicate that logic into a new module/state/handler for the new feature rather than refactoring the established feature.
- Preserve Dual RX, existing Web Spectrum behavior, Clean Layout and especially Layout-TEF6686 unless a change is explicitly required.
- Shared low-level radio/scanning primitives may be reused only after checking their side effects and compatibility.
- Build workflow is manual (`workflow_dispatch`). Do not claim a build/device test passed unless verified. User launches build and tests on ATS Mini.
- Keep branch hygiene simple; development first. User manages copying approved README/release changes to main unless explicitly asked.

## Current task: native VHF Spectrum Scanner
### Intended user experience
1. Main menu has XXL `SPECTRUM` category.
2. Spectrum submenu has `VHF Spectrum` and `ALL Band Spectrum`. The all-band item is a future implementation and must not accidentally start the VHF scan.
3. VHF scanner asks for start frequency and end frequency sequentially with its own keypad/input flow.
4. Valid range: 88.00–108.00 MHz, end frequency strictly greater than start.
5. Invalid input shows an error and does not start scanning or change the receiver.
6. Show a visible “SCANNING SPECTRUM” status/range before the sweep.
7. Show graph; encoder rotation selects a point, short click accepts the selected frequency and exits.
8. Cancel exits cleanly and should not tune/change the receiver as a side effect.
9. Match the dark-blue/cyan/green TEF6686 visual style.

### Isolation requirements
- Do not alter Dual RX's `dualRxSetupStage`, keypad functions, or click behavior to implement the scanner.
- Do not reuse `CMD_FREQ_KEYPAD` for the scanner. Add a dedicated scanner-input command/state and separate keypad handling/drawing, ideally in scanner-specific module(s).
- Existing `FreqKeypad.cpp/.h` is used by radio tuning and Dual RX. Treat it as protected.
- Existing web scan engine is in `Scan.cpp`; the native VHF scanner now uses a separate private sample buffer and sweep implementation in `SpectrumScanner.cpp` to avoid overwriting web scan results.
- `Network.cpp` should not be changed unnecessarily; existing web-spectrum functionality must remain intact.
- Scanner code must explicitly prevent the `ALL Band Spectrum` menu item from invoking the VHF scan until all-band support is implemented.


### New UI requirements confirmed on 2026-10-28
- VHF Spectrum keypad must visually match the existing native frequency keypad: same compact black panel, blue double frame, frequency field, fixed-position FM indicator, 4x4 key geometry, cyan selection state, and font treatment.
- VHF keypad is FM-only and MHz-only. No selectable modulation control and no kHz toggle. The FM indicator and MHz key are fixed informational/entry labels, not mode/unit switches.
- Future ALL Band Spectrum keypad must be a separate mode: offer only USB, LSB, AM (no FM) and allow kHz entry only (no MHz). Do not implement or expose these options on the VHF keypad.
- The native VHF keypad drawing function was updated in commit `4414b3ccb32d93a313916a2ec1237cfa47232afe` to use the existing keypad's geometry/colors/fonts and fixed FM/MHz labels. This is source-only; no build or device test has been run.
- The VHF keypad close button label was aligned with the existing keypad (`CLOSE`) in commit `9a4d9c2fbda1796ed48e7e301c9c2a9b843aafd5`.
- Native scanner tuning finds an FM band that covers the requested range, configures FM before the sweep, and restores the previously selected band/frequency/mode/BFO after scanning. Accepting a graph point explicitly switches to the FM band covering that frequency. Graph long-press exits without tuning. Commits: `827091d16d4f46625f5ad989e7a3ebb3bccc5305`, `42f34a90788a0c6f5d971ad76174f29323873381`, `0cae2833afec6acf20bb44e3740996af079a11b1`, `d7b79986eeab993828456faf633750e4769cf07b`.
- Native VHF Spectrum now has its own 201-point RSSI/SNR sample buffer and isolated sweep loop in `SpectrumScanner.cpp`; it no longer uses or overwrites the Web Spectrum's `Scan.cpp` sample buffer. The native loop keeps its own data and tunes the shared physical receiver only while scanning. Commits: `ff0ed62319b51e75ccf2e509808491abd9216a2a`, `5e55a3ae099c768afdd9b153ab547a0d9c6a3fe3`, `dca1574dd612a8f95cd402ade25287dad77bb0a4`, `bc350911571369583d39e77ef5d3e67bf9f2bd8`, `07bac5f4b21d4ca70bdb6e62a74071758360e425`.

## Current repository state observed on 2026-10-28
- Branch: `development`.
- `ats-mini/SpectrumScanner.cpp/.h` now contain scanner-owned input state and a dedicated numeric keypad for start/end frequencies; the existing `FreqKeypad.cpp/.h` is not modified.
- Native scan sample storage/sweep is now isolated from Web Spectrum; only the physical SI4735 receiver is necessarily shared.
- `CMD_SPECTRUM_INPUT` is a separate command. `Draw.cpp` renders its own input screen and `ats-mini.ino` dispatches its own encoder input, outside the Dual RX block.
- The VHF input accepts 88.00–108.00 MHz, accepts dot or comma decimals up to two decimal places, and rejects an end frequency that is not greater than the start. The scan graph no longer tunes on encoder rotation; a short click tunes only the accepted point, while long press exits without tuning.
- `Menu_german.cpp.src` localizes the Spectrum submenu. The all-band placeholder is guarded so it does not start VHF scan.
- `Network.cpp` ignores pending web spectrum commands while native spectrum input is active.
- Source review completed on 2026-10-28: delimiter-balance check passed, no calls remain to the shared web-scan APIs in `SpectrumScanner.cpp`, and the workflow remains `workflow_dispatch` only. The first manual build (run 40781905177, job `build ospi-de`) failed because `SpectrumScanner.cpp` called `muteOn(MUTE_TEMP, ...)` without including `Utils.h`, where both are declared. Fixed by adding `#include "Utils.h"` and clarified the keypad input condition's operator parentheses in commit `db8c1f7fe7ab23911d7a65ea53b779ebb893f8c3`. A new build is still required; no successful compile or hardware test is confirmed.
- `Menu.h` defines `CMD_SPECTRUM` and `CMD_SPECTRUM_MENU`.
- English/German menu templates include Spectrum and submenu entries; the click handler currently starts the VHF scanner only for submenu index 0, so index 1 must be handled safely.
- `Draw.cpp` routes `CMD_SPECTRUM_MENU` to the full-screen menu and `CMD_SPECTRUM` to `drawSpectrumScanner()`.
- `ats-mini.ino` has dedicated encoder/click handling for `CMD_SPECTRUM_INPUT` and `CMD_SPECTRUM`; it stays outside the Dual RX block. Short click accepts a graph point; long press exits without tuning.
- `FreqKeypad.cpp/.h` implements the existing radio/Dual RX keypad and must remain unchanged for this feature.
- Native VHF scanning no longer calls `Scan.cpp`; the web scanner's existing global result buffer is untouched.
- GitHub Actions workflow was changed earlier to `ubuntu-24.04` for both jobs after an Ubuntu runner/Python issue. Do not revert without a reason.

## Immediate next steps
1. User launches the manual GitHub Actions workflow on `development` and checks all four OSPI/QSPI × German/English builds.
2. After the fix commit `db8c1f7fe7ab23911d7a65ea53b779ebb893f8c3`, rerun the manual workflow on `development`. If another build fails, use the exact compiler output to fix only the affected code and update this handoff.
3. After a successful build, flash/test the ATS Mini V4: start/end entry, invalid range, cancel without retune, full sweep/status, graph navigation, short-click tune, long-press exit, and confirm Web Spectrum + Dual RX still work.
4. Do not merge/copy to `main` unless the user explicitly asks.

## Validation checklist (must be checked before asking user to build)
- [x] VHF start and end are entered in separate scanner-owned stages (source reviewed).
- [x] Dot/comma decimal parsing works up to two decimal places (source reviewed).
- [x] Range and ordering validation reject invalid values before radio changes (source reviewed).
- [x] Input cancel/validation errors do not retune; scan restores prior state before graph navigation (source reviewed).
- [x] Encoder navigation and short-click selection are wired to scanner-specific handlers (source reviewed; hardware test pending).
- [x] Long press cancels keypad or exits graph without tuning; Dual RX handling block remains unchanged (source reviewed).
- [x] ALL Band submenu item cannot start VHF scanning (source reviewed).
- [x] Existing Dual RX-specific keypad and state logic were not modified for the scanner (source review; hardware regression test pending).
- [x] Native scanner no longer shares Web Spectrum sample buffer (source reviewed; hardware regression test pending).
- [ ] Native sweep tune-complete polling and timeout behavior reviewed against existing scan loop.
- [x] First build failure fixed at source level: `Utils.h` is now included for `muteOn` and `MUTE_TEMP`; rebuild pending.
- [x] Native sweep configures a covering FM band and restores previous band/frequency/mode/BFO after scanning (source reviewed; hardware test pending).
- [x] German and English menu templates have VHF/All Band labels and guarded click handling (source reviewed).
- [x] VHF keypad uses matching geometry/colors/fonts and `CLOSE` label (visual confirmation requires device test).
- [x] VHF keypad exposes only fixed FM and MHz; no mode or kHz switching (source reviewed).
- [x] Future ALL Band requirement is documented; implementation is intentionally not present yet.
- [x] Build workflow uses `workflow_dispatch`; no workflow was triggered by the assistant.
- [x] Handoff updated with actual changes and remaining build/device validation.

## Changes made in this session
- Added `DEVELOPMENT_HANDOFF.md` on `development` (initial handoff commit `bee78177301f4d2be6759f40f5de46ed0c55522f`).
- Added `CMD_SPECTRUM_INPUT` and scanner-specific keypad API/state.
- Added independent VHF start/end frequency entry and basic range validation in `SpectrumScanner.cpp`.
- Added keypad drawing dispatch in `Draw.cpp`; added encoder input dispatch in `ats-mini.ino` without changing the Dual RX block itself.
- Updated English/German menu click handling so only VHF item starts scanner; German submenu localized.
- Updated `Network.cpp` to ignore web-spectrum command processing during native scanner input.
- Additional commits: `b1b657d3355526920328069a31735eeab1babfa3` (corrected label line breaks), `1c6861a7427736b7670afbe82a870d7892e174f1` (web-spectrum command isolation), `7d5d86e1a54f7db04ad28709a180c4ec0fac5c5d` (consume menu-opening click so it cannot enter a keypad digit). The handoff itself was updated in `90effb849c89d8b4c867808c5aaf6e13a572ebbd` before these final edits.
- Still required before user build: finish the source-level review of the isolated native sweep and all encoder/UI state transitions. No compile or hardware test has been run.


## Build readiness
- First manual build attempt failed in `build ospi-de` on undeclared `muteOn`/`MUTE_TEMP`; the fix is committed as `db8c1f7fe7ab23911d7a65ea53b779ebb893f8c3` (added `Utils.h`, clarified a warning-prone condition). Rerun the manual workflow on `development` before flashing.
- Ready for the user to click **Run workflow** on the `development` branch.
- The workflow matrix builds OSPI/QSPI firmware in both German and English (four builds).
- This is a source-level review only. Do not describe the build as passing until GitHub Actions completes successfully.
- Current relevant source commit: `07bac5f4b21d4ca70bdb6e62a74071758360e425`; latest handoff updates continue after it.


## Future layout reference: GNBD miniradio (requested 2026-10-28)
- User wants to recreate the screenshot's receiver layout as close to 1:1 as possible in the ATS Mini V4 firmware, using the reference source at https://github.com/GNBD/ats-mini-dualboot/tree/main/GNBD%20miniradio.
- The supplied screenshot is the visual target: dark near-black background, compact top status bar with time/signal/battery, VHF FM heading, large frequency digits, RSSI/SNR horizontal meters, right-side compact receiver settings, large RDS panel at bottom-left, and small SIG graph panel at bottom-right.
- Specific adjustment requested: raise the “MHz” unit slightly higher relative to the frequency digits.
- This is a future layout task, not part of the current VHF Spectrum Scanner build fix. Before implementing, inspect the reference repository's actual source files and map the target drawing to the user's current layout architecture. Keep Layout-TEF6686.cpp and Clean Layout protected unless the user chooses the destination layout.


## Layout default change (requested 2026-10-28)
- User wants Layout-TEF6686 to be the standard/default layout, including on a fresh installation.
- In both English and German menu source templates, `uiLayoutIdx` now initializes to `5` (TEF6686), and the layout formerly labeled `Default` is now labeled `Original`.
- In `Draw.cpp`, the invalid/unrecognized layout-index fallback now draws `Layout-TEF6686` rather than `Layout-Default`.
- Commits: `8eca92ce3358c38ebf60950a06836a1de3a746e8` (English default/label), `0adefb7ac7154f7bd627de7d6c562d472e45dc80` (German default/label), `ffb2b1cc284564090e306f0419591db3afabc195` (render fallback).
- Existing devices with a saved `UILayout` preference will keep that saved choice; fresh installs/reset preferences should initialize to TEF6686. Build and hardware confirmation pending.


## VHF scanner keypad hang fix (2026-10-29)
- User reported the ATS Mini V4 hung while rotating the native VHF Spectrum keypad from the MHz key toward CLOSE (photo showed the input screen stuck at `--- MHz`).
- Root cause found in `spectrumScannerInputRotate()`: when the next key index was the intentionally blank slot 14, a `do/while(next == 14)` loop never changed `next`, causing an infinite loop and a hard UI freeze.
- Fixed on `development` in commit `e35a2081c69861e71b284a4193f7ef7201ced944`: advance one additional step when landing on slot 14, with wrap-around handling. This directly addresses the freeze path without touching Web Spectrum, Dual RX, or existing layouts.
- Not yet compiled or device-tested. Rerun the manual workflow on `development`; then test rotating in both directions across `.`, MHz, blank slot and CLOSE, plus OK/BS and cancel behavior.


## Future layout reference: lsongdev/tinyradio (requested 2026-10-29)
- User wants the additional display layout shown near the bottom of https://github.com/lsongdev/tinyradio added to the project's layout TODO list. User jokingly notes they are a layout fan.
- The repository is an AM/FM/SSB SI4732 receiver firmware project for ATS Mini and GeekNest V5A. Inspect the specific screenshot/image near the bottom of the linked repository page and its corresponding implementation before planning a port.
- Treat this as a separate future layout option, not a replacement for the TEF6686 default layout or the previously requested GNBD miniradio-inspired layout. Preserve existing layouts and isolate the implementation where possible.
- Priority: after current VHF Spectrum scanner build and menu/input bugs are resolved. Confirm which exact bottom-of-page layout the user means if the repository page contains multiple candidates.


## VHF Spectrum scanner device-test findings and patch (2026-10-29)
- User confirmed on ATS Mini V4 that native VHF scanning completes and displays a graph for 88.00–108.00 MHz, and selecting a point tunes the chosen frequency on the main layout correctly.
- Encoder keypad hang is no longer observed in the user's current build.
- User requested: (1) Spectrum submenu must appear on the first click, with VHF Spectrum and ALL Band Spectrum plus start-frequency guidance; (2) after entering start frequency, clearly explain that end frequency is next; (3) dark Web Control Spectrum style, subdued waveform and vivid red peak tips; (4) encoder moves from peak to peak rather than every 100 kHz.
- Patch adds start/end guidance to the scanner keypad, submenu rendering and a menu hint in both languages, and excludes CMD_SPECTRUM_MENU from the generic command timeout. It darkens the graph, marks local maxima in vivid red and changes encoder navigation to select local peaks, falling back to adjacent points if no peaks are detected.
- Source-only change; manual GitHub Actions build and device regression tests are required. Verify first-click submenu behavior, input-stage instructions, graph contrast, peak-to-peak selection, and existing Web Spectrum/Dual RX behavior.


## Follow-up fix after VHF scanner test (2026-10-29)
- User confirmed the scanner graph runs and clicking a frequency tunes the main layout correctly. The encoder keypad hang is gone.
- Native VHF scanner source already includes start/end-frequency instructions, dark graph styling, vivid red local-peak markers, and peak-to-peak encoder navigation with adjacent-point fallback.
- Found why the Spectrum submenu may disappear: the generic command timeout exempted the graph and keypad but not `CMD_SPECTRUM_MENU`. The timeout now also exempts the submenu so it stays open while the user reads/selects VHF Spectrum or ALL Band Spectrum.
- Menu hint is present in both English and German. The start/end keypad text is present.
- Next: manual build and device test. Verify first click on SPECTRUM opens and stays on the submenu; selecting VHF Spectrum opens the keypad; entering start frequency prompts for end frequency; graph colors/peak navigation; selecting a peak still tunes correctly. Source changes are not build-verified yet.


## VHF Spectrum UI polish before next manual build (2026-10-29)
- Commit `782a1f3985bd3ddcee04b1e262bcb7a3af7b63d2` adjusts the scanner keypad geometry so the instruction text is no longer drawn over the frequency field. The first stage explicitly says ENTER START FREQUENCY; the second stage shows the accepted start frequency and ENTER END FREQUENCY above the field.
- Native graph panel and grid were darkened toward the near-black navy Web Spectrum aesthetic; trace color is a very dark blue-gray and peak tips remain vivid red.
- Encoder peak navigation now uses a lower local-peak threshold (5% of the RSSI span) and, if the trace has no qualifying local peak, jumps to the strongest remaining sample in the requested direction instead of advancing by a single 100 kHz point.
- The earlier timeout exemption for `CMD_SPECTRUM_MENU` remains in `ats-mini.ino` (commit `29f2865d193b3ef3ffb2132607c069cc24964414`).
- Next step: manual build and device verification. These latest changes have not been built or tested on hardware yet.


## VHF Spectrum keypad click-dispatch regression (2026-10-29)
- User clarified that before the Spectrum submenu was introduced, selecting Spectrum opened the keypad immediately; confirming the start frequency opened the end-frequency keypad; confirming the end frequency ran the scan successfully. The current regression appeared after the submenu and follow-up UI changes.
- Root cause found in `ats-mini/ats-mini.ino`: a click while `currentCmd == CMD_SPECTRUM_INPUT` was first sent to the generic `clickHandler()`. Since that handler has no `CMD_SPECTRUM_INPUT` case, it returned false and the generic fallback changed `currentCmd` to `CMD_NONE` before the dedicated `spectrumScannerInputClick(true)` block could run. This breaks keypad confirmation/state progression and can make the keypad appear frozen or exit unexpectedly.
- Targeted fix committed on `development` as `b37791abdb8b28d9ab83d24df9493fac8f182e56`: exclude `CMD_SPECTRUM_INPUT` clicks from the generic click handler so the existing dedicated scanner input handler receives them. No Spectrum scanning algorithm, menu source, layout source, or Dual RX code was changed.
- Verification: source-level root cause confirmed and targeted edit committed. Manual build and hardware test are still required; do not claim compile/device success yet. Test menu -> VHF Spectrum -> enter/confirm start frequency -> enter/confirm end frequency -> scan; also verify Back/Close and that normal radio keypad and Dual RX still behave as before.


## Restore direct VHF Spectrum menu flow (2026-10-29)
- User requested reverting the new Spectrum submenu flow because the encoder confirmation still crashed/hung. The main menu now labels the item `VHF SPEKTRUM` / `VHF SPECTRUM` and selecting it opens the existing native scanner start-frequency keypad directly, restoring the pre-submenu interaction sequence.
- The main menu shows a short hint that the start frequency is required after clicking. Closing/canceling the keypad returns to the main menu instead of the removed submenu.
- The spectrum graph, scanning logic, Web Spectrum, Dual RX and radio layout files are unchanged. The legacy submenu code remains unreachable as a conservative minimal change; ALL Band Spectrum is not exposed as a menu choice.
- No build or hardware test has been run for this change yet.

## Apply the three remaining VHF Spectrum requests without restoring the submenu (2026-10-29)
- Kept the direct main-menu flow: selecting `VHF SPEKTRUM` / `VHF SPECTRUM` opens the start-frequency keypad immediately. The submenu remains bypassed and ALL Band Spectrum is not exposed.
- After the start frequency is confirmed, the same dedicated scanner keypad is now explicitly redrawn immediately in its end-frequency stage, rather than relying on a later background refresh or routing through a separate prompt/menu. Commit: `96f1c537b273165f4cdd9a8ff14feef02fe2a430`.
- Preserved the already-implemented Web-Control-inspired graph treatment: near-black/dark-navy background, subdued dark blue-gray waveform/grid, and vivid red peak tips.
- Preserved peak-to-peak encoder navigation, including fallback selection of the strongest remaining sample when no qualifying local peak is found; it does not intentionally step through every 100 kHz sample.
- German and English main-menu hints remain localized. No changes to Dual RX, existing radio keypad, or layout files.
- Source-only update. No build or hardware test has been run; run the manual GitHub Actions workflow on `development` and test start confirmation -> immediate end-frequency keypad, graph appearance, and peak-to-peak navigation.

## Make scanner keypad instructions explicit (2026-10-29)
- User pointed out that the end-frequency instruction should also be visible inside the pop-up keypad, not only in the main menu.
- Start keypad now displays `ENTER START FREQUENCY (MHz)` above the value field. End keypad displays the accepted start frequency on its own line and `ENTER END FREQUENCY (MHz)` on a second line.
- Moved the frequency display down slightly and the key grid down 5 px to make room while keeping the keypad within the screen frame. Commit: `e24c2967d4e278e21750d2093e82f63bfe15ee4c`.
- No build or hardware test has been run; verify both keypad stages visually on the device.


## VHF Spectrum visual fixes and peak-tune follow-up (2026-10-29)
- User tested the current build successfully: start/end frequency keypads now transition correctly and the native VHF graph appears.
- Requested keypad refinement: on the end-frequency keypad, show only `ENTER END FREQUENCY (MHz)`; remove the repeated start-frequency line and use the same bright cyan as the keypad labels. Committed to `development` in `60b9177cf669ef68f2c237d7e911c5e7406561f2`.
- Requested graph refinement: waveform/vertical peak stems should remain subdued dark blue-gray; only the small peak-tip triangles should be vivid red. Changed the vertical stems from red to the subdued waveform color in the same commit.
- Remaining reported behavior: turning the encoder to select a peak updates the frequency shown on the graph, but clicking does not tune to that selected peak and instead retains the frequency that was already shown on the main radio layout. This must be traced through the actual encoder/click dispatch before patching; do not assume `spectrumScannerSelect()` is being called correctly until verified.
- Build/device status: the previous build confirmed the keypad transition and graph on hardware. The latest visual-only patch has not yet been built or tested. Peak-selection tuning bug remains open.


## VHF Spectrum live peak preview (2026-10-29)
- Clarified user intent: rotating the encoder from peak to peak should immediately tune the ATS Mini receiver to that selected peak so the user can listen before deciding to accept it. Encoder click then accepts the current frequency and exits the scanner.
- Added `spectrumTuneSelectedPeak()` in `ats-mini/SpectrumScanner.cpp`; both local-peak navigation and strongest-sample fallback now tune the selected frequency live via the existing FM-band selection and `updateFrequency(freq, false)` path.
- Commit on `development`: `7763294a9c94fc10f51f7e0af476a239202f17c6`.
- Not yet built or tested after this change. Verify encoder rotation audibly tunes each selected peak, and encoder click leaves the selected station in the main layout. Also verify existing cancel/exit behavior because live preview changes the receiver before the final click.


## VHF Spectrum red dot peak markers (2026-10-29)
- Added a small filled vivid-red circle at each detected peak tip, on top of the red triangular marker, so peaks are easier to spot at a glance. Trace and vertical stems remain subdued dark blue-gray; selected peak remains marked in green.
- Commit on `development`: `9ce4dff617820a1e8268246216cdb05502edbbd7`.
- This visual change has not yet been built or checked on hardware.


## VHF trace color and peak clipping (2026-10-29)
- User confirmed the red peak-dot version looks good, but the trace/stems were too dim to see.
- Changed the VHF spectrum trace and peak stems to bright keypad cyan (`0x07FF`). Peak markers are now red circles only; removed the red triangles so only the dots remain red.
- Dot Y coordinate is clamped inside the graph area so strong peaks/markers do not protrude above the graph's top border.
- Commit on `development`: `2a35a96ef48903e76c051dacaff567b6419edcec`.
- New user-reported bug to investigate: while Dual RX is active, a short encoder click should stop the scan, exit Dual RX and return to the layout, but currently Dual RX keeps scanning. Not fixed yet; trace encoder click/short-press dispatch and Dual RX state transitions before changing code.
- This latest visual change has not yet been built or tested on hardware.


## Dual RX encoder stop responsiveness (2026-10-29)
- Traced the dedicated encoder handler in `ats-mini/ats-mini.ino`, `Button.cpp`, and the Dual RX functions in `Menu_german.cpp.src`.
- `dualRxTick()` was called before remote-input merging and before the dedicated Dual RX click handler. It may enter `selectBand()`, which performs DSP patch loading and retuning and can block the main loop. During weak-signal alternation, the 250 ms switching cadence makes this a plausible contributor to missed short clicks; the physical button tracker emits click events on release.
- Minimal source change committed to `development`: moved the Dual RX tick until after the current loop has handled the click/long-press input, and skip starting a band switch while the physical encoder button is held. This gives stop input priority and reduces the chance that a retune blocks an in-progress press.
- Commit: `f8efb9a52e0eea85c63d981bc43a7ac750aeb550` (`Prioritize encoder clicks during Dual RX switching`). Re-fetched the file and confirmed exactly one `dualRxTick(currentTime)` call remains in the new position.
- This is a targeted mitigation, not yet a hardware-confirmed root-cause fix. No build/test run has been executed from this session. Next: run the manual GitHub Actions build on `development`, then on hardware test short-click stop repeatedly during both A RX/B WAIT and A WAIT/B RX; verify long-press dim/wake keeps Dual RX scanning. Do not merge to `main` until confirmed.


## Dual RX encoder stop fix confirmed on hardware (2026-10-29)
- User confirmed the fix works: Dual RX exits with a short encoder click both during active VFO switching and when one VFO is receiving (RX).
- Fix commit: `f8efb9a52e0eea85c63d981bc43a7ac750aeb550`; the change moves `dualRxTick()` after button handling and skips starting a band switch while the physical encoder button is held.
- User's device test confirms the reported stop bug is resolved. Long-press dim/wake behavior was not separately reconfirmed in this message; keep that behavior as a regression check if further Dual RX code changes are made.
- Main remains untouched; do not merge unless the user explicitly asks.


## ALL Band Spectrum implementation (2026-10-29)
- Added a separate `AllBandSpectrum.cpp/.h` module and dedicated commands for ALL Band Spectrum input and graph, without reusing the VHF scanner buffers or keypad state.
- Added `ALL Band Spectrum` as a separate main-menu entry in English and German. Menu hint identifies AM/USB/LSB and the maximum scan width of 1000 kHz.
- Input uses kHz and a dedicated MODE key cycling AM / USB / LSB. Start and end are validated before selecting the broad ALL tuner band or starting a scan. End must exceed start, width must be <= 1000 kHz, and the span must align to 5 kHz steps; larger ranges show a red invalid-input/scan-width hint and do not start scanning.
- Scanner uses its own 201-point RSSI/SNR buffers, 5 kHz steps, and separate graph/input rendering. Web Spectrum and the existing VHF Spectrum source were not edited in this feature.
- Source implementation is committed on `development`; it has **not been compiled or hardware-tested**. Before the manual build, review encoder click routing, tuner mode setup/restore behavior, and verify `AllBandSpectrum.cpp` compiles under all four OSPI/QSPI × German/English matrix targets. Test 2000–3000 kHz accepted and 2000–3100 kHz rejected, plus AM/USB/LSB selection and VHF/Web/Dual RX regression.


### ALL Band Spectrum source review update (2026-10-29)
- Reviewed the native VHF sweep lifecycle and the ALL Band input/graph routing.
- Corrected ALL Band radio restore order so the saved ALL-band modulation is restored before reinitializing the receiver. This matters if the radio was already on the broad ALL band when the scanner starts.
- Added the same abort checks used by the native VHF sweep so an encoder/button/remote abort can stop the blocking sweep, while restoring frequency and mute/delay state.
- Removed duplicate ALL Band click/long-press handler blocks from the main loop; one dedicated handler path remains.
- Reduced keypad button height so the fourth row no longer overlaps the bottom instruction.
- Follow-up commits on `development`: `7581598` (remove duplicate click handlers), `5495957` (restore/abort handling), `c42433a` (keypad spacing).
- Re-check confirmed the GitHub Actions workflow compiles the full `ats-mini` sketch directory, so the new `AllBandSpectrum.cpp` is included in the workflow build even though the legacy Makefile source list is not maintained for this module.
- Final source pass completed: verified dedicated draw/encoder/button paths, command timeout exclusions, English/German menu wiring, and the full-sketch GitHub Actions compilation path. Found and fixed a compile blocker where scanner buffers named `rssi`/`snr` conflicted with globals; the buffers are now `allBandRssi`/`allBandSnr`.
- Current source is ready for the manual build, but it is **not yet compiled or hardware-tested**. Next: user may start the manual GitHub Actions build on `development`; if it succeeds, hardware-test accepted/rejected widths, AM/USB/LSB, radio restore, encoder peak selection/accept, and VHF/Web/Dual RX regression. Do not touch `main`.


### WLAN-Menübezeichnung (2026-10-29)
- In den englischen und deutschen Menüquellen sind die sichtbaren WLAN-Modus-Beschriftungen korrekt zugeordnet: `AP Only` wird als `WEB CTRL` angezeigt; `AP+Connect` wird als `WEB+Conn` angezeigt. Es wurden nur Labels geändert, die WLAN-Logik blieb unverändert.
- Es wurde ausschließlich der Text in `wifiModeDesc[]` geändert; WLAN-Modus-Index und AP-/Connect-Logik bleiben unverändert.
- Noch nicht kompiliert oder am Gerät verifiziert. Auf `development`; `main` bleibt unangetastet.


### ALL Band Spectrum STEP label and hint readability (2026-10-29)
- Renamed the keypad button from `MODE` to `STEP`; its click handler now keys off `STEP` and still cycles the existing scan-step choices.
- The `STEP: n kHz` hint now uses the same cyan color as the other keypad labels and a larger bold font. Validation errors remain red.
- Updated on `development`; not yet rebuilt or hardware-verified. `main` untouched.


### Web Control ALL Band Scanner (2026-10-29)
- Work is being done on `development` only. No change to `main` is intended.
- Added dedicated `AllBandWebScanner.cpp/.h` with its own sample buffer and API; it supports AM/USB/LSB, 150–30000 kHz, configurable step, range validation, and restores the prior radio band/mode/frequency/BFO after the scan.
- Added independent `/all-spectrum/start` and `/all-spectrum.json` endpoints in `Network.cpp`, sharing the radio-operation lock with the existing spectrum scan to prevent simultaneous receiver scans.
- Added an ALL BAND SCANNER panel directly below the existing FM/VHF Spectrum Scan in `PageStatus.h`, matching the existing dark-blue/cyan style and offering clickable signal peaks.
- **Not yet built or hardware-tested.** Next: inspect compile diagnostics, run the manual build workflow on `development`, then hardware-test AM/USB/LSB, radio-state restoration, invalid ranges, clickable peaks, and regressions for VHF scanner/Web Control/Dual RX.
- A temporary branch was created accidentally during setup; user explicitly requested no additional branches. Work was copied onto `development`; remove the temporary branch `all-band-web-scanner` if repository permissions/actions allow. Do not create any further branches and do not touch `main`.


## Current status and release workflow (2026-10-30)

### Repository roles
- **Private development workspace:** https://github.com/13HN2709/ats-mini-dev
- **Public releases:** https://github.com/13HN2709/ats-mini
- Continue development in the private repository's `development` branch. Keep `main` for approved public release snapshots; do not copy changes to public `main` unless the user explicitly approves.
- The private repository currently contains the `development` branch at source snapshot `ace95a45979dfe0b88fd002029aa86c727487d4c`. This is the development snapshot that was synced to public `main` for V1.31 preparation. The public `main` has additional release-only commits: version bump to 1.31 and README feature/release documentation. Do not blindly replace private `development` with public `main`; keep the branch roles clear.
- Preserve existing branches and backups. Do not delete branches or create temporary branches without asking the user first.

### V1.31 release preparation
- Public `main` has `VER_APP 131` in `ats-mini/Common.h`.
- Public README documents the custom extensions in English, Italian and German, including the native VHF/UKW Spectrum Scanner, Web VHF/FM and ALL Band scanners, Waterfall Layout, TEF6686 Layout, Clean Layout, Dual RX, Keypad, VFO A/B, BFO, memory handling and UI/RDS improvements.
- On 2026-10-30, the user started the manual release/build workflow on public `main`. **Result is pending at the time of this handoff update.** Do not claim a successful build or published release until the user confirms the workflow result.
- After a green build, verify the generated artifact, then publish GitHub Release/tag `v1.31`. If the build fails, inspect the exact Actions log and fix only the responsible code before rerunning.
- Keep the public release source and private development work separate. Existing public commits and published assets cannot be made private retroactively.

### Web ALL Band Spectrum scanner — current intended behavior
- The Web ALL Band scanner is a separate module and must remain isolated from the native VHF scanner and Dual RX.
- Frequency range: 150–30000 kHz; maximum scan span: 1000 kHz; maximum 201 sample points. Supported modes: AM, USB and LSB; FM is intentionally not offered in this ALL Band panel.
- Supported step choices include 5, 10, 25, 50, 100, 250, 500 and 1000 kHz. Reject too-small ranges that provide fewer than 5 measurement steps (6 points minimum) and show a clear validation message instead of starting a scan.
- Scan uses its own buffer, blocks conflicting radio controls during the sweep, mutes audio while scanning, and restores radio state afterward. Clickable peaks tune the receiver while the plotted graph remains visible.
- The browser shows a running-scan status and elapsed time while polling for up to 100 seconds; scan work can take roughly 70 seconds for 201 points.
- Start/end fields should be blank by default and cleared after a scan. Range/mode/step changes schedule receiver tuning after a short delay; before scan submission, synchronize the receiver to the selected start frequency/mode. If tuning fails, do not start the scan.
- These behaviors are the latest intended/implemented state from development. Continue to use the user's manual Actions build and hardware confirmation as the source of truth for release readiness.

### Latest user-confirmed test notes
- The user reported that the firmware's overall operation and ATS hardware tests looked very good.
- Native Dual RX short-click stop is confirmed working during switching and receiving states; preserve the fix in `ats-mini/ats-mini.ino`.
- Native VHF Spectrum, Web VHF/FM Spectrum and Web ALL Band Spectrum are intended to remain separate. Avoid modifying shared scanner state or changing `Layout-TEF6686.cpp` unnecessarily.
- When the user reports a new issue, first reproduce the exact state from the relevant source/logs and make a narrowly scoped fix. Never claim a new build or test passed unless the workflow/hardware result is actually available.

### Immediate next actions
1. Wait for the user's public `main` release workflow result.
2. If green, check the artifact and publish `v1.31` only when the user is ready.
3. Continue future feature work in private `ats-mini-dev` → `development`.
4. Keep this handoff current after every meaningful code change, test result or release decision.


## Italian menu language added (2026-10-30)

- Added `ats-mini/Menu_italian.cpp.src` on the private `development` branch.
- Updated `.github/workflows/build.yml` so the build matrix now includes `it` alongside `de` and `en`; the workflow copies the Italian source to `ats-mini/Menu.cpp` for Italian builds.
- Main menu and settings strings are translated to Italian. The small menu-entry limit was checked: main menu max 9 characters, settings max 10 characters, spectrum submenu max 9 characters.
- Only the menu-language source and build workflow were changed for this task. No firmware build has been run or verified yet; the Italian build still needs to be tested in GitHub Actions before considering it ready.
- File: https://github.com/13HN2709/ats-mini-dev/blob/development/ats-mini/Menu_italian.cpp.src


## V1.31 release build handoff update (2026-10-30)

### User-confirmed features and tests
- User confirmed that the Italian menu build looks correct and that the XXL labels are fully readable; CW and RTTY decoder work remains paused and must not be advertised as available features.
- User tested the layout-selection correction on hardware: **Original** selects the original Default layout, **TEF6686** selects the custom TEF6686 layout, and a reset returns to TEF6686 as requested.
- User tested the compact EiBi status indicator in Layout-TEF6686.cpp: `Downloading...` appears in the top status area between the band label and Wi-Fi/battery area during EiBi loading, then disappears automatically after completion.
- The user confirmed both changes work correctly on the device.

### Layout and EiBi commits
- Draw.cpp correction: default/fallback drawing now routes to drawLayoutDefault() for the original layout rather than incorrectly routing both menu entries to TEF6686. Explicit TEF6686 selection and the fresh-install default index remain TEF6686. Commit: 610619929e2dbb75a1e655f0574cdfb02599aaf0.
- Layout-TEF6686.cpp: compact EiBi download status indicator. Commit: ccc5278e712abb710b77a24eb5f4f0412a319c01.
- User reports the new build/device tests passed for these two changes. This is user-confirmed hardware testing, not an independently observed Actions run.

### V1.31 release changelog scope
Only list features actually present and available to users in the exact V1.31 firmware build. Do not list paused or unfinished CW/RTTY decoders or other planned features.

Approved English feature summary for the release notes:
- Numeric keypad for direct frequency entry; VFO A/B and frequency transfer; adjustable tuning step; memory management.
- Display layouts: Original (the former Default layout), custom TEF6686 layout, Clean Layout, and Waterfall Layout.
- TEF6686 is the default layout on a fresh installation/reset.
- Improved frequency and RDS PS/RT presentation, status indicators, and EiBi download status feedback in the TEF6686 layout.
- Native VHF/FM Spectrum Scanner and Web Control Center spectrum scanning, including ALL Band scanning only if the exact release build contains the corresponding implementation.
- Web Control Center over Wi-Fi, including AP Only (WEB CTRL) and AP+Connect (WEB+Conn) modes.
- Dual RX, integrated BFO, Bandplan Assistant and available frequency/mode hints.
- German, English, and Italian menu languages.
- Updated OTA repository URL to the official 13HN2709/ats-mini repository, provided the exact release binary was built after the OTA URL fix commit.

### Current release workflow
- User is about to launch the V1.31 manual build on public main and add the release text. Wait for the actual workflow result before stating that the build passed or that release assets were published.
- Verify the official build matrix includes German, English, and Italian for both OSPI and QSPI before calling the release ready.
- After the build, verify all generated artifacts and confirm the OTA URL fix is included in those binaries. Then prepare/update the GitHub Release only when the user is ready.
- CW and RTTY are paused and inactive; exclude them from README, changelog, and release assets descriptions.
- Keep development in private ats-mini-dev → development. Only copy approved release source/text to public ats-mini when explicitly requested.

## Public Main status — checked 2026-10-30 (private handoff only)

### Repository separation — user instruction
- This `DEVELOPMENT_HANDOFF.md` belongs **only** in the private repository `13HN2709/ats-mini-dev`, branch `development`.
- Never copy this handoff/protocol into the official public repository `13HN2709/ats-mini`. The public repository must not contain `DEVELOPMENT_HANDOFF.md` or another internal development handoff document.
- Public `main` is for official firmware source, public-facing README/changelog, workflows, and release assets only. Keep internal test notes, pending work, and handoff details private.

### Official public repository snapshot
- Repository: https://github.com/13HN2709/ats-mini
- Branch: `main`
- Latest public HEAD observed in this session: `97650103e92a9a6bee2a6038a278f768e33f6b97` — “Show compact EiBi download status in TEF6686 layout”.
- The public source contains the V1.31 preparation changes discussed with the user, including:
  - `Draw.cpp` layout-selection correction: **Original** selects the former Default layout; **TEF6686** is the fresh-install/reset default.
  - `Layout-TEF6686.cpp` compact `Downloading...` indicator while EiBi data is loading.
  - `Menu_italian.cpp.src` and workflow language matrix for German, English, and Italian.
  - `Common.h` OTA firmware URL pointing to the official public repository `https://github.com/13HN2709/ats-mini`.
  - Build workflow matrix for OSPI and QSPI variants across `de`, `en`, and `it`, plus runner disk-space cleanup.
- The user has confirmed the layout selection/reset behavior, EiBi status indicator, and Italian menu appearance on device.
- The public source-code sync is **not proof that the current public-main workflow passed**. At the time of this update, the user was preparing/running the manual V1.31 build; its result and final release artifacts still need to be confirmed. Do not mark V1.31 release-ready until the actual Actions result and artifacts are checked.
- The existing V1.31 release assets mentioned earlier may predate the OTA URL correction; verify the binaries actually generated from current public `main` before publishing/replacing assets.
- CW and RTTY decoders remain paused/inactive and must not be advertised as available features.
- The approved public release changelog may include Waterfall Layout, but only features confirmed present in the exact release binaries should be described as available.

### Public Main links
- Source branch: https://github.com/13HN2709/ats-mini/tree/main
- Commit history: https://github.com/13HN2709/ats-mini/commits/main
- Build workflow: https://github.com/13HN2709/ats-mini/blob/main/.github/workflows/build.yml
- TEF6686 layout: https://github.com/13HN2709/ats-mini/blob/main/ats-mini/Layout-TEF6686.cpp
- Release v1.31: https://github.com/13HN2709/ats-mini/releases/tag/v1.31

### Next release checks
1. Confirm the public `main` manual workflow completes successfully for all six matrix combinations (OSPI/QSPI × German/English/Italian).
2. Inspect the produced artifacts and verify they include the OTA URL fix and the intended layout/EiBi/Italian changes.
3. Only then update or publish the official V1.31 release assets and release notes, when the user is ready.
4. Keep this protocol and future handoff updates exclusively in private `ats-mini-dev` → `development`.



## User update and build-failure fix (2026-10-30)

### Confirmed by user
- Web ALL Band Spectrum scanner is working well. Remove it from the active TODO list; do not spend further work on it unless a new issue is reported.
- Native VHF Spectrum work is complete. Remove it from the active TODO list; do not reopen it unless a new issue is reported.
- After the next public Main build succeeds, continue with the planned new layouts: GNBD miniradio-inspired layout and the lsongdev/tinyradio-inspired layout. Keep these separate from existing TEF6686 and Clean Layout code.

### Public Main workflow failure diagnosed and fixed
- Failed workflow run: https://github.com/13HN2709/ats-mini/actions/runs/40991869262
- Failed job: `build qspi-it`; other matrix jobs were cancelled because `fail-fast: true`.
- Exact error: `cp: cannot stat 'ats-mini/Menu_italian.cpp.src': No such file or directory`.
- Cause: public `main` workflow already selected Italian builds and attempted to copy `ats-mini/Menu_italian.cpp.src`, but that source file had only been added to the private development repository and was missing from public `main`.
- Fix committed directly to public `main`: added `ats-mini/Menu_italian.cpp.src` copied from the tested private development source, without changing the workflow or scanner/layout code.
- Fix commit: `7ddcc5d524abdefc352d20a225c54bd0a21d571a`.
- Verified after commit: the Italian source exists on public `main` (66,272 characters), and the workflow's `cp ats-mini/Menu_italian.cpp.src ats-mini/Menu.cpp` path matches it.
- This fixes the diagnosed missing-file failure, but **a new successful build has not yet been run**. Wait for the user to run the public Main workflow and confirm all six matrix jobs before marking V1.31 ready or publishing assets.

### Active immediate plan
1. User runs the public `main` manual build again after the Italian source fix.
2. If green, inspect artifacts and OTA metadata/URL, then finish V1.31 release when the user is ready.
3. After the build/release checks, work on the two planned new layouts in private `ats-mini-dev` → `development`.
4. Keep this handoff private. Do not add it to public `main`; do not create or delete branches without asking.


## Private-only layout rule — confirmed 2026-10-30

The user explicitly requires that the two new custom layouts remain private and must never be transferred into the official/public firmware repository.

- **Layout 1:** current TinyRadio-inspired layout (ats-mini/Layout-TinyRadio.cpp).
- **Layout 2:** the new custom layout shown in the user's uploaded photos/video on 2026-10-30. It is based on the original ATS Mini layout and is to be recreated as a separate layout option, not as a replacement for Original, TEF6686, Clean, Waterfall, or TinyRadio. Visual targets: large seven-segment-style main display; FM/AM mode and station name; when a valid RDS clock is received, replace the large frequency display with the RDS time, otherwise show the frequency; left-side pair of changing signal curves/bars, using **red and blue** in our version (the reference uses yellow and blue); bottom frequency ruler with a highlighted tuned-frequency marker; compact signal/stereo/status indicators. The exact data source for the two curves is unconfirmed: inspect the source/reference implementation and current radio APIs before deciding whether they represent RSSI/SNR or other values.
- Both layouts and their layout-specific code must remain in the private repository 13HN2709/ats-mini-dev.
- The user permits these two layouts to be included in the private development/mainline build (“Dev Main”), but **they must never be copied, cherry-picked, merged, synced, or otherwise transferred into the official/public repository 13HN2709/ats-mini or its main branch**.
- This exclusion also applies during every future upstream integration, release sync, new-feature transfer, merge, cherry-pick, or bulk file copy. Do not assume that a general feature transfer permits either layout to move across the private/public boundary.
- When transferring unrelated improvements to the official repository, explicitly exclude both layout files and any layout-specific registration/menu/build changes that would expose or compile them. Inspect the resulting diff before committing.
- Keep this rule in this private DEVELOPMENT_HANDOFF.md so new chats inherit it. Never copy this handoff document to the public repository.
- Do not change branch strategy or copy either layout anywhere without following this rule. If a future transfer is ambiguous, stop and ask the user first.

### TinyRadio current state
- The active TinyRadio layout is based on the lsongdev/tinyradio reference.
- Commit b474ebef061d16ce5161e072362d41e7bd74ed92 on private development changed the active BT selector styling to cyan.
- The intended active WEB selector styling should follow the user's reference image (cyan, not neon green); WEB should indicate active WEB CTRL or WEB+Conn. Do not claim this WEB color behavior has been changed unless the source is inspected and a commit confirms it.
- Do not modify shared RDS processing (including Station.cpp) to fix missing umlauts; any future fix must first be shown to be safely local to TinyRadio, otherwise leave it alone.
- The BT color change has not been independently confirmed by a successful build/device test in this handoff. The user will run the manual build and test it.


## Waterfall research and future NewWater layout (2026-10-30)

### User's explicit plan
- Preserve the existing Waterfall layout exactly as it is. Do not redesign, replace, or modify its rendering just to experiment with smoother animation.
- After the HJB layout has been implemented and tested, investigate a separate second waterfall layout, provisionally named **NewWater**.
- Keep NewWater in its own layout file and isolate its registration/drawing code as much as practical. Existing layouts, especially Waterfall, TEF6686, Clean, Original and TinyRadio, must continue to work unchanged.
- The user's desired inspiration is the very fluid waterfall demonstration they shared. Investigate whether its smoothness can be reproduced through efficient row updates, partial redraws, optimized color mapping, or other source-supported techniques; do not assume the cause before inspecting code.
- Agreed order: **HJB first; NewWater/waterfall experiments afterwards**.
- No implementation or build/device test for NewWater has been performed as part of this note.

### GitHub projects to inspect for NewWater
1. **sirruf/ats-mini-pro** — https://github.com/sirruf/ats-mini-pro  
   Interesting for its LVGL-based interface and spectrum/waterfall bandscope. Check hardware/display assumptions and porting compatibility before reusing ideas.
2. **gorucommit/ats-mini_custom** — https://github.com/gorucommit/ats-mini_custom  
   Inspect the `waterfall-experiment` branch as well as the main branch; it may contain an experimental waterfall implementation.
3. **Ho52198/ats-mini-monster** — https://github.com/Ho52198/ats-mini-monster  
   Review spectrum-analyzer and UI changes for ideas that could apply to a separate waterfall layout.
4. **G8PTN/ATS_MINI** — https://github.com/G8PTN/ATS_MINI  
   Older/original firmware reference. Existing notes indicate its frequency scale was removed, so it is not currently a confirmed match for the target layout.
5. **esp32-si4732/ats-mini** — https://github.com/esp32-si4732/ats-mini  
   Upstream/reference implementation to compare with the current firmware architecture.

These are inspiration/research candidates, not confirmed source matches for the user's fluid-waterfall video. Inspect relevant branches, history, and actual implementation before selecting any code or claiming a match. Prefer adapting concepts over copying code wholesale, and check licenses/compatibility.

### Current search status
- No exact source repository for the waterfall shown in the user's video has been confirmed yet.
- Continue searching forks, branches and older commits when the HJB task resumes or when the user asks to continue the source hunt.
- Do not let this research delay the agreed HJB-first priority.


## HJB layout — isolated implementation pass (2026-10-31)

- HJB is being implemented only in the private development repository, branch `development`.
- Separate renderer: `ats-mini/Layout-HJB.cpp`; declared in `Draw.h`, registered as layout index 7 in `Draw.cpp`, and listed in English, German and Italian menu source templates.
- The renderer currently draws a large seven-segment tuned-frequency readout, station name, FM RadioText and PTY, a left-side red/blue current RSSI/SNR level display, right-side RSSI/SNR bars and stereo/mono status, date when the firmware clock provides a valid date, and a bottom frequency ruler with a red tuned marker.
- Important correction: the firmware clock may be set by NTP, so it must not be mistaken for a received RDS clock. The main readout therefore remains the tuned frequency in this first build candidate. Do not add clock-as-RDS behavior unless a dedicated RDS-CT validity/source can be verified.
- AF remains omitted until the exact alternative-frequency API/data is verified. Do not guess a library method.
- Web Control's RDS JSON endpoint confirms the intended data sources: `getStationName()` for PS, `getRadioText()` for RT and `getProgramInfo()` for PTY. The shared RDS pipeline has not been modified for HJB.
- Recent isolated layout commits include: `415d74a`, `43ba269`, `6b4913f`, `fc879f0`, `da6a794`, `61f3f7c`, `f67abfc`, and `80d0cdd`.
- Before the first build, the source still needs compilation against the exact installed SI4735/LovyanGFX APIs. No build has run and no device test has been performed. Once the source review is complete, the user will run the manual GitHub Actions workflow on private `development`; do not trigger it without explicit confirmation.
- First device checks after a successful build: FM with and without PS/RT/PTY, FM frequency/unit alignment, SW/AM frequency and ruler, marker movement, date placement, RSSI/SNR indicators, and stereo/mono behavior.
- HJB remains private and must not be copied to public `13HN2709/ats-mini` unless the user explicitly approves a future public release.


## FT8/FT4 decoder port — initial source import (2026-11-20)

### Scope and branch
- Work is being performed only in private `ats-mini-dev` → `development`, sourced from `Mini_OS` → `dev/first-build-preparation`.
- User confirmed that the audio input and hardware compatibility are already verified on their device. Treat GPIO11 audio capture compatibility as user-confirmed; do not reopen it as a blocker.
- No changes to `main`, public `13HN2709/ats-mini`, or other branches.

### Imported source
- Added `src/ftx/FtxDecoder.h/.cpp` and `src/ftx/FtxDecoderService.h/.cpp`.
- Added `src/audio/AudioCaptureService.h/.cpp`.
- Imported the full FT8 library source tree under `src/ftx/ft8_lib/`, including common monitor, KISS FFT, FT8 encode/decode, LDPC, CRC, message and text modules and their license files.
- Adapted only the service include paths to the ATS Mini Dev directory layout.
- Added `src/ftx/FtxDiagnostics.h` as a serial-only adapter for the Mini OS `BootDiagnostics` calls. This avoids importing Mini OS storage/LittleFS dependencies.
- Updated Makefile dependency lists to include the imported C/C++ sources and headers under `src/audio` and `src/ftx`.

### Not yet integrated / not yet verified
- This is the decoder/audio/DSP source import only. No menu, decoder page, display, or user-facing activation has been connected yet.
- The original Mini OS test vectors have not yet been ported or executed in ATS Mini Dev.
- No firmware build, GitHub Actions workflow, or hardware flash/test was run by the assistant. User performs those checks manually.
- Offline UTC synchronization still needs source-level review. The capture service timestamps samples using `gettimeofday()`; before enabling slot processing, confirm the ATS Mini Dev system clock is set accurately without Wi-Fi, preferably from the existing RDS CT path if that path can safely set system UTC.
- Preserve all upstream FT8/KISS FFT license files.
- Keep the imported decoder isolated from existing radio layouts and scanner features.


### UTC clock review — completed 2026-11-20
- Reviewed `Station.cpp`: when a valid RDS CT epoch arrives, `showRdsTime()` marks RDS CT valid. If NTP is unavailable and the system clock is not already set, it calls `clockSetEpoch(epoch)`.
- Therefore the existing RDS CT path can establish the system epoch offline; the decoder must still wait until `clockAvailable()` is true before entering its UTC slot scheduler.
- NTP remains optional. Do not require Wi-Fi for offline FT8/FT4 when a valid RDS CT time has set the clock.
- This is source-level confirmation only. Validate the actual UTC offset/epoch behavior on the user's device before claiming slot timing is hardware-confirmed.


## Decoder frequency catalog — imported (2026-11-20)
- Confirmed the current `development` HEAD before editing: `334d66e6083cd965f22376937aa65ad3e8c31cd9` (the FT8/FT4 foundation commit).
- Imported the original Mini_OS `DecoderFrequencyList.h` into `ats-mini/src/radio/DecoderFrequencyList.h`, adapted to ATS Mini Dev's existing `Common.h` modulation constants and directory structure.
- Added the eleven contiguous category ranges and German/English category labels; added `src/radio/*.h` to the Makefile header dependencies.
- Source count check found **75** preset rows, not 74: CW 9, FT8 10, FT4 8, RTTY 5, DWD/Pinneberg 9, NCDXF 5, time signals 6, shortwave broadcast 8, SSB QSO 7, signal/data 6, CB 2. Preserve all 75 source rows unless a specific duplicate/incorrect preset is identified; do not silently drop one to force the handoff's stated count.
- The source header marked CW rows as `DecoderKind::CW`; ATS Mini Dev's catalog now uses only `None`, `FT8`, and `FT4`. CW remains a normal listening preset, in accordance with the user's requirement that only FT8 and FT4 open a decoder page.
- Commits: `07bccfdbe84b9f67b8157a2a033deb6758b0d274` (catalog); `561ccdcd718115356f913d00aba931a9989cd1e0` (Makefile dependency).
- Still pending: native menu integration, 2-second tune preview, FT8/FT4-only click routing, decoder page/lifecycle, Wi-Fi restore, UTC readiness gate, documentation/changelog, source review. No build, Actions run, or hardware test was started.
