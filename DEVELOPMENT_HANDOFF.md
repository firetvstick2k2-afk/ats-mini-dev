# ATS Mini Development Handoff

Last updated: 2026-11-21 (RTTY preset preview submenu)
Repository: https://github.com/firetvstick2k2-afk/ats-mini-dev (active development repository); Mini_OS reference: https://github.com/firetvstick2k2-afk/Mini_OS
Active work branch: `development` (never modify `main` unless explicitly requested)

## Latest bugfix: FT8/FT4 logging in offline mode (2026-11-21)
- User-reported issue: FT8 Actions > Logging stayed OFF and could not be enabled.
- Root cause: `ftxLogStart()` rejected offline synchronization and required a valid UTC date; `ftxLogMessage()` also discarded messages without valid UTC. Switching to offline mode additionally stopped and cleared the active log.
- Fixed in commit `4b3a3e8d46e6061235349d15bf4cad9e4bf8d84e`: logging can start without UTC, uses a unique `FT8_UNTIMED` / `FT4_UNTIMED` filename when no date is available, writes unknown timestamps for offline receptions, and remains active across online/offline sync changes.
- Changelog fragment: `changelog/+ft8-offline-logging.fixed.md`.
- Source review only. No compile, GitHub Actions run, or hardware test was performed; user must verify Logging ON, actual file creation, offline reception entries, and online/offline switching on device.

## Working agreement
- Keep this file current after every meaningful feature or code change. Use it as the handoff reference when starting a new chat.
- **Bugfix priority:** when the user reports a bug during planned feature work, pause the roadmap and handle that bug first; after the fix, resume the previous highest-priority task (currently JS8Call).
- New features must be isolated from existing features wherever practical. If implementing a feature would require changing shared feature-specific logic, duplicate that logic into a new module/state/handler for the new feature rather than refactoring the established feature.
- Preserve Dual RX, existing Web Spectrum behavior, Clean Layout and especially Layout-TEF6686 unless a change is explicitly required.
- Shared low-level radio/scanning primitives may be reused only after checking their side effects and compatibility.
- Build workflow is manual (`workflow_dispatch`). Do not claim a build/device test passed unless verified. User launches build and tests on ATS Mini.
- Keep branch hygiene simple; development first. User manages copying approved README/release changes to main unless explicitly asked.

## Previous task: native VHF Spectrum Scanner
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
- **Private development workspace:** https://github.com/firetvstick2k2-afk/ats-mini-dev
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
- Commits: `07bccfdbe84b9f67b8157a2a033deb6758b0d274` (catalog); `561ccdcd718115356f913d00aba931a9989cd1e0` (Makefile dependency); `f4fa722fdd0b264d6f5d75ac978cf4a1523c30ee` (use ATS Mini's `uint8_t` modulation representation).
- Still pending: native menu integration, 2-second tune preview, FT8/FT4-only click routing, decoder page/lifecycle, Wi-Fi restore, UTC readiness gate, documentation/changelog, source review. No build, Actions run, or hardware test was started.


## Permanent handover/documentation rule
- Keep this handover document current throughout all ATS Mini Dev work. It is the continuity source for future chats; do not rely on chat history alone.
- Document every meaningful change, investigation result, decision, planned task, discovered issue, correction, and remaining limitation as soon as it is made or agreed. Do not defer documentation until a feature is finished.
- For code changes, record the affected paths, behavior/change summary, commit SHA, and whether it is implemented, source-reviewed, built, or hardware-verified. Never imply a stronger verification level than actually completed.
- For plans, record the agreed behavior and constraints before implementation, then update the entry as work progresses or decisions change.
- At each handover point, record the exact repository, branch, current HEAD, completed work, in-progress work, next steps, known risks, and explicit tests not run.
- Keep all work on `development`; leave `main` untouched. User performs firmware builds, GitHub Actions, and hardware tests.


## Decoder frequency UI integration — in progress (2026-11-20)
### Agreed implementation plan
- Add a dedicated full-screen `CMD_DECODER_FREQUENCIES` screen without changing any radio layout or the TEF6686 renderer.
- Add the Decoder Frequencies entry to the English, German, and Italian main-menu source files; keep each menu's item indices stable and append the new item.
- First encoder click opens the eleven-category list; click on a category opens its preset list. Encoder rotation changes the highlighted category/preset. Long press backs out one level (or returns to the main menu).
- In the preset list, a two-second idle after the last encoder movement automatically tunes the selected preset for listening. Further rotation selects another preset and restarts the two-second timer.
- Tune using the narrowest existing non-FM band range that contains the preset frequency, changing only the selected band's mode/frequency/bandwidth as needed; keep the currently selected UI layout unchanged.
- Ordinary presets return to the active radio layout on click. FT8/FT4 presets are reserved for the dedicated decoder entry/page and service lifecycle, which remain a separate follow-up stage; do not start CW/RTTY decoders.
- No firmware build, GitHub Actions run, or hardware test is to be started by the assistant.


### Decoder frequency menu — first UI milestone committed (2026-11-20)
- Added `CMD_DECODER_FREQUENCIES` and its dedicated full-screen renderer in `Draw.cpp`; the active radio layout remains selected and is only redrawn when this command is exited.
- Added the new main-menu entry at index 20 in English, German, and Italian menu sources. The existing 20 item indices were preserved by appending the entry.
- Added category and preset navigation: main menu → 11 categories → presets. Encoder rotation changes selection; long press backs to the previous level or main menu.
- Added a 2,000 ms idle preview timer in the main loop. A preset is tuned after the encoder has stopped moving for two seconds; subsequent movement restarts the timer.
- Tuning selects the narrowest existing non-FM band that covers the preset, applies the preset modulation/frequency/bandwidth, then uses the existing `selectBand()` radio path. No radio layout source file was changed.
- Normal preset click returns to `CMD_NONE` (the currently selected layout). FT8/FT4 click currently reports that the decoder page is pending; it deliberately does not launch a partial decoder. Dedicated FT8/FT4 page, UTC/Wi-Fi lifecycle, and service connection remain the next implementation stage.
- Commits: `bb8aca0d031fad4782ed20dacccbf5f910190f01` (command/API); `ba1004b43279d9e33eac387c6665d240211b7baa` (draw dispatch); `d6ae5a87b8ff17a7322a8cbd6b79e5713f2315c8` (encoder/timer/back handling); `b92963444d6c20ab7227437107ad804fb9b49b24`, `ae490b302cc5f6263f34add2e5ea7437b4d73b47`, `a376b50d7285846753ec9cb102891cba1c71e8bc` (EN/DE/IT menu integration).
- Source-level review only. No build, Actions run, or hardware test was run.


### FT8/FT4 decoder page and service lifecycle — integrated (2026-11-20)
- Added `src/ftx/FtxUi.h/.cpp`; Makefile already includes `src/ftx/*.cpp` and `src/ftx/*.h`, so no build-list change was needed for these files.
- Added `CMD_FTX_DECODER` and dedicated page rendering in `Draw.cpp`. The page shows FT8/FT4 mode, UTC time, tuned frequency/modulation, audio level, slot/block state, decoded message text, decode count, and dropped audio blocks. It refreshes every 500 ms while active.
- FT8/FT4 preset click first ensures the selected preset is tuned, then opens the matching `FtxMode` through the existing `FtxDecoderService` and shared `AudioCaptureService`. No CW or RTTY decoder path is introduced.
- Entry requires `clockAvailable()`. When UTC is unavailable, the user receives an NTP/RDS CT prompt and the decoder is not entered. When UTC is valid, the code records the configured `wifiModeIdx`, calls `netStop()`, verifies the Wi-Fi mode is off, then starts the decoder service with Wi-Fi-off and UTC-ready flags.
- On service start failure, the decoder is exited and the configured network mode is restored. On encoder click or long press from the decoder page, the service is stopped, the configured network mode is restored via `netInit()`, and the previously active radio layout is displayed again.
- Preset tuning now applies the modulation-specific default step and the preset bandwidth (falling back to the mode's default bandwidth if needed). Clicking a preset before the two-second preview completes also tunes it immediately, so a quick confirm cannot leave the radio on the previous frequency.
- Relevant commits: `82689d682a2d26b219ce65319e5ec31e28f5e50f` (FTX UI header); `d80cc1c47556ceaa571e90085a3be4f89d78d79f` (FTX page and Wi-Fi/UTC/service lifecycle); `2f5f9091f17da2b1eb8f98597870c72e58b20e31` (decoder command); `fee72b19fb756e88255ceb7c03e28a13154b7789` (display dispatch); `b7242ee353219b9966f58d7ce775044a0add344c` (main-loop lifecycle/refresh/exit); `883c8271e7bd277a3cb1ab524209cc8af5f15bdb`, `e4400645a0d66cba97907deca630a7192e35e5bb`, `63785d0bc664c96c33017875cabc71a4d1d3fb63` (EN/DE/IT decoder start/stop routing); `17151c67ba3488278f094f23f5ae485624175956`, `c5629ac3f481aa9d993de55ccd450b02b6aff4a6`, `afe5272974f5855a2a66087dd46be7401447d437` (click-time tune and preview retry); `7adbe9ce9317810000c78b4f1daf6871052ee5e3`, `3cb47cedcb284490ad748d15ee5932c977057572`, `5d63cbb5f83fce343e2d23302436dfa747ca5177` (mode-specific band step/bandwidth defaults); `ef4867f7f0b7406920ba4407cf60028ba780af61` (show tuned frequency on decoder page).
- Known limitations / verification: the decoder page currently shows decoded text and service/audio diagnostics but does not yet render the decoder waterfall. Wi-Fi restoration follows the configured network mode, not a snapshot of transient AP client/connection state. Source-level review only; no firmware build, GitHub Actions run, or hardware test was performed. First FT8/FT4 hardware decoding remains unconfirmed.


### Decoder-menu long-press edge case — fixed (2026-11-20)
- Source review found that `ButtonTracker::isLongPressed` remains asserted while the physical button is held. Exiting the decoder screen/category list could therefore allow the generic long-press handler to toggle display sleep on the next loop iteration.
- Added a one-shot `decoderMenuLongPressHandled` latch in `ats-mini.ino`, reset only after button release. The same held press now performs exactly one back/exit action and cannot fall through into the generic sleep toggle.
- Commit: `6c56ae0adefcb54ffa69c517b4f07dcb8187a7b9`.

### Latest source review checkpoint
- Current `development` HEAD: `6c56ae0adefcb54ffa69c517b4f07dcb8187a7b9`.
- Verified the three localized main-menu source arrays contain 21 entries (the new decoder-frequency item is appended at index 20); the English, German, and Italian sources all route FT8/FT4 into the new page and route exit back to the radio screen.
- Verified all 11 catalog categories and all 75 source presets remain present.
- Compared `ats-mini/Layout-TEF6686.cpp` against the pre-integration tree at `f7667fc79d6f9a565c3ad0013676a21f27cefc86`: identical blob SHA `fe31d4fe0a3621334eda1aca08bde214712dcc2f`. No TEF6686 layout file was modified.
- `main` was not written to; all commits were made with branch explicitly set to `development`.
- No firmware build, GitHub Actions run, or hardware test was performed. The next useful development step is source-reviewing the decoder timing/audio path and adding a waterfall view to the decoder page if desired; then the user can perform their own build/hardware validation.

## GitHub Actions build narrowed to OSPI-DE (2026-11-20)

- User requested that once the currently running six-way build completed, future builds should generate only the German OSPI target. The manual run `44690333439` completed successfully before this change; all six matrix jobs succeeded, including `build ospi-de`.
- Confirmed uploaded OSPI-DE artifacts from run `44690333439`: `ats-mini-dev-v1.30-165-g54a7d0d-ospi-de`, `ats-mini-v1.30-165-g54a7d0d-ospi-de-ota.bin`, and `sketches-report-ospi-de`. The run produced 18 artifacts total across all six targets. Artifact retention reported expiry on 2027-02-18.
- Updated `.github/workflows/build.yml` on `development`: matrix language is now only `de`; board matrix retains only `esp32s3-ospi` with suffix `ospi`. QSPI, English and Italian matrix entries were removed. All build steps, artifact upload logic, manual trigger, and release logic were otherwise left unchanged.
- Workflow change commit: `fefad16d8f2c6ae5a8932528f1acff2cf68abc19`.
- Future manual workflow runs on `development` will execute one matrix job only: `build ospi-de`. This workflow edit does not rebuild the current firmware; it affects subsequent runs.
- No changes were made to `main`. Hardware flashing/testing remains the user's responsibility; the successful GitHub Actions compile is not a hardware test.


## Release / future build-matrix restoration note (2026-11-20)

- User's current testing requirement is **OSPI-DE only**. Keep the active GitHub Actions build matrix narrowed to `lang: [de]` and the single board profile `esp32s3-ospi`. Do not spend CI time building QSPI, English, or Italian for routine development/testing.
- This is a convenience reduction for development/testing, **not a permanent deletion or abandonment** of the other targets. When preparing a future release, restore the additional targets if the user wants the full release matrix.
- Exact previous matrix before narrowing (from the successful manual run `44690333439`):
  ```yaml
  matrix:
    lang: [de, en, it]
    board:
      - profile: esp32s3-ospi
        fqbn: esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,CPUFreq=80,USBMode=hwcdc,FlashMode=qio,PartitionScheme=custom,DebugLevel=none
        artifact-suffix: ospi
        cli-compile-flags: |
          - --profile
          - esp32s3-ospi
          - --export-binaries
      - profile: esp32s3-qspi
        fqbn: esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PSRAM=enabled,CPUFreq=80,USBMode=hwcdc,FlashMode=qio,PartitionScheme=custom,DebugLevel=none
        artifact-suffix: qspi
        cli-compile-flags: |
          - --profile
          - esp32s3-qspi
          - --export-binaries
  ```
- To restore the full matrix for a release, change `lang: [de]` back to `lang: [de, en, it]` and reinsert the exact QSPI board block above. Keep the OSPI board block. This produces the six combinations OSPI/QSPI × DE/EN/IT.
- The existing workflow file is `.github/workflows/build.yml`; its build, artifact upload, manual `workflow_dispatch` trigger, and release job logic were retained during narrowing. Restore only the matrix unless a separately reviewed release requirement calls for another change.
- The last full-matrix run `44690333439` completed successfully: all six build jobs passed, including OSPI-DE. It produced 18 artifacts total. OSPI-DE artifacts included `ats-mini-dev-v1.30-165-g54a7d0d-ospi-de`, `ats-mini-v1.30-165-g54a7d0d-ospi-de-ota.bin`, and `sketches-report-ospi-de`.
- Current active matrix is intentionally OSPI-DE only. This note preserves the exact prior settings for later restoration; it does not itself change the workflow.


### LittleFS logo directory preparation for DAB Style — implemented (2026-11-20)
- Added `logosInit()` in `ats-mini/Storage.cpp`, called immediately after the existing `diskInit()` startup call. It creates `/Logos/` when missing and verifies that an existing path is actually a directory.
- On startup it scans only files directly in the LittleFS root and collects PNG files case-insensitively. It excludes the existing splash path `/splash.png`; directories and non-PNG files are untouched.
- Root-level PNG files are moved into `/Logos/<filename>` using LittleFS rename. Existing destination files are never overwritten; on a name collision the root-level source is left untouched. The scan is collected before renaming to avoid skipping directory entries.
- The existing `diskInit()` mount/format behavior was not modified. The helper does not format, erase, or recreate LittleFS.
- This is storage preparation for the DAB Style logo integration; it does not yet add a DAB Style layout or change the logo renderer. The MiniOS repository currently contains planning/architecture documentation for layouts and storage rather than an executable implementation of this migration, so the behavior above is implemented directly in ATS Mini Dev.
- Source-level review only; no firmware build, Actions run, or hardware test has been performed.


### DAB Style FM layout port — integrated (2026-11-20)
- Ported the DAB-style FM screen structure from the user's ATS Mini OS development branch into ATS Mini Dev as the separate `ats-mini/Layout-DABStyle.cpp` renderer. The layout includes the band/header row, valid RDS clock, Wi-Fi/battery indicators, PI-matched local sender-logo viewport, station name, large FM frequency, MHz label, PTY badge and framed RadioText row.
- Local station PNG lookup uses the migrated `/Logos/` directory and searches filenames case-insensitively for the current four-character RDS PI code. It does not initiate network logo lookup. The image is drawn from LittleFS and is not copied into firmware resources.
- Added the layout as a new UI layout choice in English, German and Italian menu source files; existing layout indices 0–7 and the existing default (TEF6686, index 5) are unchanged. DAB Style is appended as index 8.
- Special routing: when DAB Style (index 8) is selected, FM uses DAB Style; any non-FM mode/band uses Layout-Clean as the universal fallback. Returning to FM automatically restores DAB Style. When any other layout (including TEF6686) is selected, the existing normal system layout dispatch remains unchanged.
- Added the new renderer to `Draw.cpp` and the Makefile source list. No TEF6686 renderer changes were made.
- No fixed logo is needed on non-FM bands: sender PI logos are FM/RDS-specific. The Clean fallback retains its bandplan guidance and does not display a stale FM logo.
- Source-level integration only. No firmware build, GitHub Actions run, or hardware test has been performed. User must run the manual OSPI-DE build and then test FM → non-FM/All Band → FM switching and local logo rendering.


### DAB Style alternative layout and color refinement — implemented (2026-11-20)
- User requested TinyRadio instead of Layout-Clean as the non-FM fallback while DAB Style (index 8) is selected. Updated the special dispatch in `ats-mini/Draw.cpp`: FM continues to use DAB Style; non-FM bands now use `drawLayoutTinyRadio()`. Returning to FM restores DAB Style automatically. Other selected layouts and their normal dispatch remain unchanged.
- Updated `ats-mini/Layout-DABStyle.cpp`: all former green accent elements (header accent, divider, badge frame/text and RadioText frame) now use bright cyan-blue RGB565 `0x07FF`, matching the vivid blue menu frame. The large frequency digits now use white `0xFFFF`, matching the menu text. The MHz label remains cyan-blue and the other white elements remain unchanged.
- Commits: `63354f1c598fc6ad35e14a6676e6e2095011a455` (TinyRadio fallback); `87a9c04ad1ff0710e68712f659c3e28a0ac5ac68` (DAB Style colors).
- Source changes committed on `development` only. No firmware build, GitHub Actions run, or hardware test was performed. User should test DAB Style on FM, switch to a non-FM band to confirm TinyRadio, then return to FM and confirm DAB Style is restored.


## Current handoff checkpoint — 2026-11-20 evening
- Repository: private `firetvstick2k2-afk/ats-mini-dev`; active branch: `development`; latest verified commit: `5d6aa64953f890707a9f3090b46e3d353a42c606` (handoff documentation commit). Keep `main` and the public release repository untouched.
- Latest code changes: `ats-mini/Draw.cpp` routes DAB Style (layout index 8) to DAB Style on FM and TinyRadio on non-FM bands; returning to FM restores DAB Style. `ats-mini/Layout-DABStyle.cpp` replaces green accents with RGB565 `0x07FF`, matching the bright cyan-blue used by the main-menu frame; frequency digits are white `0xFFFF`. Relevant code commits: `63354f1c598fc6ad35e14a6676e6e2095011a455` and `87a9c04ad1ff0710e68712f659c3e28a0ac5ac68`.
- `DEVELOPMENT_HANDOFF.md` has been updated with those changes. A separate `NEW_CHAT_HANDOVER.md` contains the concise startup brief for the next conversation.
- Build status for these latest DAB Style changes is **pending user confirmation**. User said they were starting the manual build; no run result has been reported in this chat. Do not assume success. If the build fails, request the exact failing log/error and fix only the affected source. If it passes, ask for device verification of FM → non-FM/All Band → FM, station logo, and the updated colors.
- Routine build target remains **OSPI-DE only** via manual `workflow_dispatch`. User starts builds and flashes/tests the ATS Mini V4; assistant must not trigger or claim a build/device test without evidence.
- No changes to TEF6686 layout, default layout index (5), main branch, or public release repo.
- Next broader development priorities already agreed: (1) Speicher-/Speichermanagement review, especially LittleFS/logo storage safety; (2) review and correctly complete half-finished ports/features instead of adding more features; (3) stabilize core functions before resuming optional decoder/waterfall polish. Preserve working LittleFS mount/format behavior; do not format or erase user data.


### OSPI-DE Build-Fehlerbehebung und Development-Workflow — 2026-11-20
- Nutzer bestätigt ausdrücklich, dass ausschließlich auf `development` gearbeitet wird; `main` darf nicht als Arbeitsbranch verwendet werden.
- Untersucht wurde GitHub Actions Run #3 / ID `44800260714` auf `development`. Fehlerursachen laut Compiler: fehlendes `Menu.h` in `Layout-DABStyle.cpp` (dadurch unbekannte `getCurrentBand()`, `CMD_NONE`, `CMD_SCAN`, `CMD_FREQ`, `drawSideBar()`) sowie LovyanGFX Sprite-/LittleFS-Inkompatibilität bei `spr.drawPngFile(LittleFS,...)`.
- `ats-mini/Layout-DABStyle.cpp` angepasst: `LittleFS.h` wird vor `Common.h`/LovyanGFX geladen; `Menu.h` liefert die fehlenden Deklarationen. PNG wird nun über ein geöffnetes LittleFS-`File` als Stream mit `spr.drawPng(&logoFile, ...)` dekodiert. Logo-Suche und Fallback-Anzeige bleiben erhalten.
- `.github/workflows/build.yml` auf `development` neu strukturiert: ausschließlich OSPI-DE und Deutsch; erzeugt/lädt nur `Mini_OS_#${{ github.run_number }}.bin` als nicht archiviertes Build-Artefakt (`archive: false`, direkter BIN-Download). OTA-Metadatenprüfung, OTA-BIN, Partitions-/Bootloaderdateien, Sketch-Report und ZIP-Erstellung entfernt. Bei manuell angegebener Release-Version bzw. passenden Tag-Events wird die rohe BIN als GitHub-Release-Asset bereitgestellt.
- Commits: Code `188f0cb7ba75b99b8cb6c7a7cc160d0396f04d1c`; Workflow `9b38c9c954999269e00cb16a59014975a6e0c4a4`.
- Beide geänderten Dateien wurden nach dem Commit erneut von `development` abgerufen und auf die erwarteten Inhalte geprüft. Kein Build/Actions-Run/Flash durch Assistant. Nächster Schritt: Nutzer startet den Build manuell auf `development`; erst danach Compiler- oder Hardware-Erfolg dokumentieren.

### Future feature: NAVTEX decoder without hardware modification — noted 2026-11-20
- User wants NAVTEX support considered for ATS Mini V4 only if it works with the stock, unmodified hardware. No soldering, extra microphone, external sound card, board changes, or other hardware modifications are acceptable.
- User confirms that their ATS Mini V4 already decodes CW, RTTY, and FT8 successfully without any hardware modifications, tested with both HJ Berndt and Diamond Radio firmware. Therefore, the existing hardware/audio path is available for software-based digital decoding; this is a user-confirmed hardware test.
- NAVTEX is a future firmware-only investigation, not implemented yet. It uses SITOR-B (FEC), so a conventional RTTY decoder cannot simply be assumed to decode NAVTEX. A dedicated SITOR-B decoder/framing and error-correction path will need to be researched or implemented.
- Initial target to investigate: 518 kHz international NAVTEX; consider 490 kHz national-language service and 4209.5 kHz only after the core decoder is proven. These are candidate frequencies, not a promise of current receiver coverage or successful decoding.
- Proposed scope: inspect the already-working internal audio/DSP path used by the existing decoders; determine whether it can feed a dedicated SITOR-B decoder without disturbing CW/RTTY/FT8; then display decoded NAVTEX message headers and text on the ATS Mini screen.
- Hard constraint: do not add or recommend hardware modifications as a prerequisite. If the stock V4 audio path cannot support reliable SITOR-B decoding in firmware, stop and report the limitation rather than asking the user to modify the device.
- Priority: backlog / later feature. Do not start implementation until the user explicitly brings NAVTEX back into scope and the agreed core stability/storage review priorities have been addressed.
- No source code changed, no build run, and no NAVTEX reception test has been performed. Feasibility remains to be established.

### FT8 reset diagnosis and first stack patch — 2026-11-20
- User captured a live Android USB-CDC serial log; no PC required. The FT8 worker ran with `wf_blocks=93/93` for two consecutive FT8 slots, then panicked during the next slot.
- Exact panic: `Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception)` / `Stack canary watchpoint triggered (ftx-decoder)`. ESP32-S3 rebooted and the existing Recovery Boot returned to App1 successfully.
- User recalls an earlier run resetting around block 91/95 during the first slot. Keep both observations: one run failed during a slot near completion; the latest run completed two full slots and failed on the third. The current log proves a stack-canary violation in task `ftx-decoder`, but does not yet identify the exact overflowing call/frame.
- Source inspection on `development`: `ats-mini/src/ftx/FtxDecoderService.cpp` created `ftx-decoder` with an 8192-byte stack. The worker performs `decoder_.decodeSlot()` while holding the engine mutex. `FtxDecoder::decodeSlot()` in `FtxDecoder.cpp` allocates candidate, message/status, decoded text, offsets, and tone arrays on the task stack; library decode/LDPC calls add nested stack use. This is the first area to investigate.
- First diagnostic mitigation committed to `development`: increase the worker stack from 8192 to 16384 bytes and log `uxTaskGetStackHighWaterMark(nullptr)` immediately before and after each decode, including free words and bytes. Commit: `21d6db3756f1d7332eb4f48d7da2c83dd541ba21`.
- This is a targeted diagnostic/mitigation, not a confirmed final fix. No build or hardware test has been run for this patch. User should run the manual OSPI-DE build, then observe several FT8 slots over a longer period and capture the new `[FTX][STACK]` lines. If stack canary still triggers, inspect the backtrace and reduce large local stack allocations / move suitable work buffers to controlled heap or static task-owned storage; do not blindly keep increasing stack.
- USB serial capture via Android app Serial USB Terminal works at 115200 8N1 over USB CDC. This means PC is not required for future live logs. Keep LittleFS persistent crash logging as a fallback/future enhancement, not the immediate blocker.

### Development build workflow speed-up — 2026-11-20
- User reported that development firmware builds take close to 10 minutes and compared this with MiniOS at roughly 5–6 minutes.
- Reviewed `.github/workflows/build.yml` on `development`. The prior workflow fetched the full Git history and all tags, then ran a costly disk-cleanup step before compiling. Neither is needed for a normal manually dispatched development build.
- Updated the development workflow: shallow checkout (`fetch-depth: 1`, no tag fetch); use the short commit SHA as the fallback build slug (an explicitly supplied revision is still preserved); remove the disk-cleanup step; cache `~/.arduino15` so Arduino CLI packages, ESP32 platform files, and libraries can be reused across runs.
- Workflow commit: `d421dcc0f5ac2e1398435d7ffa6039cb5fa8d6bd`.
- This is an expected speed improvement, not yet measured. The first run after the change may still need to populate the cache; compare the next two successful OSPI-DE runs. Do not change `main`; user builds manually and does not want Actions triggered automatically.
- Earlier FT8 stack-size correction: commit `5552f57f7ca66acf46ca92fb6b2ee8d6657f47f8` fixed a malformed line-comment that had hidden the `kWorkerStackBytes = 16384` definition, and retained the before/after stack high-water diagnostics. The prior note's commit `21d6db...` was not the final corrected source state.

## Verbindliche Portierungsquelle Mini_OS → ATS Mini Dev — 2026-11-20
- **Grundsatz des Projekts:** Die Funktionen und Layouts, die wir in ATS Mini Dev übernehmen bzw. weiterentwickeln, stammen aus dem Repository `firetvstick2k2-afk/Mini_OS`, Branch **`dev/first-build-preparation`** (der tatsächliche Branchname; nicht `dev/first-build-reparation`).
- **Zielrepository:** `firetvstick2k2-afk/ats-mini-dev`, ausschließlich Branch **`development`**.
- Wir portieren die relevanten Mini_OS-Funktionen und -Layouts von diesem Quellbranch nach ATS Mini Dev / `development`. Dazu gehören ausdrücklich bereits übernommene Komponenten wie **DAB Style**, **FT8-Decoder** und weitere aus Mini_OS importierte Funktionen.
- Mini_OS ist damit die maßgebliche Herkunfts-/Referenzbasis für diese Portierungen. Bei Änderungen oder Fehleranalysen zuerst den entsprechenden Stand im Quellbranch prüfen und dann die ATS-Mini-Dev-Integration auf `development` abgleichen.
- Die Portierung bedeutet nicht, dass jede Mini_OS-Funktion automatisch vollständig oder identisch integriert ist: den jeweiligen Integrationsstatus weiterhin einzeln prüfen und als vollständig / teilweise / offen dokumentieren.
- **Branch-Schutz bleibt verbindlich:** Mini_OS-Quellbranch nur als Quelle lesen; ATS Mini Dev ausschließlich auf `development` ändern. `main` und das öffentliche Repository bleiben unangetastet, sofern der Nutzer nichts anderes ausdrücklich verlangt.


## FT8 decoder display redesign — 2026-11-20
- The FT8/FT4 decoder page was visually redesigned in `ats-mini/src/ftx/FtxUi.cpp`; no CW decoder files or CW integration were added.
- Top row: tuned frequency on the left; cumulative decoded-message count and summed LDPC error count on the right (`DEC n ERR n`).
- A cyan slot-progress bar shows collected decoder blocks against the slot maximum.
- Three alternating dark/medium-blue message rows show the most recent decoded messages in larger bold text. Each row has a small country flag when its callsign prefix is recognized, the message text, and the distance on the right.
- Grid locator distance is calculated from the configured QTH locator `JO31TV` to a 4- or 6-character Maidenhead locator found in the decoded message. Missing/invalid locator displays `---`; unknown callsign prefix does not invent a flag.
- Current country-prefix/flag mapping is intentionally a starter set of common DXCC prefixes, not a complete DXCC database. Expand only with a reliable prefix dataset if broader coverage is required.
- UI keeps the latest three messages across slots; the decoder DSP/audio/slot logic was not changed. The displayed error counter sums `ldpcErrors` reported on successfully decoded messages; it is not a count of every failed decode attempt.
- Source commits: `c2af7005bc4c70a4a7c36821a83f1fc020a1288f` (initial UI) and `2eac7c1ea2a11a7fc23313db7bf76a0a96bb911a` (slot-history fix).
- Source review only; no build or hardware test has been run. Next: manual OSPI-DE build on `development`, then verify actual flag glyph/graphics, row readability, QTH distance, progress bar, and FT8/FT4 behavior.


## FT8 audio monitoring and result retention — 2026-11-20
- User confirmed the FT8 decoder has now successfully decoded messages on hardware (the latest redesigned build was not yet tested at the time of this request; this confirmation refers to the running decoder behavior).
- While the FT8/FT4 decoder page is active, receiver output volume is temporarily raised to at least 50/63 via `rx.setVolume()` so the user can hear the received signal and the existing audio tap receives the higher output level. If the user's volume is already above 50, it is left unchanged. The exact previous volume is restored on decoder exit. The ADC sample buffer and decoder DSP path are not digitally amplified or modified.
- When a slot has zero decoded messages, `FtxDecoderService::decodeAndPublish()` now keeps the last non-empty published result set instead of replacing it with an empty array. This complements the UI's three-message rolling history so a weak/empty subsequent slot does not wipe the last successful decodes.
- Commits: UI/audio volume `4e6d9c8caf325e336cf32ef56e36b484dc442c48`; empty-slot result retention `c0b3a3111d76410bb27bfff7dc5a1c635ee95d0d`.
- No build or hardware verification was run for these two changes. User should manually run the OSPI-DE workflow on `development`, check audible volume during FT8, confirm volume restoration on exit, and verify that two prior messages remain visible when the next slot has no successful decode.


## FT8/FT4 decoder display polish — 2026-11-21
- Unknown/unmapped country flags are now displayed as a dark-blue badge with a bright cyan border and bold cyan country-code text, instead of the subdued fallback label.
- The empty-history placeholder now reads `Waiting for FT8/FT4 decodes...` in bold cyan rather than muted gray.
- Only `ats-mini/src/ftx/FtxUi.cpp` was changed; decoder DSP, audio capture, slot timing, and message retention are untouched.
- Code commit: `6bec9ae62e3ef7858b6ebf513fc694e463a24c8d`. Source-only change; no build or hardware test has been run. User should run the manual OSPI-DE workflow on `development`.


## RTTY decoder port — upstream engine imported (2026-11-21)
- User explicitly rejected the previous ATS Mini Dev RTTY decoder because it did not work and instructed a clean import from `https://github.com/joaquimorg/ham_decoder`, starting from zero. The prior RTTY implementation is not being reused.
- Imported the upstream MIT-licensed Baudot/ITA2 DSP engine into `ats-mini/src/rtty/RttyDecoder.cpp` and its public API into `ats-mini/src/rtty/RttyDecoder.h`. Added the upstream MIT license as `ats-mini/src/rtty/LICENSE.md`.
- The upstream project expects `DSP_SAMPLE_RATE`; this isolated adaptation defines it as **4000 Hz**, matching the existing ATS Mini `AudioCaptureService` GPIO11 stream. The upstream default is 45.45 baud, with its own configurable tone pair, baud rate (30–300), polarity auto/normal/reverse, squelch and ITA2 shift handling. No resampling was added.
- Added `src/rtty/*.cpp` and `src/rtty/*.h` to the ATS Mini Makefile's source/header wildcards so the imported engine is compiled with the firmware.
- This is **engine import only**, not a completed usable RTTY decoder page yet. Still required: integrate a reliable two-tone FSK detector / tone-pair tracking with the 4 kHz stream; create an isolated RTTY capture/service and connect it to the decoder; add the requested CW-style RTTY UI with four text rows, frequency upper-left and receive/status upper-right; wire menu/navigation and cleanly release audio capture on exit.
- The RTTY engine consumes float samples and requires a detected pair of audio tones. Do not feed raw ADC values or assume tone frequencies; normalize the existing 12-sample audio blocks and design/test a stable tone-pair estimator before claiming decode functionality.
- **Hard isolation rule:** do not modify any file under `ats-mini/src/ftx/`, do not modify FT8/FT4 DSP, capture, slot timing, UI, settings or menu behavior as part of RTTY. FT8/FT4 is frozen and must remain untouched.
- Target repo/branch: `firetvstick2k2-afk/ats-mini-dev`, branch `development` only. Source repo remains read-only. User starts Actions builds and hardware tests; no build, Actions run, or device test has been performed for this import.
- Commits: engine `52632c6318c81df19a57eef037d0618ef445a6ee`; API header `e398a85f65ceeb037d82423c27adfa70a106e9e6`; MIT license `54f312dd6ee40cf254f67dc68bde6320e6c11043`; Makefile integration `e9f4c7e43d0a6d0359a8f3796808f608098ee871`.


## RTTY decoder integration — 2026-11-21
- Corrected the case-sensitive include in `ats-mini/src/rtty/RttyDecoder.cpp` to `#include "RttyDecoder.h"` (commit `bb2e9cfff95596d6c4f00a4c0e140639c7b3a11d`). This matches the actual header filename and avoids a guaranteed include lookup failure on case-sensitive build environments.
- The RTTY engine remains an imported, standalone module only. It is not yet connected to `AudioCaptureService`, because it requires a robust two-tone FSK estimator/tracker before `rtty_process()` can be called with valid tones.
- Do not feed raw ADC samples to the engine. The capture stream is 4 kHz signed ADC data; a dedicated RTTY path must remove DC, normalize to float, estimate mark/space tone frequencies and confidence, then feed the engine. Keep this path isolated from all files and behavior under `ats-mini/src/ftx/`.
- CW reference UI source has not yet been located in the currently available `Mini_OS` branches/paths. Do not invent a substitute layout or copy the generic `Layout-Decoder.cpp` as if it were the CW reference. Need locate the exact existing CW layout in the correct Mini_OS source location before building the RTTY four-row screen.
- No build, GitHub Actions run, flash, or hardware test was performed for this correction.


## RTTY decoder — implementation update 2026-11-21

### CW reference recovered
- Exact reference found read-only in `firetvstick2k2-afk/Mini_OS`, branch `dev/first-build-preparation`, file `firmware/ats-mini-os/src/display/DisplayDriver.cpp`, function `drawLegacyCWPage()`.
- RTTY screen follows that visual layout: black/navy background, upper frequency/status bar, four alternating deep-blue text rows, RX indicator and bottom encoder hint. FT8/FT4 display is not changed.

### RTTY implementation on `development`
- Imported ITA2/Baudot engine remains in `ats-mini/src/rtty/RttyDecoder.cpp/.h`.
- Added `RttyService.cpp/.h`: separate GPIO11 ADC capture task at 8 kHz, independent of the shared 4 kHz FT8/FT4 audio capture; queue/worker separation; DC removal; Goertzel tone-pair search from 300–3500 Hz; automatic polarity; engine input normalization.
- 8 kHz is intentional: standard 2125/2295 Hz audio tones cannot both be represented by a 4 kHz sample stream. Do not change `AudioCaptureService` or any FTX files to accommodate RTTY.
- Added `RttyUi.cpp/.h`: four-row CW-style screen, receive indication, 45.45 / 50 / 75 baud selection by encoder rotation, click or long-press exit. Wi-Fi is paused while the decoder runs and restored on exit; playback volume is temporarily raised and restored.
- The RTTY decoder screen is a dedicated command and is not routed through the FT8/FT4 screen.

### Decoder Frequency presets
- `DecoderKind::RTTY` is now assigned to all five dedicated amateur RTTY presets (80/40/20/15/10 m).
- Also assigned to the Pinneberg DDH/DDK entries and the DWD RTTY entries (3.855 MHz is weather fax, not RTTY).
- DWD weather-fax presets (7.880 and 13.882 MHz) remain listen-only and are deliberately not sent to the RTTY decoder.
- The decoder-frequency menu opens the RTTY screen for entries tagged `DecoderKind::RTTY`; frequency preview/tuning remains the existing mechanism.

### Verification and limitations
- FT8/FT4 source files and existing capture implementation were not edited for RTTY. TEF6686 layout and LittleFS/storage code were not edited.
- Static source review only; no compiler build, GitHub Actions run, flash or on-air RF validation has been performed. The current GitHub integration exposes read operations and repository file commits but no workflow-dispatch action, so the OSPI-DE workflow was not started from this session.
- The Goertzel pair estimator and real RF decoding still require validation with an actual RTTY signal. The selected baud is manual (45.45 / 50 / 75); tone pair and polarity are automatic. The current implementation is a first build candidate, not yet a hardware-verified decoder.
- Next: user starts the manual OSPI-DE workflow on `development`; fix any compiler errors from its log before hardware reception testing.


## Correction: Pinneberg weather fax vs RTTY (2026-11-21)
- Corrected `DecoderFrequencyList.h`: 3.855 MHz (DDH3) is FAX, not RTTY. The preset is now `DWD FAX 3.855` with no RTTY decoder route.
- 7.880 MHz (DDK3) and 13.8825 MHz (DDK6) are also FAX and remain outside the RTTY decoder.
- Pinneberg RTTY presets are the text services: 147.3, 4583, 7646, 10100.8, 11039 and 14467.3 kHz. RTTY 147.3 kHz uses a narrower 85 Hz shift and needs a suitable detector configuration/test.
- Source checked: DWD maritime weather service frequency table and DWD RTTY transmission plan. Keep weather fax and RTTY as distinct decoder types.


## RTTY BFO control — 2026-11-21
- Added a three-state encoder control cycle on the RTTY page: **BAUD → BFO AUTO → BFO MANUAL → BAUD**. A short click changes the control mode; long press still exits the decoder.
- In BAUD mode, encoder rotation selects 45.45 / 50 / 75 baud as before. In BFO MANUAL mode, rotation changes the receiver's real SI4735 SSB BFO in 5 Hz increments. The manual range is limited to ±250 Hz from the BFO value present on decoder entry.
- BFO AUTO mode estimates the detected FSK pair's center and gently adjusts the radio BFO toward the conventional 2210 Hz audio center, with an 18 Hz deadband, 8 Hz maximum correction per adjustment, and a 500 ms adjustment interval. It is an initial automatic centering strategy and requires RF validation; non-standard tone plans (including the 85 Hz DDH47 shift) still need separate testing.
- The RTTY screen footer displays the active encoder mode and current BFO offset. On decoder exit, the entry BFO and original volume are restored. RTTY remains isolated from FT8/FT4; no FTX files, shared audio capture, TEF6686 layout, or LittleFS code were changed.
- Source-only implementation on development; no compiler build, Actions run, flash, or hardware test was performed. The automatic correction direction/response and audible result must be confirmed on the ATS Mini V4 with a known RTTY signal before calling this validated.


## RTTY preset baud defaults — 2026-11-21
- Added a recommended baud-rate field to each decoder frequency preset. The five amateur HF RTTY presets default to **45.45 baud**; all six DWD/Pinneberg RTTY presets (including DDH47) default to **50 baud**.
- When the user opens an RTTY preset, the selected preset's baud rate is passed into the RTTY UI, selected in the encoder's BAUD mode, applied to the decoder service immediately, and shown in the top-right RX/NO RX status (e.g. `NO RX 50.00Bd`). Encoder selection remains available for manual override.
- The RTTY decoder API retains a 45.45 baud default for any non-preset entry. No FT8/FT4 files, DSP, audio capture, TEF6686 layout, or LittleFS/storage code changed.
- Source-only change on `development`; no build, Actions run, flash, or hardware test performed. User starts the manual OSPI-DE workflow.


## RTTY display alignment and contrast — 2026-11-21
- User reviewed the first on-device RTTY screen and supplied a photo. The baud/RX label was red and overlapped the activity dot; frequency glyphs sat too high in the header; the waiting placeholder was gray and too high in its row.
- Updated only `ats-mini/src/rtty/RttyUi.cpp`: header frequency vertically centered; RX/baud text white and shifted left with a smaller font; activity dot moved to the upper-right corner so it no longer covers the text; decoded rows and the waiting placeholder vertically centered inside their blue panels; waiting text and footer changed to white for contrast.
- Four decoded message rows retain their existing panel geometry and white bold text. No RTTY DSP/service, baud presets, FT8/FT4, shared audio, TEF6686, or LittleFS changes.
- Source-only visual adjustment. No build, Actions run, flash, or hardware test performed; user will inspect the next OSPI-DE build on device.


## RTTY long-press exit reliability — 2026-11-21
- User reported that holding the encoder while the RTTY screen was open did not exit; device required a restart.
- Moved the RTTY-specific long-press handling to immediately after the physical ButtonTracker sample in `ats-mini/ats-mini.ino`, before serial/BLE/TCP and encoder event handling. The handler is one-shot per physical press, clears pending short-click flags, calls `rttyUiExit()`, and refreshes the main screen/timers. The existing later generic RTTY long-press branch remains guarded by the same one-shot flag and therefore cannot double-exit.
- Scope is limited to RTTY exit dispatch; decoder DSP/service, baud presets, FT8/FT4, shared audio, TEF6686 and LittleFS are unchanged.
- Source-only fix; no build, Actions run, flash or hardware verification. On next device test, verify long press exits RTTY, restores BFO/volume/Wi-Fi, and returns to the normal radio screen; short click must still cycle BAUD/BFO modes.


## FT8/FT4 switchable speaker monitoring — 2026-11-21
- Added short encoder click on the FT8/FT4 decoder page to toggle speaker output while the decoder continues running. The header shows `SPK ON` / `SPK OFF`; default is the pre-entry amplifier state (normally ON).
- The toggle controls only `PIN_AMP_EN` (external speaker amplifier). It deliberately does NOT call `muteOn()`, `rx.setAudioMute()`, alter receiver volume, or change GPIO11 ADC capture, because GPIO11 is the live FT8/FT4 decoder audio tap. This keeps the receiver volume boost at 50 and preserves the captured audio path while the speaker amp is disabled.
- On decoder exit, the amplifier enable state present on entry is restored; the existing original receiver volume restore remains intact. Long press still exits FT8/FT4. No decoder DSP, slot timing, shared audio capture, RTTY, TEF6686, or LittleFS logic changed.
- Source-only implementation on `development`; no build, Actions run, flash, or hardware verification. Verify that short click toggles speaker audibility without affecting decodes, and long press restores the original speaker/volume state.


## FT8/FT4 actions overlay — 2026-11-21
- Added an on-screen actions overlay opened by short encoder click while the FT8/FT4 decoder remains active. Encoder rotation selects items; click activates the selected item. Long press remains the decoder exit.
- Menu deliberately has no waterfall item because the current FT8/FT4 layout does not display a waterfall. Items implemented: SPEAKER ON/OFF (external amplifier only; GPIO11 ADC path and decoder remain live), AUTO SCROLL ON/OFF (freezes/resumes the visible recent-message list), CLEAR STATIONS (clears displayed messages and counters), BACK (closes overlay only), EXIT FT8 + FT4 (ends decoder and restores original amp/volume/network state).
- The overlay is rendered after the underlying decoder page; the decoder service and its capture/slot worker continue running. No DSP, slot timing, RTTY, TEF6686, or LittleFS changes.
- Noise filter and volume adjustment are not exposed yet: they need safe receiver/audio-path behavior validation so they cannot disturb the FT8 ADC capture. Decoder volume remains at the existing boost level 50.
- Source-only change on development; no build, Actions run, flash, or hardware verification. Confirm menu navigation, background decoding while open, speaker toggle, clear, auto-scroll freeze/resume, BACK, and EXIT.


## FT8/FT4 log and display row color — 2026-11-21
- User confirmed the live FT8 screen and speaker toggle work. The middle message row appeared bright green; all three message row backgrounds are now explicitly drawn in the same deep blue (`0x1036`) to remove the unintended green-looking alternating row.
- Added a switchable per-session FT8/FT4 text log to the actions overlay: LOGGING ON/OFF. Enabling starts a new file in LittleFS root; disabling stops appends. Leaving the decoder also stops the current session. Each enable creates a unique dated file such as `FT8_21.11.26.txt`, then `FT8_21.11.26_1.txt`, etc.; FT4 sessions use an `FT4_` prefix.
- Each successfully decoded message is appended with UTC timestamp, frequency, mode, country prefix/code when recognized, Maidenhead locator, distance from QTH `JO31TV` in km (or N/A), SNR, and decoded text. Unknown country prefixes are labelled Unknown; no guessed location/distance is written.
- Added authenticated web log listing at `/radio/ft8-logs` and individual downloads at `/radio/ft8-log?name=...`. The list only exposes FT8_/FT4_ .txt files; download names are validated to prevent path traversal. This uses existing LittleFS and existing web-server authentication; no formatting, erasing, or reinitialization of LittleFS was added.
- The logger opens/appends/closes the file per decoded message and flushes each line. No decoder DSP, slot timing, audio capture, RTTY, TEF6686, or existing storage preferences were changed.
- Source-only implementation on `development`; no build, Actions run, flash, or hardware test. Verify green row is gone, logging toggles, repeated same-day sessions get numbered files, decoded rows contain metadata, files persist after reboot, and downloads work while Wi-Fi/web server is active.


## FT8 log behavior clarification — 2026-11-21
- Log filenames use the configured local calendar date via `clockGetDate()`; each message line is timestamped in UTC. A new logging session on the same date receives the next available suffix (`_1`, `_2`, ...).
- Auto-scroll only controls the visible three-message history. The periodic result poll always runs, and successful decoded messages are appended to the active log even while AUTO SCROLL is OFF.
- The decoder intentionally shuts Wi-Fi down while active to protect timing/CPU headroom. Therefore log files are stored during reception and can be listed/downloaded after leaving FT8/FT4 and restoring the configured Wi-Fi mode, at `/radio/ft8-logs` (or `http://atsmini.local/radio/ft8-logs` when mDNS is available).
- No LittleFS format/erase/mount behavior was changed. No build or hardware test performed.


## FT8 preset submenu — 2026-11-21
- User explicitly requested FT8 presets inside the FT8 actions overlay only. The main Decoder Frequencies menu and its category/preset contents are not changed. CW and RTTY are out of scope and untouched.
- Added an `FT8 PRESETS` submenu to the FT8 actions overlay. It dynamically lists every entry tagged `DecoderKind::FT8` in the existing `src/radio/DecoderFrequencyList.h` category (10 entries): 160m 1.840, 80m 3.573, 60m 5.357, 40m 7.074, 30m 10.136, 20m 14.074, 17m 18.100, 15m 21.074, 12m 24.915 and 10m 28.074 MHz; all USB.
- Selecting a preset chooses the narrowest existing receiver band covering that frequency, applies its preset frequency, USB mode and bandwidth index, and returns to the FT8 decoder screen. The FT8 decoder/audio worker remains active; no decoder/DSP/slot logic was edited. The broad ALL-SW band is deliberately avoided when a dedicated narrower band matches.
- No extra 6m/2m frequencies were invented because the receiver's current bands stop at 30 MHz. Generic HF data-hunt, beacon, broadcast, RTTY, FAX and FT4 entries are not FT8 presets and were not imported. No changes to the main Decoder Frequencies menu.
- Only `ats-mini/src/ftx/FtxUi.cpp` changed for this feature. No build, Actions run, flash or hardware test performed. Verify preset scrolling, band selection/tuning, USB mode, return to active FT8 decoder and subsequent decoding.


## FT8/FT4 preset menu extension — 2026-11-21
- Extended the FT8 actions overlay with a second submenu entry, `FT4 PRESETS`, directly below `FT8 PRESETS`. FT4 items are sourced dynamically from the existing `DecoderFrequencyList.h` entries tagged `DecoderKind::FT4` (8 entries: 80m 3.575, 40m 7.048, 30m 10.140, 20m 14.080, 17m 18.104, 15m 21.140, 12m 24.919 and 10m 28.180 MHz; all USB).
- Added two FT8 quick-preset-only 11m CB channels: channel 24 at 27.235 MHz USB and channel 25 at 27.245 MHz USB. They are local to the FT8 submenu and deliberately do not alter the shared `DecoderFrequencyList.h` or the main `DECODER FREQUENCIES` menu.
- Both submenus use the existing preset application path: select the narrowest receiver band containing the frequency, apply USB and bandwidth index 4, and return to the active decoder screen. The decoder worker remains active; no FT8/FT4 DSP, audio capture, or slot-timing code changed.
- Changed only `ats-mini/src/ftx/FtxUi.cpp` for the implementation. Handoff documents updated. No build, Actions run, or flash was started; user performs the OSPI-DE build and hardware tests.
- Hardware checks: confirm the two new 11m entries tune to 27.235/27.245 MHz; test all 10 FT8 amateur-band presets and all 8 FT4 presets; verify USB/bandwidth and that reception continues after selecting any preset.


## FT8/FT4 preset delayed preview listening — 2026-11-21
- Added a 2-second idle delay to the FT8 and FT4 preset submenus. Opening a preset submenu starts the delay for the initially highlighted row; every encoder movement restarts the delay for the newly selected row.
- After 2 seconds without encoder movement, the receiver automatically tunes to the highlighted preset using the existing narrowest-band selection, USB mode and bandwidth index. The preset list displays `WAIT` while waiting and `LISTEN` once preview tuning has succeeded.
- A short encoder click commits the currently previewed frequency and closes the overlay. Clicking before the 2-second preview delay still applies the highlighted preset immediately, preserving the previous quick-select behavior.
- If the user rotates to another preset after a preview has started, the radio is first restored to the exact pre-submenu VFO state (band, frequency, mode, step, bandwidth and BFO); the new 2-second delay then begins. A long press/decoder exit during preview also restores that original radio state before leaving.
- If the external speaker amplifier was OFF before preview, it is temporarily enabled so the user can hear the signal, then restored to its prior state when the preview is committed, changed, or cancelled. The receiver volume boost and GPIO11 capture path remain unchanged.
- Preview rollback also restores the target band's previous saved frequency/mode/step/bandwidth, not just the original active VFO, so browsing another band does not leave behind a modified band-memory frequency. If the radio reports a tune mismatch, the provisional tune is rolled back as well.
- Implementation is isolated to `ats-mini/src/ftx/FtxUi.cpp`; no FT8/FT4 DSP, audio-capture, slot timing, RTTY, main Decoder Frequencies menu, TEF6686, or LittleFS code was changed.
- Source-only review. No build, GitHub Actions run, flash or hardware test was started. User should manually build OSPI-DE on `development` and verify initial auto-preview, timer restart while scrolling, audible speaker override/restore, click-to-commit, and long-press restoration.
- Commits: `e558f93714321ebab74f0fed7ae2710bc3a3a89a`, `c9f0d8b96aebf08f56c13057fa53b3bcafc49971`, `07f79ccc00169fbbfba9ad166274c6f78d150e6c`, `aac467d07b01c93133b92146b735099d09338834`, `8ab686ff8627ce888a60bbce5d97bffea36660a8`, `4fc3e4a2552dc94c5c451be8beef0bdaef22e143`.


## Hauptmenü: neue Reihenfolge und direkter FT8/FT4-Einstieg (2026-11-21)
- Hauptmenü auf exakt 22 Einträge umsortiert. Deutsche Reihenfolge: LAUTSTÄRKE, KEYPAD, BAND, MODULATION, BFO, STEPS, SUCHE, SCANNEN, SPEICHER, RAUSCHSPERRE, BANDBREITE, FREQUENZ-LINE, AGC/DÄMPFUNG, AUTO-LAUTSTÄRKE, SOFT-MUTE, DUAL RX, VFO A/B, VHF SPEKTRUM, ALL BAND SPEKTRUM, DECODER-FREQUENZEN, FT8 /FT4 DEKODER, EINSTELLUNGEN.
- Der neue Menüpunkt FT8 /FT4 DEKODER startet direkt den bestehenden FT8-Decoder. Im vorhandenen Actions-Untermenü bleiben FT8 PRESETS und FT4 PRESETS verfügbar; FT4 kann dort wie bisher ausgewählt werden.
- Alle bestehenden Menüaktionen wurden auf die neuen symbolischen Indizes umgestellt; MENU_SETTINGS ist jetzt Index 21. menuIdx startet weiterhin auf KEYPAD. Die englische Menüdatei erhielt dieselbe Struktur mit englischen Bezeichnungen.
- Geändert: ats-mini/Menu_german.cpp.src und ats-mini/Menu_english.cpp.src.
- Code-Commits: 43afc499742ee4a7c96d8fa3770a3e16676e4dd1 (Deutsch), e1ff8b954fd796a7e2475c89fe4c9b424f6a1717 (Englisch).
- Source-only; kein GitHub Actions Build, kein Flash und kein Hardwaretest durch den Assistant. Nutzer startet den OSPI-DE-Build manuell auf development.


## RTTY actions menu and SWL-friendly defaults — 2026-11-21
- Replaced the old short-click BAUD/BFO mode cycle with a dedicated RTTY actions overlay. Short click opens the menu; encoder rotation selects an item; click activates it. Long physical press still exits immediately through the existing handler in `ats-mini.ino`.
- Default on every RTTY entry is beginner-friendly: use the selected frequency preset's recommended baud rate, BFO AUTO, and polarity AUTO. Amateur RTTY presets keep 45.45 baud; DWD/Pinneberg RTTY presets keep their configured 50 baud. Manual changes are session-only; reopening a preset reapplies its recommended baud.
- Actions: BFO MODE toggles AUTO/MANUAL; when MANUAL is selected, encoder rotation on the decoder page adjusts BFO in 5 Hz steps, bounded to ±250 Hz from entry. BAUD RATE opens an edit state for 45.45 / 50 / 75 baud. POLARITY opens an edit state for AUTO / NORMAL / REVERSE. CLEAR TEXT clears displayed and queued decoded text. RESET DECODER resets decoder framing/state while retaining the current baud, polarity and detected tone pair. BACK closes the overlay; EXIT RTTY leaves the decoder.
- The existing automatic BFO correction target/deadband/step limits and original-BFO restoration on exit are retained. Receiver volume restoration and Wi-Fi/network restoration are unchanged.
- Tone-pair detection remains automatic. The actions overlay displays the currently detected MARK and SPACE frequencies (or `TONE PAIR: SEARCHING...`). There is no manual tone-frequency editor or true automatic baud-rate detector in the current decoder, so neither is presented as a feature.
- Changed only `ats-mini/src/rtty/RttyUi.cpp`, `RttyService.h`, and `RttyService.cpp`. FT8/FT4 files and DSP, the main Decoder Frequencies menu, TEF6686, LittleFS, and shared audio capture were not changed.
- Source-only changes on `development`. No compile, GitHub Actions run, flash, or hardware test was performed. User must manually build OSPI-DE on `development`; test preset baud defaults, manual baud and polarity edits, AUTO/MANUAL BFO behavior, clear/reset, BACK/EXIT, and restoration of BFO, volume and Wi-Fi.


## Direct RTTY decoder main-menu entry — 2026-11-21
- Added a standalone `RTTY DECODER` entry directly after `DECODER FREQUENCIES` and before `FT8 / FT4 DECODER` in both German and English main menus.
- Selecting it starts the existing RTTY decoder directly at the currently tuned receiver frequency, with 45.45 baud as the general non-preset default, BFO AUTO, and polarity AUTO. The RTTY actions menu remains available immediately after launch.
- Existing RTTY frequency presets continue to pass their own recommended baud rate (amateur RTTY 45.45, DWD/Pinneberg 50); the new direct entry does not change preset routing.
- Shifted the main-menu indices for FT8/FT4 and Settings by one position to make room. Existing symbolic menu indices and settings handlers were kept aligned in both language files.
- Changed only `ats-mini/Menu_german.cpp.src` and `ats-mini/Menu_english.cpp.src`. No FT8/FT4 decoder implementation, RTTY service/UI logic, TEF6686, LittleFS, or frequency preset definitions changed in this addition.
- Source-only on `development`; no compile, Actions run, flash, or hardware test was performed. Verify that the main-menu order is Decoder Frequencies → RTTY Decoder → FT8 / FT4 Decoder and that the direct entry starts RTTY at the current frequency.


## FT8/FT4 speaker mute correction — 2026-11-21
- User tested the previous Speaker OFF implementation and found that FT8/FT4 stopped decoding when the external amplifier was disabled. The earlier assumption that `PIN_AMP_EN` was independent of the GPIO11 audio tap was therefore incorrect for the user's hardware.
- Corrected `ats-mini/src/ftx/FtxUi.cpp`: during FT8/FT4, keep `PIN_AMP_EN` enabled so the GPIO11 capture path remains powered. Speaker ON/OFF now toggles only the active-high `AUDIO_MUTE` line (GPIO3 on ATS Mini), documented as the L/R audio mute. It does not call `rx.setAudioMute()` and does not change receiver volume or decoder capture.
- On decoder entry, preserve the original amplifier and hardware-mute states; restore both on exit. Preset preview temporarily unmutes only the L/R speaker output and restores the prior speaker state afterward.
- Changed only `ats-mini/src/ftx/FtxUi.cpp` plus these handoff notes. No FT8/FT4 DSP, slot timing, ADC capture service, RTTY, TEF6686, or LittleFS code changed.
- Source-only correction on `development`, commit `3d00b9b7a6019ab0556763d97b95467d14e4b819`. No build, flash, or hardware verification performed. User should test: Speaker OFF must silence the speaker while FT8 decode counters/messages continue; Speaker ON restores audio; exit restores the prior mute/amplifier state.


## DAB layout menu label (2026-11-21)
- The UI Layout menu entry at index 8 is now labeled simply **DAB** in German and English; the explanatory FM-only suffix was removed.
- The existing behavior is intentionally preserved: on FM the DAB Style renderer is used; on non-FM bands the existing TinyRadio fallback remains active.
- No renderer or fallback logic is changed by the label-only adjustment.


## DAB layout visual and logo corrections (2026-11-21)
- DAB layout accents now use RGB565 `0x001F`, matching the vivid blue main-menu frame instead of cyan.
- FM and PTY labels use centered text datum and the vertical center of their boxes. RDS RadioText is centered in its ticker frame.
- RDS clock moved down to the vertical center of the top status row. Wi-Fi indicator moved further left of the battery voltage/icon.
- Logo lookup now normalizes directory-entry paths to the `/Logos/<filename>` form before opening a PI-matched PNG. The existing LittleFS logo directory/migration logic is unchanged.
- Only `ats-mini/Layout-DABStyle.cpp` and these handoff notes were changed. Source-only; no build or device test performed. If logos still do not appear, check that the flashed LittleFS image contains PI-named PNG files in `/Logos/` and confirm the station's received PI code.

## DAB Style station logos — Mini_OS port (2026-11-21)
- Ported the Mini_OS `StationLogoClient` behavior from `firetvstick2k2-afk/Mini_OS`, branch `dev/first-build-preparation`, into ATS Mini development.
- Added `ats-mini/StationLogoClient.cpp/.h`: asynchronous FreeRTOS lookup task, mutex-protected RAM image buffer, PSRAM-first/internal-heap allocation, 220 KB image limit, PNG/JPEG signature validation, 5 s connect and 7 s request timeouts, strict redirects, and the same deliberately insecure TLS setting as Mini_OS.
- Lookup order: local PI match in `/logos/`; if absent and Wi-Fi is already connected, FM-DX Station Logo lookup for D-prefixed PI (program-suffixed PNG candidates from expected name, RDS station name and RadioText, then PI PNG/JPG/JPEG); finally Radio Browser Germany fallback with exact normalized station-name match and PNG/JPEG favicon.
- Successful HTTPS remote logos are cached in `/logos/<PI>_REMOTE.png|jpg`; image bytes are held in RAM for rendering. The DAB layout renders PNG/JPEG proportionally in the existing logo viewport and shows SEARCHING / NO LOGO / LOGO ERROR states.
- ATS Mini-specific directory rule confirmed by user: `/logos/` lowercase only. At every boot, `logosInit()` scans LittleFS root for PNG files, skips `splash.png` case-insensitively, and renames other root PNGs into `/logos/`. Existing destination files are not overwritten. The logo client itself searches only `/logos/` (no `/Logos` or root fallback).
- Startup initializes the logo client after LittleFS migration. The initial offline lookup can still load a local logo; if Wi-Fi becomes available afterward, the same station key is retried immediately rather than waiting for the 60-second online retry interval.
- Changed: `ats-mini/StationLogoClient.cpp/.h`, `ats-mini/Layout-DABStyle.cpp`, `ats-mini/Storage.cpp`, `ats-mini/ats-mini.ino`.
- Source-only port. No GitHub Actions build, flash, or hardware/network test was started. User should manually run the OSPI-DE build on `development`, then test local PNG/JPG/JPEG display, root PNG migration (including splash exclusion and duplicate handling), online FM-DX lookup, Radio Browser fallback, offline behavior, cache persistence and station changes.


## CW decoder port — 2026-11-21
- Ported the Mini_OS CW Goertzel DSP core from `firetvstick2k2-afk/Mini_OS`, branch `dev/first-build-preparation`, reference SHA `924bc6c6c38bf6eaa9eccc738b159b921cac7ab6`, into `ats-mini/src/cw/CwDecoder.h`.
- Ported `CwDecoderService` from Mini_OS reference SHAs `30af4436dc8ef5705db17d9848014885b2886b73` (header) and `8a2c29ae72cf38473fe214bd5a26d2fce9093dc9` (implementation). Added a mutex-protected tone-frequency setter and decoder reset.
- Added `CwUi.h/.cpp`: dedicated 320×170 decoder page, live text, RX/no-RX state, WPM, ADC level, default 700 Hz tone, selectable tone choices (400–900 Hz), actions for tone, clear text, reset, back and exit.
- The CW service subscribes to the existing 4 kSamples/s `AudioCaptureService`; it creates no additional ADC sampler. A singleton accessor was added and FT8/FT4 now use that same capture instance, allowing CW and FT8/FT4 to share the one GPIO11 capture task without running simultaneously.
- Added `DecoderKind::CW`, tagged all nine existing CW presets, routed CW preset selection into the CW page, and added direct CW Decoder menu entries in German and English. Added `CMD_CW_DECODER` and encoder rotate/click/long-press/tick/display dispatch.
- **Existing architecture discrepancy found during source review:** current `RttyService.cpp` still owns a separate 8 kSamples/s ADC capture task; it does not subscribe to the 4 kSamples/s `AudioCaptureService` despite the earlier handoff description. RTTY was deliberately left unchanged to avoid risking its working demodulator. CW itself adds no new sampler; CW and FT8/FT4 share the 4 kSamples/s singleton.
- Changed files: `ats-mini/src/cw/CwDecoder.h`, `CwDecoderService.h/.cpp`, `CwUi.h/.cpp`, `ats-mini/src/audio/AudioCaptureService.h/.cpp`, `ats-mini/src/ftx/FtxUi.cpp`, `ats-mini/src/radio/DecoderFrequencyList.h`, `ats-mini/Menu.h`, `ats-mini/Menu_german.cpp.src`, `ats-mini/Menu_english.cpp.src`, `ats-mini/Draw.cpp`, `ats-mini/ats-mini.ino`.
- Source-only implementation. No GitHub Actions build, local firmware build, flash or hardware test was run. User must build OSPI-DE and test CW reception, tone selection, WPM/level display, clear/reset, CW preset launch, direct menu launch, long-press exit, and restoration of configured Wi-Fi. Also regression-test FT8/FT4 and RTTY separately.
- Commits: `25775d1dfba9519ce04be67e05903340a5b4a54d`, `401eb98d99d5f2219e57496d8d64f9f4b9e44ab3`, `3dbdad4d9925ddd737aa6a18c2f1b55ebb5a965d`, `2d917e58a06943fcf39c68941a6ee169658da470`, `a764c87925b5e0387e234de4ebc5e962ad5f83fb`, `9ac9dc1633ff2c21c73b1d92802a64a2dc239a25`, `965adc959e29ddacf222934c4a6883fd01d3b4d8`, `bbed86448b7b99f61ca02df08d0cef07f32d312c`, `2559340c50e8444459ebc7346da96131a089222d`, `e970b9cf5a71e8e85e5a01ca687bfce9967e3ac2`, `cbda87aa0f2b404b69e34be4bae752644916f09f`, `cdbff5f06915a8934237464aa67b1f1da6dc5a92`, `9b98c518ce01848e2922f3ae69af4b4dad5b1538`, `04d595c9c5205d0dadf5e99a8dc438ff72ef911a`, `9de4ceda973e6e25c1060eaf74670a3bedfddd97`, `faff7cd6ff8695ae5511935d6a9240388d7f4a09f`, `256098b22d8572f5a1d4f309971f816f113f2610`, `866a7798908eafe729a34b03b9489b0b53b4e2a7`.


## RTTY quick presets submenu — 2026-11-21
- Added a dedicated **RTTY PRESETS** entry inside the already-running RTTY decoder's encoder-click action menu. This is separate from the top-level `Decoder Frequencies` browser; that global browser is not modified by this task.
- The submenu filters the shared `kDecoderFrequencyPresets` list by `DecoderKind::RTTY`. It exposes all 11 currently tagged RTTY entries: five amateur-band presets (80/40/20/15/10 m) and six DWD/Pinneberg RTTY frequencies (147 kHz DDH 47, 4583 DDK 2, 7646 DDH 7, 10101 DDK 9, 11039 DDH 9, 14467 DDH 8). Weather-FAX-only entries are not tagged RTTY and are intentionally excluded.
- Preset browser behavior follows FT8: encoder rotation changes the selection; after 2 seconds without further rotation, the receiver provisionally tunes to that preset and displays WAIT/LISTEN. The preview remains until another rotation or click. Rotating again restores the original VFO/band/BFO before scheduling the next preview. A short click commits the selected frequency, applies that preset's recommended baud (45.45 or 50 Bd), resets RTTY polarity to AUTO and resets the decoder. If clicked before the 2-second preview, it applies the selected preset immediately.
- During preview, speaker mute is temporarily released and the external amplifier is temporarily enabled when required; the prior speaker/amp state is restored on preview cancel or acceptance. Exiting the RTTY decoder while previewing also restores the original receiver state.
- RTTY action menu now uses the FT8 action-menu geometry, dark-blue panel (`0x1036`), cyan border (`0x07FF`), blue selection row (`0x0359`), white selected text and cyan headings. The RTTY decoder receive page itself is unchanged.
- Source-only change in `ats-mini/src/rtty/RttyUi.cpp`. No build, Actions run, flash or hardware test was started. User should run OSPI-DE build on `development` and test all 11 presets, 2-second preview, audible speaker audition, rotate-to-cancel/next, click-to-commit, baud selection (45.45/50 Bd), BFO restoration, and decoder exit. Re-test FT8/FT4 and CW for regressions.
- Code commits: `5941a11ea95a2c0f12ce0f8d28c030dda44036ba` (submenu/preview), `d9361fd80c40b7852e78780a3ceb7d2c4eaab1a3` (scope correction), `5c8cbdbdee7047b3875f9e28316578cbc8627b06` (speaker/amplifier preview handling), `b036dbcd234f7fc5577b46d7bfc4335a205fe19a` (preserve RTTY tone-pair status in the FT8-style action menu).

## RTTY 147 kHz low-band workaround and USB preset (2026-11-21)
- Added an experimental 147 kHz handling path only inside the running RTTY decoder's `RTTY PRESETS` submenu. The shared top-level `Decoder Frequencies` list and `DecoderFrequencyList.h` were deliberately left unchanged.
- The existing `PINNEBERG DDH 47` 147 kHz AM preset now selects the receiver's normal 150 kHz lower-edge band, temporarily lowers that band's minimum to 147 kHz, initializes the receiver at 147 kHz USB, then reselects the same band at 147 kHz AM. The original band minimum is restored immediately after initialization. This approximates the USB-then-AM workaround reported by the user; it is experimental and requires device validation.
- Added a separate local-only `PINNEBERG DDH 47 USB` 147 kHz USB preset immediately after the AM preset. Both presets use 50 baud and the existing two-second WAIT/LISTEN preview/commit flow. The extra USB entry is generated locally in `RttyUi.cpp` and does not change the shared frequency catalog or main menu.
- The preview rollback still restores the complete original target-band snapshot, including its original 150 kHz minimum. On commit, the temporary minimum is already restored.
- Changed only `ats-mini/src/rtty/RttyUi.cpp` for the implementation. No RTTY DSP/capture, other decoders, top-level Decoder Frequencies menu, FT8/CW, layouts, Wi-Fi, LittleFS or Web UI behavior was changed.
- Code commit: `6f041ec34c24f462e3a9ac1d3e8961800df6b62e`.
- Source-only change. No GitHub Actions run, build, flash or hardware test was started. User should manually build OSPI-DE on `development` and test both 147 kHz entries: confirm the AM entry actually produces audio after USB→AM initialization, confirm the USB entry can receive/decode RTTY, and verify preview cancellation restores the previous band and 150 kHz floor.

## FT8 country-code fallback styling (2026-11-21)
- In `ats-mini/src/ftx/FtxUi.cpp`, the fallback for countries without a drawn flag now paints over the generic flag outline and displays only the cyan two-letter country code (for example `PL`). No surrounding rectangle/badge remains.
- Actual drawn flags retain their existing border and appearance.
- Only the unknown-flag fallback rendering changed. No country mapping, message parsing, FT8 DSP, slot timing, or Actions menu behavior changed.
- Code commit: `5605a3b672421f6b450f326a4cb7cc53c93e25eb`. Source-only; no build or hardware test performed.



## FT8 offline signal-derived slot synchronization (21.11.2026)
- Added a persistent TIME SYNC choice to the FT8 Actions menu: ONLINE (existing UTC/NTP slot timing) or OFFLINE (slot phase recovered from successfully decoded FT8 transmissions). The selection is stored in ESP32 NVS using Preferences and survives decoder exit, firmware restart, and power-off. Default for devices without a saved preference is ONLINE.
- The setting is FT8-specific. FT4 keeps its existing UTC/NTP timing and still requires a valid system clock.
- Offline mode does not call NTP, does not require clockAvailable(), and keeps Wi-Fi off while the decoder runs, as before. It does not set or fake the system wall clock and cannot provide an absolute UTC date/time.
- Offline acquisition stores a rolling 15-second window of 4 kHz audio (60,000 int16 samples) in PSRAM and retries a signal scan every 2.5 seconds. A successful FT8 candidate supplies its measured symbol/time offset; that offset establishes the 15-second slot grid using the monotonic ESP timer. Normal per-slot decoding then follows that grid. After six consecutive empty slots, the decoder drops the lock and returns to rolling signal acquisition.
- The FT8 page displays SIG SCAN while searching and SIG LOCK after signal-derived synchronization. The Actions menu shows the saved online/offline choice. Switching modes while FT8 is open restarts the decoder service without leaving the page; selecting ONLINE requires an already-valid UTC clock. If UTC is not available, the selection is rejected with a status hint so the user can exit, restore Wi-Fi/NTP, and switch afterward.
- Offline messages carry a monotonic slot identifier and are never presented as UTC. UTC-based LittleFS logging is unavailable in offline mode; an active UTC log is stopped when switching to offline. Online logging behavior is unchanged.
- The FT8 country-code fallback frame removal remains intact.
- Source-only implementation on development. No build, GitHub Actions run, flash, or hardware test was performed. User must manually run the OSPI-DE build and test on the ATS Mini. Recommended checks: offline start with no UTC clock/Wi-Fi, wait for SIG LOCK and decode, verify re-acquisition after six empty slots, toggle modes, verify the choice survives decoder exit and full power cycle, and confirm FT4 still requires/uses UTC.
- Code commits: cb05e59bdbe85d5a95997f3341c1da0b2eeefc38, f4f8e85ac25d485610b6208f783a23fda5c2f35f, 7d0e62a1760e4ffa1074712ea46de3508150df76, 01b9b03b440e22455ef46357f7e9d5447829f96a, 236328d3b0008d71556175c5cb18534361013ffb, 8029b9a3c0cf6a21cf88306defce9d881180cc9e, b589bc7b3be69ee051ff26effb38d08ba87838c2, 88e5dec419bcc7af42d5c8453854308fa2626895, d9cd59c4911826b6c86b52afe7d3831d2477fce0, 0a4bd171c345d33150d5139f492e97ab63e59892, 7842c9dcc80387dcb52ba6ae8e76757c2a674ce7. Additional diagnostics and audio-gap recovery commits: b7e5ab7cea0435140dc4ac8f0fced0c17aa453c0, 44f5eb6c05c334cac318333450217fec823630e7.

- FT8 UI follow-up: moved the SPK ON/OFF indicator left from x=151 to x=126, closer to the frequency, and removed the SIG SCAN/SIG LOCK status text from the top row to avoid overlap. Offline synchronization remains active and selectable; only its redundant on-screen status label was removed. Code commit: 3f58403a146ae9232ab7ea18f5ecafd01edaabcb.


## RTTY 147 kHz removal and audio frame buffer fix (21.11.2026)
- User confirmed both Pinneberg DDH47 147 kHz RTTY presets (AM and the local USB variant) must be removed from the running RTTY decoder preset menu because neither starts/works on the receiver.
- Removed the local-only `PINNEBERG DDH 47 USB` entry and removed the shared 147 kHz AM preset's `DecoderKind::RTTY` classification. The 147 kHz AM receiver preset remains available in the general Decoder Frequencies browser as a receiver-only preset; it is no longer offered as an RTTY preset. Other Pinneberg RTTY presets remain unchanged.
- Removed the experimental 147 kHz band-floor override and USB-first/AM reinitialization workaround from `RttyUi.cpp`; all remaining RTTY presets now use the normal supported-band selection path.
- Corrected `RttyService.h` audio frame storage from 512 to 1024 floats, matching `kFrameSamples=1024` in `RttyService.cpp` and preventing out-of-bounds writes during tone analysis/decoding.
- Changed: `ats-mini/src/rtty/RttyUi.cpp`, `ats-mini/src/rtty/RttyService.h`, `ats-mini/src/radio/DecoderFrequencyList.h`.
- Source-only changes on `development`. No build, GitHub Actions run, flash, or hardware test was performed. User should run the OSPI-DE build and verify RTTY decoder startup plus all remaining amateur/DWD presets. The 147 kHz AM entry remains only as a general receiver preset and is not an RTTY decoder option.
- Code commits: c97847a6db4baf845934aa86689c453882238305, 2ea3ef75564f1b759f5f5d45e9da4c9b691a9b91, 2b8d510446b777511e1425f82cbe911f67978c75.


## Future feature: NAVTEX text receiver (planned after current ATS Mini Dev ports)
- User approved NAVTEX as a future standalone decoder feature to be ported only after the current ATS Mini Dev port work is completed. This is a handoff/planning item only; do not start implementation as part of the current ports.
- Target hardware remains ATS Mini V4 (ESP32-S3 + SI4732), using the existing supported 150 kHz–30 MHz receive range; no hardware modification is planned.
- UI is deliberately text-first: no waterfall or spectrum display. Show the incoming message as it is decoded, with new text appended and older text scrolling upward. Keep the page focused on readable message text and compact receiver/message status.
- Implement a separate NAVTEX/SITOR-B decoder path. NAVTEX is not plain RTTY: it requires SITOR-B/CCIR-476 character handling, synchronization and forward-error-correction (FEC), plus message framing and validation.
- Frequencies/presets requested: 518 kHz international NAVTEX (normally English), 490 kHz national/local-language NAVTEX, and 4209.5 kHz HF NAVTEX. Presets should be selectable from the NAVTEX Actions submenu; no separate manual Frequency item is currently required.
- Planned NAVTEX Actions submenu: START NAVTEX, STOP NAVTEX, NAVTEX PRESETS, MESSAGE LOG, CLEAR LOG, BACK. Presets contain the supported NAVTEX frequencies and their labels.
- Completed messages should be archived in LittleFS and remain readable after decoder exit/reboot. Live display and archive browsing are separate functions. Use message identifiers to detect duplicates; only mark a message complete after its NNNN end marker. Preserve/label incomplete receptions separately rather than presenting them as complete.
- Reuse existing receiver/audio infrastructure only after checking its sample-rate, bandwidth, capture ownership and lifecycle constraints. Do not assume the shared 4 kHz FT8/CW audio service or the separate 8 kHz RTTY capture is automatically suitable; do not modify existing CW, FT8/FT4 or RTTY behavior to make NAVTEX work.
- Keep the implementation isolated in NAVTEX-specific service, decoder and UI files, with localized German/English menu strings. No changes to TEF6686/Clean layouts or unrelated radio features.
- No code has been written, no build/Actions run, no flash and no hardware test performed. User will port/test after current ATS Mini Dev ports.


## JS8Call Receive-Only Decoder – added 2026-11-21

- New top-priority project: native JS8Call receive-only decoder; no transmit UI, encoder, PTT or TX path.
- Work only on `development`; leave `main` untouched. Do not compile or start Actions unless the user explicitly asks.
- Research/architecture plan: `JS8CALL_RECEIVE_DECODER.md`.
- Existing shared audio input: `ats-mini/src/audio/AudioCaptureService.h/.cpp`, GPIO11, 4 kHz, 24-sample blocks; JS8 should subscribe to this service and must not create a second ADC sampler.
- JS8 is related to FT8 but has its own 8-FSK synchronization/Costas patterns, channel decoding and six frame classes. Do not reuse FT8 payload parsing as if it were JS8.
- First milestone is JS8 NORMAL, then SLOW/FAST; JS8 40/60 only after performance validation. USB and accurate 15/10/6/30-second frame timing matter.
- Preserve existing FT8/FT4, CW, RTTY and all radio layouts/presets. Preserve the LILYGO GPIO11/SPI exclusion.
- Licensing: ATS Mini is MIT; JS8Call reference source is GPLv3. Do not directly copy upstream GPL source into this repo without an explicit licensing decision.
- No build or hardware test has been performed for JS8; user will perform firmware build and on-device reception tests.

## Korrigierte verbindliche Entwicklungsreihenfolge — 2026-11-21

**Korrektur nach ausdrücklicher Klarstellung des Nutzers:** JS8Call ist Priorität **Nr. 1**. Danach folgt FM DX. Diese Reihenfolge ersetzt alle älteren Prioritätslisten und die unmittelbar vorherige, fehlerhafte Reihenfolge in diesem Handoff.

1. **JS8Call – nativer Empfangsdecoder**
   - Ausschließlich Empfang: keine Sendefunktion, kein Encoder, keine PTT, kein TX-Menü und keine TX-Hardware.
   - Der technische Plan liegt in `JS8CALL_RECEIVE_DECODER.md`; die Firmware-Implementierung ist noch nicht begonnen.
   - Zunächst JS8 NORMAL, danach SLOW/FAST; JS8 40/60 erst nach Leistungsprüfung.
   - Bestehende Audio-Infrastruktur nach Prüfung verwenden und abonnieren, keinen zweiten ADC-Sampler starten. LILYGO-GPIO11/SPI-Ausschluss erhalten.
   - ATS Mini ist MIT-lizenziert, JS8Call-Referenzcode GPLv3. Keine direkte GPL-Codeübernahme in das Repository ohne vorherige Lizenzentscheidung; unabhängige Implementierung anhand der Protokolldokumentation bevorzugen.
   - FT8/FT4, CW, RTTY und bestehende Layouts schützen.
2. **FM DX**
   - Eigener FM-DX-Menümodus mit den tatsächlich vom SI4735 unterstützten Bandbreiten und AUTO.
   - Fokus auf bestmögliche Selektivität/Trennschärfe; keine nicht unterstützten TEF6686-Bandbreiten vortäuschen.
   - Im Anschluss separater Audio-FFT-Prototyp: weiße, schmale Peaks auf dunklem Hintergrund, schnelle Reaktion und weicher Abfall. Eigenes Testlayout, vorhandene Layouts unverändert lassen. Auch demoduliertes FM-Audio (Sprache/Musik) untersuchen. Erst den tatsächlichen Audio-Datenpfad prüfen und vorhandene Samples wiederverwenden, keinen zweiten ADC-Capture starten.
3. **Wetter portieren** – aus Mini_OS, Branch `dev/first-build-preparation`.
4. **Newsticker portieren** – aus Mini_OS, demselben Quellbranch.
5. **Mondphase portieren** – aus Mini_OS, demselben Quellbranch.
6. **NAVTEX** – eigener SITOR-B/CCIR-476-Empfangsdecoder mit Synchronisation, FEC, Nachrichtenrahmen, 518/490/4209.5-kHz-Presets, Live-Text und LittleFS-Archiv. Vollständige und unvollständige Nachrichten unterscheiden; Duplikate erkennen.
7. **Übrige Decoder** – danach WEFAX, DSC, SSTV, PSK31/63, Olivia/Contestia und weitere geeignete Empfangsmodi.

### Verbindliche Arbeitsregeln
- Ausschließlich `firetvstick2k2-afk/ats-mini-dev`, Branch `development`; `main` bleibt unangetastet.
- Keine NAVTEX- oder weiteren Decoderarbeiten vor den Punkten 1–5.
- Nutzer startet Builds/Actions manuell und testet auf dem ATS Mini V4. Assistant startet keine Builds/Actions und behauptet keine Hardwareprüfung.
- Nach jedem wesentlichen Arbeitsschritt beide Handoff-Dateien aktualisieren.


## JS8Call implementation preparation — 2026-11-21
- Rechecked the live `development` branch, `AGENTS.md`, development guide, Makefile, shared audio capture and FT8 engine/UI before continuing.
- Refined `JS8CALL_RECEIVE_DECODER.md` against the current JS8Call-improved User Guide. Important physical-layer detail: NORMAL uses its legacy-compatible triple Costas pattern; SLOW/FAST/JS8 40 use three distinct Costas arrays. All four established modes use 8 tones and 79 symbols. JS8 60 remains experimental and is deferred.
- Confirmed the official USB-only and timing requirement: the receive window is aligned to 15/10/6/30-second frame boundaries; an error greater than roughly two seconds can prevent decoding. No synthetic UTC or guessed timing will be used.
- Added the requested receive-screen behavior to the plan: line-by-line incoming text, automatic upward scrolling in live-follow mode, pause/resume and manual history scrolling. Reception and optional logging must continue independently of the visible scroll position.
- No JS8 firmware source has been added yet. This is protocol/UI design refinement only; the next implementation step is a clean-room NORMAL physical-layer decoder and independent frame/protocol parser, with reference-vector validation before display integration.
- Existing shared capture is 4 kHz / 24 samples per callback. The current FT8 engine uses a different monitor/payload pipeline; do not modify it or assume it is directly reusable. The ATS Mini Makefile uses wildcard source/header lists for `src/audio`, `src/ftx`, and `src/rtty`; add `src/js8` explicitly when new files are created.
- No compile, GitHub Actions run, flash, or hardware test was performed.


## JS8Call technical baseline update (2026-11-21)
- Continued JS8 receive-only planning on `development`; no firmware code, build, Actions run, flash or hardware test in this step.
- Verified against the current public JS8Call source: 79 channel symbols = 21 sync symbols (three 7-symbol Costas blocks) + 58 data symbols; LDPC(174,87); 87 information bits comprise 75 payload bits plus CRC-12; CRC polynomial 0xC06 with JS8 XOR value 42. CRC validation is required before payload parsing.
- Verified sample-rate constraint: upstream source processes 12 kHz, while ATS Mini `AudioCaptureService` supplies 4 kHz. Nominal 4 kHz symbol lengths are 640 samples NORMAL, 1,280 SLOW, 400 FAST and 200 JS8 40. These are planning calculations only; filter/passband, timing and frequency stability must be measured on actual captured reference signals before choosing DSP constants.
- AUTO/MULTI speed detection remains a planned feature after individual modes are validated. Accept a speed only after mode-specific synchronization, channel decode and CRC validation. Benchmark parallel mode decoding against a lower-cost candidate scheduler; do not overload the ESP32-S3.
- Source provenance was recorded in `JS8CALL_RECEIVE_DECODER.md`. Do not copy GPL-licensed JS8Call implementation code or tables into this MIT repository without an explicit licensing decision.
- Next technical gate: obtain/prepare legally usable JS8 NORMAL reference recordings and an independently generated host-side validation path; then implement and verify the physical layer before integrating UI or logging.
