# ATS Mini Dev — Übergabe für neuen Chat

Stand: 21.11.2026  
Repository: `firetvstick2k2-afk/ats-mini-dev` (privat)  
Arbeitsbranch: `development`  
Letzter Code-Commit: b036dbcd234f7fc5577b46d7bfc4335a205fe19a (RTTY preset preview submenu and action-menu polish).

## 1. Wichtige Arbeitsregeln
- **Bugfixes haben Vorrang:** Meldet der Nutzer während geplanter Feature-Arbeit einen Fehler, zuerst diesen Bug beheben und danach mit der zuletzt priorisierten Roadmap-Aufgabe fortfahren.

- Ausschließlich im privaten Repo `firetvstick2k2-afk/ats-mini-dev` auf Branch `development` arbeiten.
- `main` und das öffentliche Repo `13HN2709/ats-mini` niemals ändern, außer der Nutzer fordert es ausdrücklich.
- Vor Codeänderungen die aktuellen Dateien auf `development` lesen. Keine Änderungen aus alten Chat-Snippets blind übernehmen.
- Möglichst kleine, isolierte Änderungen. Bestehende funktionierende Features nicht unnötig refaktorisieren.
- **TEF6686-Layout ist das Lieblingslayout und bleibt unangetastet**, außer bei ausdrücklichem Auftrag.
- LittleFS-Mount-/Formatierungslogik ist funktionierend und geschützt. Keine Formatierung, kein Löschen und keine Änderung an der Mount-Strategie ohne ausdrücklichen Auftrag.
- Der Nutzer startet GitHub Actions manuell und flasht/testet das ATS Mini V4 selbst. Niemals Build- oder Hardware-Erfolg behaupten, bevor der Nutzer bzw. ein verlässlicher Lauf dies bestätigt.
- Nach jeder sinnvollen Änderung `DEVELOPMENT_HANDOFF.md` aktualisieren. Dieses Dokument ist die ausführliche technische Langzeitdokumentation.

## 2. Aktueller Stand / letzter Auftrag

### DAB Layout (21.11.2026)
- Layout-Menüeintrag heißt in Deutsch und Englisch nur noch **DAB**.
- Das bestehende Verhalten bleibt erhalten: FM zeigt DAB Style, Nicht-FM verwendet weiterhin den TinyRadio-Fallback.
- Es wurde ausschließlich die Menübezeichnung angepasst; die Fallback-Logik bleibt unverändert.



Der Nutzer bat um zwei Anpassungen, die bereits auf `development` committed sind:

1. **DAB Style als FM-only Layout:** Bei Auswahl von DAB Style (Index 8) wird auf UKW/FM DAB Style angezeigt; auf allen Nicht-FM-Bändern wird jetzt **TinyRadio** als alternatives Layout verwendet. Beim Zurückwechseln auf FM erscheint wieder DAB Style. Andere ausgewählte Layouts folgen weiterhin der normalen Layout-Auswahl.
2. **DAB Style Farben:** Alle vorher grünen Akzente wurden auf RGB565 `0x07FF` (knalliges Cyanblau) geändert. Dieser Farbwert entspricht dem kräftigen Blau des Außenrahmens im Hauptmenü. Die große Frequenzanzeige ist jetzt weiß (`0xFFFF)), passend zur Menüschrift.

Code-Commits:
- `63354f1c598fc6ad35e14a6676e6e2095011a455` — TinyRadio statt Clean als DAB-Fallback.
- `87a9c04ad1ff0710e68712f659c3e28a0ac5ac68` — DAB Style Farbanpassung.

Dokumentations-Commits:
- `5d6aa64953f890707a9f3090b46e3d353a42c606` — DAB-Änderungen im Handover ergänzt.
- `2daa5a1a6df3b34e76d9466f03df21c51bfde989` — aktueller Status und Chat-Übergabe ergänzt.

**Build-Status:** Der Nutzer hat angekündigt, den OSPI-DE-Build zu starten. Das Ergebnis liegt zum Zeitpunkt dieser Übergabe noch nicht vor. Im neuen Chat zuerst nach dem Build-Ergebnis fragen, nicht von einem erfolgreichen Build ausgehen.

## 3. Layouts in ATS Mini Dev

Layout-Indizes (bestehende Indizes erhalten):

| Index | Layout |
|---:|---|
| 0 | Original / Layout-Default |
| 1 | S-Meter |
| 2 | NewUI |
| 3 | Waterfall / Decoder |
| 4 | Clean |
| 5 | TEF6686 |
| 6 | TinyRadio |
| 7 | HJB |
| 8 | DAB Style (nur UKW) |

- Frische Installation: TEF6686 bleibt Standard (Index 5).
- DAB Style (Index 8): FM → DAB Style; Nicht-FM → TinyRadio.
- Alle anderen Layouts einschließlich TEF6686 verwenden normale Systemvorgabe.
- Keine bestehenden Layout-Indizes verschieben.

## 4. DAB Style / Logos

- Renderer: `ats-mini/Layout-DABStyle.cpp`; Routing in `ats-mini/Draw.cpp`; Datei in `ats-mini/Makefile` eingetragen.
- DAB Style zeigt Sendername, PI-zugeordnetes lokales PNG-Logo, Frequenz, MHz, PTY und RadioText.
- Logos werden aus LittleFS-Verzeichnis `/Logos/` geladen; Dateinamen werden anhand der vierstelligen RDS-PI gesucht.
- `logosInit()` in `Storage.cpp` legt `/Logos/` an und verschiebt PNGs aus dem LittleFS-Root dorthin, ausgenommen `/splash.png`. Bestehende Zieldateien werden nicht überschrieben; bei Kollision bleibt die Quelldatei liegen.
- Kein Netzwerk-Logoabruf. Nicht-FM braucht kein festes Senderlogo.
- Der aktuelle DAB-Code verwendet `0x07FF` für blaue Akzente und `0xFFFF` für die Frequenz.
- Hardwareprüfung nach dem Build: FM-Logo/PTY/RadioText, Wechsel FM → All Band/Nicht-FM → FM, TinyRadio-Fallback und Farbwirkung prüfen.

## 5. Build- und Release-Regeln

- GitHub Actions Workflow: `.github/workflows/build.yml`, manuell per `workflow_dispatch`.
- Für die laufende Entwicklung ist die Matrix bewusst auf **OSPI-DE** reduziert: Sprache `de`, Board `esp32s3-ospi`.
- Routine-Builds nicht auf QSPI/EN/IT ausweiten. Die frühere Sechsfach-Matrix ist in `DEVELOPMENT_HANDOFF.md` dokumentiert und kann bei einer ausdrücklich gewünschten Release-Vorbereitung wiederhergestellt werden.
- Nutzer startet den Build und meldet Ergebnis/Compilerfehler. Bei Fehlern nur anhand des konkreten Logs gezielt korrigieren.
- Ein früherer vollständiger Matrixlauf `44690333439` war erfolgreich, aber das ist kein Nachweis für die aktuellen DAB-/Decoder-Änderungen.

## 6. Weitere wichtige integrierte Funktionen

### Spectrum Scanner
- Separater nativer VHF-Scanner mit eigenem Eingabe-/Keypad-Flow und eigenem Sample-Buffer, getrennt vom Web Spectrum.
- VHF-Bereich 88–108 MHz, Start/Ende getrennt, Validierung; Peak-Auswahl und Klick zum Abstimmen.
- Dual RX und Web Spectrum sind geschützte Bestandsfunktionen.
- All Band Spectrum ist als eigener Bereich/Platzhalter vorgesehen; nicht versehentlich VHF-Scan starten.

### FT8/FT4
- FT8/FT4 Decoder-/Audio-/DSP-Quellen und UI-Service sind integriert; eigener Decoder-Bildschirm, Audio-Capture-Service, UTC-Gate, Wi-Fi-Aus-/Wiederherstellungslogik.
- FT8/FT4 öffnen den FT8/FT4-Decoder. CW startet den CW-Decoder; RTTY startet den RTTY-Decoder. RTTY besitzt im laufenden Decoder ein eigenes Preset-Untermenü (siehe unten).
- UTC kann offline über gültige RDS CT gesetzt werden; Decoder-Einstieg wartet auf `clockAvailable()`.
- Decoder-Waterfall ist noch nicht implementiert.
- Empfang/Decodierung auf echter Hardware ist nicht bestätigt. Timing, Audio-Pfad und UTC müssen nach erfolgreichem Build weiter geprüft werden.

### Decoder-Frequenzkatalog
- 11 Kategorien und 75 Presets sind importiert.
- Navigation: Kategorien → Presets; 2-Sekunden-Vorschau stimmt Frequenz ab.
- FT8/FT4-Presets öffnen den Decoder, normale Presets kehren zur aktiven Layoutanzeige zurück.
- Keine Presets stillschweigend entfernen oder die Quellanzahl auf 74 ändern.

## 7. Korrigierte verbindliche Entwicklungsreihenfolge — 2026-11-21

**Korrektur: JS8Call ist Priorität Nr. 1. Danach folgt FM DX.** Diese Reihenfolge ersetzt alle früheren Prioritätslisten.

1. **JS8Call – nativer Empfangsdecoder**
   - Ausschließlich Empfang: keine Sendefunktion, kein Encoder, keine PTT, kein TX-Menü und keine TX-Hardware.
   - Technischer Plan: `JS8CALL_RECEIVE_DECODER.md`. Planung liegt vor, Firmwarecode ist noch nicht implementiert.
   - Zuerst JS8 NORMAL, danach SLOW/FAST; JS8 40/60 erst nach Performanceprüfung.
   - Vorhandenen Audio-Datenpfad nach Prüfung abonnieren; keinen zweiten ADC-Sampler starten. LILYGO-GPIO11/SPI-Ausschluss erhalten. FT8/FT4, CW, RTTY und vorhandene Layouts schützen.
   - ATS Mini ist MIT-lizenziert, JS8Call-Referenzcode GPLv3. Keine direkte GPL-Codeübernahme ohne vorherige Lizenzentscheidung; unabhängige Implementierung anhand der Protokolldokumentation bevorzugen.
2. **FM DX** – eigener Menümodus mit den real unterstützten SI4735-Bandbreiten und AUTO, Fokus auf Selektivität. Keine nicht unterstützten TEF6686-Bandbreiten vortäuschen.
   - Danach separater Audio-FFT-Prototyp als eigenes Testlayout: weiße schmale Peaks, schnelle Reaktion, weicher Abfall; auch demoduliertes FM-Audio untersuchen. Erst den echten Audio-Datenpfad prüfen und vorhandene Samples nutzen, keinen zweiten ADC-Capture starten.
3. **Wetter portieren** – aus Mini_OS, Branch `dev/first-build-preparation`.
4. **Newsticker portieren** – aus Mini_OS, demselben Quellbranch.
5. **Mondphase portieren** – aus Mini_OS, demselben Quellbranch.
6. **NAVTEX** – eigener SITOR-B/CCIR-476-Decoder mit FEC, Synchronisation, Nachrichtenrahmen, 518/490/4209.5-kHz-Presets, Live-Text und LittleFS-Archiv; vollständige/unvollständige Nachrichten unterscheiden und Duplikate erkennen.
7. **Übrige Decoder** – danach WEFAX, DSC, SSTV, PSK31/63, Olivia/Contestia und weitere geeignete Empfangsmodi.

Ausschließlich im privaten Repo `firetvstick2k2-afk/ats-mini-dev`, Branch `development` arbeiten; `main` bleibt unangetastet. Keine NAVTEX- oder weiteren Decoderarbeiten vor Abschluss von JS8Call, FM DX, Wetter, Newsticker und Mondphase. Nutzer startet Builds/Actions manuell und testet selbst; keine unbestätigten Build- oder Hardware-Erfolge behaupten.

## 9. Ton und Zusammenarbeit

Der Nutzer arbeitet aktiv mit GitHub, lässt Builds manuell laufen und testet auf dem ATS Mini V4. Er erwartet konkrete Änderungen im Repo, wenn er „übernehmen“ oder „bauen“ sagt, sowie eine ehrliche Aussage dazu, was wirklich committed, gebaut oder auf Hardware geprüft wurde. Antworten auf Deutsch, freundlich und direkt. Vorhandene Informationen aus diesem Dokument nicht erneut abfragen.


## 10. Build-Fehlerbehebung und Workflow — 2026-11-20
- Nutzer bestätigt ausdrücklich: gearbeitet wird ausschließlich auf Branch `development`; nicht auf `main`.
- Fehlgeschlagener Run #3 (Run-ID `44800260714`) wurde auf `development` ausgeführt. Compilerfehler: `Layout-DABStyle.cpp` kann `getCurrentBand`, `CMD_NONE`, `CMD_SCAN`, `CMD_FREQ` und `drawSideBar` nicht auflösen; außerdem LovyanGFX `drawPngFile(LittleFS,...)` in Sprite wegen abstrakter `DataWrapperT<LittleFSFS>`.
- Fix committed: `ats-mini/Layout-DABStyle.cpp` bindet `LittleFS.h` vor LovyanGFX-/Projekt-Headern ein, inkludiert `Menu.h` für Band-/Command-/Sidebar-Deklarationen und decodiert PNG-Logos über einen geöffneten LittleFS-`File`-Stream mit `spr.drawPng(&logoFile,...)`, statt der nicht kompatiblen FS-Überladung.
- Workflow `.github/workflows/build.yml` auf `development` vereinfacht: nur OSPI-DE + Deutsch; keine OTA-Metadatenprüfung, keine OTA-Datei, keine Partitions-/Bootloader-Uploads, kein Sketch-Report und kein ZIP-Archiv. Ein einzelnes, nicht archiviertes Firmware-Artefakt `Mini_OS_#${{ github.run_number }}.bin` (`archive: false`, direkter BIN-Download); bei explizitem Release-Tag wird die rohe BIN zusätzlich als GitHub-Release-Asset hochgeladen.
- Commits: Code `188f0cb7ba75b99b8cb6c7a7cc160d0396f04d1c`; Workflow `9b38c9c954999269e00cb16a59014975a6e0c4a4`.
- Nach dem Commit wurden beide Dateien auf `development` erneut abgerufen und die Änderungen verifiziert. Kein Build, keine Actions und kein Flash durch den Assistant. Der korrigierte Code ist noch nicht compiler-verifiziert; Nutzer soll den nächsten Build manuell starten.

## 10. Future idea: NAVTEX — firmware only, no hardware mods
- User wants NAVTEX support explored later only if it works on the stock ATS Mini V4. No soldering, extra microphone, external sound card, or board modification.
- User has successfully tested CW, RTTY, and FT8 decoding on the unmodified V4 using HJ Berndt and Diamond Radio firmware. This confirms the existing digital-audio path is available, but does not by itself prove SITOR-B compatibility.
- NAVTEX needs a dedicated SITOR-B decoder with FEC; ordinary RTTY decoding is not sufficient to assume NAVTEX support.
- Future investigation: reuse the existing internal audio/DSP path, isolate a SITOR-B/FEC decoder from the existing CW/RTTY/FT8 functions, and display received NAVTEX messages on-screen. Start by investigating 518 kHz; other NAVTEX frequencies can be considered later.
- This is backlog only, not implemented or build-tested. Do not begin until the user reopens the topic and the agreed core stability/storage priorities are addressed. If firmware-only decoding proves impractical on stock hardware, stop without proposing hardware modifications.

## 11. FT8 reset diagnosis (2026-11-20)
- Android USB serial capture works at 115200 8N1; PC is not required.
- Exact crash: `Stack canary watchpoint triggered (ftx-decoder)` on Core 1. Latest run completed two 93/93 FT8 slots, then reset during the third. User recalls an earlier reset around block 91/95 on the first pass.
- Source finding: FT8 worker stack was 8192 bytes; `decodeSlot()` and nested FT8/LDPC decode use local stack buffers.
- First diagnostic patch committed on `development`: worker stack raised to 16384 bytes; free stack high-water mark logged before/after each decode. Commit `21d6db3756f1d7332eb4f48d7da2c83dd541ba21`.
- Build/device test pending. Do not call this fixed until several slots have been tested. Watch `[FTX][STACK]` serial lines.

## Verbindliche Portierungsquelle — Mini_OS → ATS Mini Dev (2026-11-20)
- Alle relevanten übernommenen Funktionen und Layouts portieren wir aus `firetvstick2k2-afk/Mini_OS`, Branch **`dev/first-build-preparation`** (exakter Branchname; der Nutzer bezeichnete ihn versehentlich als „first-build-reparation“).
- Ziel ist `firetvstick2k2-afk/ats-mini-dev`, Branch **`development`**. Ausschließlich dort Änderungen vornehmen.
- Beispiele bereits portierter Mini_OS-Bestandteile: **DAB Style**, **FT8-Decoder** und weitere importierte Funktionen. Mini_OS ist die Quell- und Referenzbasis.
- Bei jeder weiteren Portierung zuerst die Implementierung im Mini_OS-Quellbranch ansehen, dann mit dem ATS-Mini-Dev-Stand vergleichen und die Integration gezielt fertigstellen.
- Nicht annehmen, dass ein importiertes Feature automatisch vollständig integriert ist: Status pro Feature als vollständig / teilweise / offen festhalten.
- `main` im Zielrepo und das öffentliche ATS-Mini-Repository nicht verändern, außer auf ausdrückliche Anweisung.


## FT8-Anzeige — 20.11.2026
- Die FT8/FT4-Decoderseite wurde optisch neu aufgebaut; **CW wurde weiterhin nicht importiert**.
- Oben links steht die Frequenz, oben rechts `DEC n ERR n` (Nachrichtenanzahl und aufsummierte LDPC-Fehler der erfolgreich dekodierten Nachrichten).
- Darunter läuft ein cyanfarbener Fortschrittsbalken anhand der empfangenen Decoder-Blöcke des Slots.
- Die unteren drei großen Zeilen zeigen die letzten empfangenen Nachrichten auf abwechselnden dunklen/mittleren Blautönen. Links erscheint eine Länderflagge, wenn das Rufzeichenpräfix erkannt wird; rechts die Entfernung.
- Entfernung: Maidenhead-Locator aus der Nachricht gegen den QTH-Locator `JO31TV`. Bei fehlendem/ungültigem Locator steht `---`. Unbekannte Rufzeichenpräfixe bekommen bewusst keine erfundene Flagge.
- Die Präfix-/Flaggenzuordnung deckt derzeit häufige Länder ab, ist aber noch keine vollständige DXCC-Datenbank.
- Nur `FtxUi.cpp` wurde für die Darstellung und die Anzeigehistorie geändert; Decoder-DSP, Audio-Capture und Slot-Timing blieben unverändert. Die ERR-Zahl zählt nur LDPC-Fehler, die an erfolgreich dekodierten Nachrichten gemeldet wurden, nicht alle fehlgeschlagenen Kandidaten.
- Commits: `c2af7005bc4c70a4a7c36821a83f1fc020a1288f`, `2eac7c1ea2a11a7fc23313db7bf76a0a96bb911a`.
- Noch kein Build und kein Hardwaretest. Als Nächstes OSPI-DE manuell auf `development` bauen und anschließend Darstellung, Locator-Distanz, Flaggen, Ladebalken sowie FT8/FT4 prüfen.


## FT8: Audio boost and persistent messages — 20.11.2026
- User reports the decoder has now successfully decoded two FT8 messages on hardware.
- On entry to FT8/FT4, receiver volume is temporarily raised to at least 50/63 (unless already louder) so monitoring is audible; the original volume is restored when leaving the decoder. No digital gain is applied to the captured samples or decoder DSP.
- Empty decode slots no longer overwrite the last successful published result set with an empty array. The three-message UI history therefore retains prior successful messages while later slots fail to decode.
- Commits: `4e6d9c8caf325e336cf32ef56e36b484dc442c48` and `c0b3a3111d76410bb27bfff7dc5a1c635ee95d0d`.
- Not yet built or hardware-tested; user starts OSPI-DE manually on `development`. Verify louder monitoring, restore of previous volume, and message persistence after an empty slot.


## FT8/FT4 decoder display polish — 21.11.2026
- Unmapped country flags now appear as dark-blue badges with cyan borders and bold cyan country-code text (e.g. `CN`) instead of subdued gray-looking labels.
- The empty message-history placeholder is now bold cyan: `Waiting for FT8/FT4 decodes...`.
- UI-only change in `ats-mini/src/ftx/FtxUi.cpp`; decoder/audio/timing logic unchanged.
- Commit: `6bec9ae62e3ef7858b6ebf513fc694e463a24c8d`. Build and device test pending; user runs OSPI-DE manually on `development`.


## Aktueller RTTY-Port-Stand (2026-11-21)
- Nutzer hat den bisherigen ATS-Mini-RTTY-Decoder als nicht funktionsfähig verworfen und einen sauberen Neuimport des MIT-Decoders von `joaquimorg/ham_decoder` verlangt.
- Neu importiert nach `ats-mini/src/rtty/`: `RttyDecoder.cpp`, `RttyDecoder.h`, `LICENSE.md`. DSP-Rate isoliert auf 4 kHz gesetzt, passend zum bestehenden GPIO11-Audio-Capture. Makefile nimmt `src/rtty/*.cpp/.h` auf.
- Commits: `52632c6318c81df19a57eef037d0618ef445a6ee`, `e398a85f65ceeb037d82423c27adfa70a106e9e6`, `54f312dd6ee40cf254f67dc68bde6320e6c11043`, Makefile `e9f4c7e43d0a6d0359a8f3796808f608098ee871`, DEVELOPMENT_HANDOFF `7ab0a523641d6db91df1b8627cd777b9548a6219`.
- **Noch nicht fertig / nicht als funktionierend bezeichnen:** FSK-Tonpaar-Erkennung und -Tracking, Audio-Service-Anbindung, RTTY-Seite mit CW-Stil und vier Textzeilen, Frequenz links oben / Empfangsstatus rechts oben, Menüintegration.
- **FT8/FT4 bleibt strikt eingefroren.** Keine Änderungen unter `ats-mini/src/ftx/`; keinerlei Änderung an FT8/FT4-DSP, Audio-Capture, Slot-Timing, UI oder Einstellungen. RTTY getrennt halten.
- Ziel ausschließlich `firetvstick2k2-afk/ats-mini-dev` Branch `development`. Mini_OS und Joaquim-Repository nur als Quelle lesen. Nutzer führt Builds und Hardwaretests selbst aus; für den aktuellen Import wurde kein Build/Actions-Run/Flash durchgeführt.


## RTTY decoder — current continuation status (2026-11-21)
- Imported RTTY engine is present under `ats-mini/src/rtty/`; corrected its include to the actual case-sensitive filename `RttyDecoder.h` in commit `bb2e9cfff95596d6c4f00a4c0e140639c7b3a11d`.
- Still not a usable on-air decoder: two-tone FSK detection/tracking, isolated audio consumer/service, menu wiring and CW-style four-row UI remain outstanding.
- Preserve FT8/FT4 completely. No files under `ats-mini/src/ftx/` may be changed for this work.
- The exact CW reference UI has not yet been found in the available Mini_OS branches and guessed paths. Locate the true source before implementing the matching RTTY UI; do not approximate it with the generic decoder layout.
- No build or device test has been run.


## RTTY decoder — latest implementation state (2026-11-21)
- Exact CW visual reference recovered: `Mini_OS/firmware/ats-mini-os/src/display/DisplayDriver.cpp`, `drawLegacyCWPage()`, branch `dev/first-build-preparation`.
- RTTY screen now uses the same top frequency/status bar and four deep-blue text rows. Encoder rotates baud 45.45 / 50 / 75; click or long press exits.
- RTTY uses its own GPIO11 capture at 8 kHz and a separate worker/tone estimator. This is deliberately separate from the 4 kHz shared FT8/FT4 capture. No files under `ats-mini/src/ftx/` were changed for RTTY.
- Five amateur RTTY presets plus Pinneberg DDH/DDK and DWD FAX 3.855 are routed to RTTY. Weather-fax entries remain listen-only.
- RTTY frequency routing, UI, and service are committed on `development`. No build, Actions run, flash, or RF test has been completed. Workflow dispatch was not available through the connected GitHub actions in this session; user should manually start OSPI-DE build on `development`.
- First build candidate: Goertzel FSK tone pair detection (300–3500 Hz), automatic polarity, manually selectable 45.45/50/75 baud. Hardware/RF validation remains essential; do not describe decoding as proven until tested.
- Preserve FT8/FT4, TEF6686, LittleFS and all other protected areas.


## Correction: Pinneberg weather fax vs RTTY (2026-11-21)
- Corrected `DecoderFrequencyList.h`: 3.855 MHz (DDH3) is FAX, not RTTY. The preset is now `DWD FAX 3.855` with no RTTY decoder route.
- 7.880 MHz (DDK3) and 13.8825 MHz (DDK6) are also FAX and remain outside the RTTY decoder.
- Pinneberg RTTY presets are the text services: 147.3, 4583, 7646, 10100.8, 11039 and 14467.3 kHz. RTTY 147.3 kHz uses a narrower 85 Hz shift and needs a suitable detector configuration/test.
- Source checked: DWD maritime weather service frequency table and DWD RTTY transmission plan. Keep weather fax and RTTY as distinct decoder types.


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
- Implementation is isolated to `ats-mini/src/ftx/FtxUi.cpp`; no FT8/FT4 DSP, audio-capture, slot timing, RTTY, main Decoder Frequencies menu, TEF6686, or LittleFS code was changed.
- Source-only review. No build, GitHub Actions run, flash or hardware test was started. User should manually build OSPI-DE on `development` and verify initial auto-preview, timer restart while scrolling, audible speaker override/restore, click-to-commit, and long-press restoration.

- Preview rollback also restores the target band's previous saved frequency/mode/step/bandwidth, not just the original active VFO, so browsing another band does not leave behind a modified band-memory frequency. If the radio reports a tune mismatch, the provisional tune is rolled back as well.
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


## DAB layout fine-tuning (21.11.2026)
- Main-menu neon-blue frame color matched with RGB565 `0x001F` throughout the DAB layout.
- FM, PTY and RDS RadioText centered in their respective frames; RDS clock lowered; Wi-Fi icon moved left to avoid battery-voltage overlap.
- The initial local-only PNG lookup was superseded by the Mini_OS StationLogoClient port documented below; the final directory is lowercase `/logos/` with boot migration and online fallback.
- Changed only `ats-mini/Layout-DABStyle.cpp` plus handoff notes. Build/hardware validation pending.

## DAB Style station logos — Mini_OS port (2026-11-21)
- Ported the Mini_OS `StationLogoClient` behavior from `firetvstick2k2-afk/Mini_OS`, branch `dev/first-build-preparation`, into ATS Mini development.
- Added `ats-mini/StationLogoClient.cpp/.h`: asynchronous FreeRTOS lookup task, mutex-protected RAM image buffer, PSRAM-first/internal-heap allocation, 220 KB image limit, PNG/JPEG signature validation, 5 s connect and 7 s request timeouts, strict redirects, and the same deliberately insecure TLS setting as Mini_OS.
- Lookup order: local PI match in `/logos/`; if absent and Wi-Fi is already connected, FM-DX Station Logo lookup for D-prefixed PI (program-suffixed PNG candidates from expected name, RDS station name and RadioText, then PI PNG/JPG/JPEG); finally Radio Browser Germany fallback with exact normalized station-name match and PNG/JPEG favicon.
- Successful HTTPS remote logos are cached in `/logos/<PI>_REMOTE.png|jpg`; image bytes are held in RAM for rendering. The DAB layout renders PNG/JPEG proportionally in the existing logo viewport and shows SEARCHING / NO LOGO / LOGO ERROR states.
- ATS Mini-specific directory rule confirmed by user: `/logos/` lowercase only. At every boot, `logosInit()` scans LittleFS root for PNG files, skips `splash.png` case-insensitively, and renames other root PNGs into `/logos/`. Existing destination files are not overwritten. The logo client itself searches only `/logos/` (no `/Logos` or root fallback).
- Startup initializes the logo client after LittleFS migration. The initial offline lookup can still load a local logo; if Wi-Fi becomes available afterward, the same station key is retried immediately rather than waiting for the 60-second online retry interval.
- Changed: `ats-mini/StationLogoClient.cpp/.h`, `ats-mini/Layout-DABStyle.cpp`, `ats-mini/Storage.cpp`, `ats-mini/ats-mini.ino`.
- Source-only port. No GitHub Actions build, flash, or hardware/network test was started. User should manually run the OSPI-DE build on `development`, then test local PNG/JPG/JPEG display, root PNG migration (including splash exclusion and duplicate handling), online FM-DX lookup, Radio Browser fallback, offline behavior, cache persistence and station changes.


## CW-Decoder-Port (21.11.2026)
- Neuer eigener CW-Decoder auf `CMD_CW_DECODER`, direktes Hauptmenü in Deutsch/Englisch und Start aus allen neun CW-Presets in Decoder Frequencies.
- Mini_OS Goertzel-DSP-Kern übernommen; Service verwendet Subscriber am bestehenden GPIO11 AudioCaptureService mit 4.000 Samples/s. FT8/FT4 verwenden nun denselben Singleton-Capture-Service. Es wird kein zusätzlicher CW-ADC-Sampler erzeugt.
- Eigene 320×170 CW-Seite mit RX/SIGNAL, WPM, Audiopegel, Tonfrequenz (Standard 700 Hz), einstellbaren Tönen sowie Aktionen CLEAR TEXT, RESET DECODER, BACK und EXIT CW.
- Encoder: drehen navigiert/ändert Ton im Aktionenmenü; kurzer Klick öffnet/bestätigt; langer Druck beendet CW und stellt die zuvor konfigurierte WLAN-Betriebsart wieder her.
- Wichtiger Quellcodebefund: `RttyService.cpp` besitzt aktuell weiterhin einen eigenen 8-kSamples/s ADC-Capture-Task. Er wurde absichtlich nicht verändert, um RTTY nicht zu gefährden. Die frühere Annahme, RTTY nutze bereits den gemeinsamen 4-kSamples/s Subscriber, stimmt mit dem tatsächlich abgerufenen `development`-Code nicht überein.
- Keine Actions, kein Build, kein Flash und kein Hardwaretest durch ChatGPT. Nachfolgend Nutzer-Build und Gerätetest erforderlich.
- Relevante Code-Commits: `25775d1dfba9519ce04be67e05903340a5b4a54d`, `401eb98d99d5f2219e57496d8d64f9f4b9e44ab3`, `3dbdad4d9925ddd737aa6a18c2f1b55ebb5a965d`, `2d917e58a06943fcf39c68941a6ee169658da470`, `a764c87925b5e0387e234de4ebc5e962ad5f83fb`, `9ac9dc1633ff2c21c73b1d92802a64a2dc239a25`, `965adc959e29ddacf222934c4a6883fd01d3b4d8`, `bbed86448b7b99f61ca02df08d0cef07f32d312c`, `2559340c50e8444459ebc7346da96131a089222d`, `e970b9cf5a71e8e85e5a01ca687bfce9967e3ac2`, `cbda87aa0f2b404b69e34be4bae752644916f09f`, `cdbff5f06915a8934237464aa67b1f1da6dc5a92`, `9b98c518ce01848e2922f3ae69af4b4dad5b1538`, `04d595c9c5205d0dadf5e99a8dc438ff72ef911a`, `9de4ceda973e6e25c1060eaf74670a3bedfddd97`, `faff7cd6ff8695ae5511935d6a9240388d7f4a09f`, `256098b22d8572f5a1d4f309971f816f113f2610`, `866a7798908eafe729a34b03b9489b0b53b4e2a7`.


## RTTY-Preset-Untermenü im laufenden Decoder (21.11.2026)
- Beim kurzen Encoderklick auf der RTTY-Empfangsseite öffnet sich `RTTY ACTIONS`; erster Eintrag ist `RTTY PRESETS`. Das ist ausdrücklich nicht der Hauptmenüpunkt `Decoder Frequencies`.
- Die Liste zieht alle 11 mit `DecoderKind::RTTY` markierten Einträge aus `DecoderFrequencyList.h`: fünf Amateurfunk-Presets (80/40/20/15/10 m) und sechs DWD/Pinneberg-RTTY-Frequenzen (147, 4583, 7646, 10101, 11039, 14467 kHz). Reine DWD-FAX-Presets ohne RTTY-Markierung bleiben außen vor.
- Bedienung wie FT8: drehen wählt, nach 2 Sekunden wird die Frequenz probeweise abgestimmt (WAIT → LISTEN), erneutes Drehen stellt die vorherige Frequenz wieder her und wechselt zur nächsten Vorauswahl. Kurzer Klick übernimmt die Frequenz und setzt die zum Preset gehörige Baudrate (45.45/50 Bd), AUTO-Polarität und Decoder-Reset. Klick vor Ablauf der 2 Sekunden übernimmt direkt.
- Während der Vorschau werden Lautsprecher-Mute und Verstärker nur falls nötig temporär aktiviert und anschließend auf den vorherigen Zustand zurückgesetzt. Beim Verlassen während einer Vorschau wird auch der ursprüngliche VFO/BFO wiederhergestellt.
- Nur das RTTY-Aktions-/Preset-Untermenü wurde optisch an FT8 angepasst; die RTTY-Empfangsseite bleibt unverändert.
- Quellcode-Commits: `5941a11ea95a2c0f12ce0f8d28c030dda44036ba`, `d9361fd80c40b7852e78780a3ceb7d2c4eaab1a3`, `5c8cbdbdee7047b3875f9e28316578cbc8627b06`, `b036dbcd234f7fc5577b46d7bfc4335a205fe19a`.
- Build und Hardwaretest stehen noch aus. Nächster Schritt: manueller OSPI-DE-Build auf `development`; anschließend alle 11 Presets, 2-s-Vorschau, Audio, Auswahl/Abbruch, Baudrate, BFO- und Exit-Wiederherstellung testen.

## RTTY 147 kHz workaround / additional USB preset (21.11.2026)
- Latest code commit on `development`: `6f041ec34c24f462e3a9ac1d3e8961800df6b62e`.
- In the existing running RTTY decoder action submenu, `PINNEBERG DDH 47` at 147 kHz AM now uses an experimental USB-first, then AM reinitialization sequence. The selected receiver band's lower limit is temporarily set to 147 kHz only while the normal band initialization runs; the original 150 kHz limit is restored immediately afterward.
- Added `PINNEBERG DDH 47 USB` as a separate local-only RTTY preset directly after the 147 kHz AM entry. It uses 50 baud and the same two-second preview/commit behavior.
- Important scope: the USB preset is defined locally in `RttyUi.cpp`; `DecoderFrequencyList.h` and the top-level `Decoder Frequencies` browser are unchanged.
- Both 147 kHz paths are experimental and unverified on hardware. No build, Actions run or flash was performed. Next step is the user's manual OSPI-DE build on `development`, then test 147 kHz AM audio after USB→AM switching and RTTY decoding on 147 kHz USB. Also verify preview rollback restores the original band state and 150 kHz minimum.

## FT8 country-code fallback (21.11.2026)
- Latest UI styling commit: `5605a3b672421f6b450f326a4cb7cc53c93e25eb`.
- Country codes shown when no flag graphic exists (e.g. `PL`) are now plain cyan two-letter text without the surrounding frame. Actual flag graphics keep their prior styling.
- Changed only the fallback drawing branch in `ats-mini/src/ftx/FtxUi.cpp`; no FT8 decoding/timing behavior changed. No build or hardware test performed.



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


## JS8Call – aktueller Arbeitsstand (21.11.2026)
- Weiterhin höchste Feature-Priorität: nativer JS8Call-Empfangsdecoder, ausschließlich RX. Kein TX, kein Encoder, keine PTT und keine Sendefunktion.
- `JS8CALL_RECEIVE_DECODER.md` wurde anhand des aktuellen JS8Call-improved User Guide präzisiert: 8-FSK, 79 Symbole; NORMAL 15 s / 12,64 s Signal / 6,25 baud / 50 Hz; SLOW 30 s; FAST 10 s; JS8 40 6 s; JS8 60 ist experimentell und wird zurückgestellt.
- NORMAL besitzt ein eigenes legacy-kompatibles Triple-Costas-Synchronisationsmuster. SLOW/FAST/JS8 40 verwenden drei unterschiedliche 7×7-Costas-Blöcke. JS8-Frame-/Payload-Parsing ist separat von FT8 zu implementieren.
- USB und Slot-Synchronisation sind wesentlich. Mehr als ungefähr zwei Sekunden Zeitfehler können Decodierung verhindern. Keine erfundene UTC-Zeit und kein vorgetäuschter Slot-Lock.
- Anzeige: empfangene Nachrichten zeilenweise als Lauftext; neue Zeilen unten, ältere automatisch nach oben. Pause/Weiter und manuelles Zurückblättern. Empfang und Logging laufen unabhängig von der Anzeige-Scrollposition weiter.
- Die AudioCaptureService-Quelle (4 kHz, 24 Samples/Block) ist vorhanden und muss abonniert werden; keinen zweiten ADC-Sampler erzeugen. FT8-Engine nicht verändern oder als direkt JS8-kompatibel behandeln.
- Aktuell wurden nur Protokoll-/UI-Plan und Handoff-Dokumentation aktualisiert. JS8-Firmwarecode ist noch nicht implementiert. Nächster Schritt: eigenständige NORMAL-Physical-Layer-Implementierung und JS8-Protokollparser, dann Referenzvektoren, danach UI/Logging.
- Kein Build, keine Actions, kein Flash und kein Hardwaretest durch den Assistant.


## JS8Call technical baseline update (2026-11-21)
- Continued JS8 receive-only planning on `development`; no firmware code, build, Actions run, flash or hardware test in this step.
- Verified against the current public JS8Call source: 79 channel symbols = 21 sync symbols (three 7-symbol Costas blocks) + 58 data symbols; LDPC(174,87); 87 information bits comprise 75 payload bits plus CRC-12; CRC polynomial 0xC06 with JS8 XOR value 42. CRC validation is required before payload parsing.
- Verified sample-rate constraint: upstream source processes 12 kHz, while ATS Mini `AudioCaptureService` supplies 4 kHz. Nominal 4 kHz symbol lengths are 640 samples NORMAL, 1,280 SLOW, 400 FAST and 200 JS8 40. These are planning calculations only; filter/passband, timing and frequency stability must be measured on actual captured reference signals before choosing DSP constants.
- AUTO/MULTI speed detection remains a planned feature after individual modes are validated. Accept a speed only after mode-specific synchronization, channel decode and CRC validation. Benchmark parallel mode decoding against a lower-cost candidate scheduler; do not overload the ESP32-S3.
- Source provenance was recorded in `JS8CALL_RECEIVE_DECODER.md`. Do not copy GPL-licensed JS8Call implementation code or tables into this MIT repository without an explicit licensing decision.
- Next technical gate: obtain/prepare legally usable JS8 NORMAL reference recordings and an independently generated host-side validation path; then implement and verify the physical layer before integrating UI or logging.


## JS8 NORMAL 4 kHz receive-chain gate (2026-11-21)
- Follow-up to JS8 baseline research: the ATS Mini capture is 4 kHz, Nyquist 2 kHz, while upstream JS8's baseline window extends to 2.5 kHz. Do not port its 12 kHz FFT/filter constants or baseline window unchanged.
- NORMAL at 4 kHz is 640 samples/symbol, 160 ms/symbol, eight orthogonal tone bins spaced 6.25 Hz. Confirm the receiver's actual audio center/passband and GPIO11 capture response on the hardware; a center near 1 kHz is only an initial measurement target, not an assumed setting.
- A 15-second mono int16 raw ring window is 120,000 bytes. Keep AudioCaptureService callbacks producer-only and short; DSP, sync search, LDPC and parser belong in a worker. Count overruns and reject frames with audio gaps.
- New validation gates in `JS8CALL_RECEIVE_DECODER.md`: deterministic NORMAL vectors; host-side 4 kHz DSP tests with timing/carrier/noise offsets; independently verified LDPC bit ordering and CRC vectors; host performance/RAM tests; only then firmware integration.
- Offline phase acquisition must use validated JS8 sync candidates and a monotonic 15-second phase grid, never fake UTC from the capture callback's wall-clock timestamp. Do not display OFFLINE LOCK before physical-layer validation passes.
- Source-only documentation update. No firmware code, build, Actions, flash or hardware test performed.


## JS8 audio capture audit (2026-11-21)
- Actual AudioCaptureService source inspected: software-timed analogRead() on GPIO11 (not I2S/DMA), nominal 4 kHz scheduler, 24-sample callbacks, periodic vTaskDelay(1) and timing-base reset every 2,000 samples.
- Callback timestamp uses gettimeofday() at block delivery and back-calculates a block start; it is not a monotonic per-sample hardware timestamp. Sample-rate stability/jitter remains unverified.
- Next: JS8-owned non-invasive capture diagnostic subscriber. Measure callback cadence/drift estimate, missing blocks/overruns, ADC min/max/DC/RMS/clipping, then spectral response using controlled tones and real legally usable JS8 NORMAL recordings. Keep DSP out of callback and do not modify shared capture or existing decoders until evidence requires it.
- Detailed plan: JS8CALL_RECEIVE_DECODER.md section 2c. No firmware code or build/Actions/flash/hardware validation yet.


## Kurze Chat-Übergabe – JS8 (21.11.2026)
**Fortsetzen:** `firetvstick2k2-afk/ats-mini-dev`, Branch `development` בלבד. `main` nicht ändern.

**Priorität:** nativer JS8Call-Empfangsdecoder (RX-only; kein TX/Encoder/PTT). Zuerst NORMAL. Danach ggf. SLOW/FAST; JS8 40/60 erst nach Performanceprüfung.

**Aktueller Stand:** bisher Planung/Dokumentation, noch kein JS8-Firmwarecode. Zuletzt wurde `AudioCaptureService.cpp` geprüft: GPIO11 softwaregetaktetes `analogRead()`, kein I2S/DMA; 4 kHz nur Sollwert. 24 Samples je Block, nominal 6 ms; Blockzeitstempel wird bei Übergabe aus `gettimeofday()` zurückgerechnet; periodischer yield/Zeitbasisreset alle 2.000 Samples. Sample-Jitter und Drift sind unbestätigt.

**Nächster Schritt:** eigenen JS8-Diagnose-Subscriber an den vorhandenen AudioCaptureService hängen. Im Worker Callback-Abstände (monotone Zeit), Drift-Schätzung, Aussetzer/Queue-Overruns, ADC min/max/DC/RMS/Clipping erfassen. Danach kontrollierte 4-kHz-Tontests und rechtmäßig nutzbare JS8-NORMAL-Referenzaufnahmen. Callback nur kopieren/enqueue; keine DSP-Arbeit darin.

**Schutzregeln:** keinen zweiten ADC-Sampler; gemeinsamen Audiopfad nicht vor Messung verändern; FT8/FT4, CW, RTTY und bestehende Layouts nicht beeinträchtigen; LILYGO GPIO11/SPI-Guard erhalten. Keine GPLv3-Übernahme aus JS8Call in MIT-Repo ohne Lizenzentscheidung. Kein Build/Actions/Flash ohne ausdrückliche Bitte; Nutzer baut und testet Hardware.

**Letzte Commits:** Plan 2c `3de3b88495ddfde2d52d6f763a1392054f28e108`; vorherige Handoff-Updates `29f6ed05530332acfbf305e91aaef273e064fe64` und `4a96910b5c75ca9e0c1c711e6dca10f4cb38aa1b`. Details: `JS8CALL_RECEIVE_DECODER.md`, Abschnitt 2c.


## JS8 – Audiomessdienst erster Implementierungsschritt (21.11.2026)
- Auf development wurden ats-mini/src/js8/Js8AudioMonitor.h/.cpp angelegt. Das ist ein JS8-eigener Subscriber für den bestehenden AudioCaptureService, kein zweiter Sampler.
- Der Callback kopiert nur den 24-Sample-Block in eine begrenzte 96-Block-FreeRTOS-Queue, erfasst esp_timer_get_time() bei Callback-Eintritt und zählt Callback-/Queue-Drops. Er wertet keine Audiosamples aus und ignoriert den Wall-Clock-Zeitstempel für die Taktmessung.
- Ein separater Worker ermittelt Callback-Abstands-Min/Max/Mittelwert, eine aus Blockabständen geschätzte effektive Ausgaberate, längere Lücken/fehlende Blöcke sowie ADC-Min/Max, DC/Mittelwert, AC-RMS und Near-Rail-Clipping. Ein PSRAM-first-Ringpuffer (Fallback interner Heap) hält die letzten 4.096 Samples (nominal ca. 1,024 s) für spätere Spektralanalyse bereit.
- API: begin(AudioCaptureService&), stop(), copyMetrics() und copyRecentSamples(). Die Makefile-Wildcards berücksichtigen src/js8.
- Ein eigener Menüpunkt „JS8 RX MONITOR“ steht direkt nach FT8/FT4 (englische und deutsche Menüquelle). Er öffnet eine separate JS8-Monitorseite und startet/stoppt ausschließlich den eigenen Subscriber. Keine Radio-Retune-, WLAN-Stop- oder FT8/CW/RTTY-Service-Aufrufe.
- Die Seite zeigt Sample-Rate-Schätzung aus Callback-Zeitstempeln, Blockintervall, ADC-Min/Max, DC-Mittelwert, AC-RMS sowie Callback-/Drop-/Gap-/Clipping-Zähler. Es ist ausdrücklich noch kein JS8-Decoder. Kurzer Klick ist reserviert, langer Druck beendet die Ansicht. Die Messwerte beschreiben die Callback-/Blockausgabe des Software-Schedulers und können keine intra-Block-analogRead-Jittermessung ersetzen.
- LILYGO GPIO11/SPI-Ausschluss bleibt erhalten. Bestehende FT8/FT4-, CW-, RTTY- und Layout-Dateien wurden nicht geändert.
- Noch kein Build, kein prek, keine GitHub Actions, kein Flash und keine Hardwaremessung. Code ist nicht compiler-verifiziert.
- Nächster Schritt: Menü-/UI-/Lifecycle-Integration source-seitig prüfen; danach kontrollierte Tonmessungen auf echter Hardware, Spektralanalyse und reale JS8-NORMAL-Aufnahmen. Noch kein JS8-NORMAL-Decoder.


### JS8 synthetic test playback update — 2026-11-21
- JS8 RX Monitor short-click now starts/stops a built-in deterministic 8-FSK test waveform; long press exits. It generates 79 × 160 ms symbols at nominal 4 kHz, eight tones spaced 6.25 Hz around 1 kHz, with ADC-offset samples. Only the JS8 monitor subscriber is paused; live capture resumes after playback.
- Not a valid JS8 frame and not decodable yet. No verified Costas sync, LDPC, CRC or message payload is included. This tests monitor block processing/levels/timing only. A protocol-valid NORMAL golden frame and decoder sample-consumer hook are still required.
- Commits on development: 25d2723c6a97e92b48671481b1395b279d73d16f, b889bd8d3dbc36917097823ce79365110b969762, 0ed599949f1ed0b5f428b36ec501bdad7e9db337, fa04aa000084fd63524753addd412689600e8436, fd822658bd84d152f8b299ed13788fb266b4343e.
- No compile, Actions, flash or hardware test.

- UI follow-up: while the synthetic source is active, the monitor now labels it SYNTH / NOMINAL 4 kHz instead of implying GPIO11 is being sampled (commit 6f659ff205a64b6922ee772c5a882e0fe26fb927).


## JS8 UI / next self-test update — 2026-11-21

- Latest JS8 screen commit on development: 959e58308b4982fa96e3a754c7bc4f0249b55597.
- Js8Ui.cpp now uses pure black, white primary text, muted secondary text and limited green accents. Metrics are compressed into three rows; lower half is reserved for decoded messages. It displays a waiting prompt rather than fake decoded content.
- Existing generic 8-FSK click test is still only an audio pipeline/timing stimulus. It is not a protocol-valid JS8 NORMAL frame and cannot decode a known message.
- Next: build an independently generated, protocol-valid JS8 NORMAL golden vector with a fixed payload; host-validate symbol/tone/timing/LDPC/CRC metadata; then implement the JS8-only sample consumer and NORMAL decoder and use the golden vector as an on-device decoder self-test. Only display text after valid sync + LDPC + CRC.
- No build/Actions/flash/hardware test was performed. Continue only on development; main untouched.


## JS8 license decision / clean-room boundary — 2026-11-21
The owner explicitly chose a fully independent implementation and preservation of the MIT license. Do not import GPL JS8Call code or copied/source-extracted LDPC tables. Public docs give the NORMAL waveform parameters but not the complete LDPC construction. A third-party article containing complete tables says those tables were machine-generated from JS8Call reference source, so it is not an independent table source and must not be copied. An arbitrary newly generated LDPC(174,87) matrix would be a different code and is not interoperable. Next: independently implement and test the NORMAL 4 kHz front end, Costas synchronization and soft 8-tone decisions with a clearly non-protocol-valid fixture; keep message decode blocked until the exact interoperable LDPC construction is lawfully available. Details: JS8CALL_RECEIVE_DECODER.md section 10. Latest documentation commits: 746dac66148af67abd18e196d08afc66924c70de (decoder plan), 06ff4d74da7c34391b44d04a78b3a1c9e5cae4d7 (development handoff). No build/Actions/flash was run.


## FM-DX adaptive filter and screen — 21.11.2026

- Automatic firmware workflow disabled for push events on `development`; only `workflow_dispatch` remains. Manual build is available when the user chooses. Workflow commit: `e14df4ad19c661cca25eaceb3a1d210cf3ea7751`. No build was launched.
- FM-DX implementation added under `ats-mini/src/radio/`: `FmDxController.h/.cpp` and `FmDxUi.h/.cpp`.
- Controller is FM-only, saves/restores the actual current SI4735 FM bandwidth, starts at 60 kHz, then uses RSSI/SNR hysteresis and a 6-second minimum dwell to select 40/60/84 kHz. It does not modify the band's persisted bandwidth index, AGC, or existing SI4735 stereo blend.
- Dedicated screen is 320×170, dark navy/cyan/neon green, with frequency, RSSI, SNR and filter status. Short click pauses/resumes automatic filtering; long press exits/restores. Menu item appended in German, English and Italian. Existing layout renderers were not changed.
- Integration: `CMD_FM_DX` in `Menu.h`, draw dispatch in `Draw.cpp`, menu actions in all three menu sources, loop/button/encoder and RSSI hooks in `ats-mini.ino`, stop-before-band-switch, and `src/radio/*.cpp` Makefile wildcard.
- Static checks only (balanced braces and menu entries). No compilation, Actions, flash, or hardware validation. User starts the respective build manually. Initial RSSI/SNR thresholds need real receiver testing and may need calibration.
- Work only on `development`; do not touch `main`.


## FM-DX UI verification follow-up (2026-11-21)
- Reworked `ats-mini/src/radio/FmDxUi.cpp` into the compact 320x170 monochrome DX instrument screen: left-aligned frequency, station/RDS clock, 20-segment red/yellow/green RSSI meter, SNR, PI, PTY, two-line RadioText, filter and AUTO/PAUSE status.
- Static source audit confirmed the menu entries/actions exist in German, English and Italian; `CMD_FM_DX` dispatches to `drawFmDxPage()`; `fmDxUiTick()` requests 500 ms redraws; RSSI/SNR are sampled through the existing `processRssiSnr()`; the Makefile wildcard includes `src/radio/*.cpp/.h`.
- Confirmed FM bandwidth table indices: array positions map to device indices Auto=0, 110k=1, 84k=2, 60k=3, 40k=4. The controller's 40/60/84 device indices therefore match the existing table.
- Hardened lifecycle for external/remote band changes: the FM-DX page exits when the mode is no longer FM, and the controller no longer attempts to restore an FM filter while the receiver is in AM/SSB. Also made new UI variadic format arguments explicitly unsigned.
- Remaining validation limits: no compilation, GitHub Actions, flash or hardware test was run. Live stereo indication is now implemented using the inherited SI4735 FM_RSQ_STATUS pilot and blend getters (see the later FM-DX live stereo indication entry). RSSI/SNR thresholds and filter decisions still require real receiver testing/calibration; they are not validated performance thresholds.
- Work remains on `development` only; `main` was not targeted. User starts the build manually.


## FM-DX live stereo indication — 2026-11-21
- Research confirmed that the inherited PU2CLR SI4735 API exposes `getCurrentReceivedSignalQuality()`, `getCurrentPilot()` and `getCurrentStereoBlend()`. The SI4735 FM_RSQ_STATUS response reports the detected 19 kHz stereo pilot and blend percentage; this is a live receiver measurement, not the configured `fmStereoIdx` setting.
- `ats-mini/src/radio/FmDxUi.cpp` now polls FM_RSQ_STATUS on the existing 500 ms FM-DX refresh and displays `STEREO xx%`, `MONO`, or `MONO LOCK` when forced-mono mode is selected.
- The forced-mono setting is respected in the display even if a pilot is detected. In automatic mode, pilot presence and the chip's blend percentage are shown.
- Source change committed on `development`: `7898b6999726e7644c03688f7207c91b87171fcc`.
- No build, GitHub Actions run, flash or hardware test was performed. User should verify on a known mono station, a strong stereo station, a weak/blended station, and with forced mono enabled. No change to `main`.


## Quiet startup volume and silent FT8/FT4 monitoring — 2026-11-21
- On every boot, the runtime receiver volume is now forced to 10/63 after saved settings are loaded. This deliberately overrides the previous session's saved volume for startup only; the user can raise it manually after boot.
- FT8/FT4 still raises the SI473x output volume to 50/63 while the decoder is active, then restores the pre-decoder volume on exit. Decoder capture/DSP samples are unchanged.
- FT8/FT4 now always enters with the external speaker muted (speaker toggle OFF), while the amplifier remains powered for the existing GPIO11 decoder audio tap. The user can turn the speaker ON explicitly in the decoder Actions menu. On exit, the speaker mute/amplifier state from before entering the decoder is restored.
- Scope: `development` only. No build, GitHub Actions, flash or hardware test was run; user performs the OSPI-DE build and device verification. `main` remains untouched.


## FM-DX control and layout corrections — 2026-11-22
- Encoder rotation on the FM-DX page now tunes the current FM frequency using the selected band step and existing updateFrequency() range/wrap logic. The updateFrequency() declaration is kept at global namespace scope to match its definition in ats-mini.ino.
- Short encoder click opens a compact FM-DX Actions overlay. Rotate to select AUTO start/pause, cycle the manual filter width (84/60/40 kHz), or Back; click applies the selected action. Long press still exits.
- Controller lifecycle now distinguishes pausing adaptive AUTO from ending the FM-DX session. The originally active FM bandwidth is captured once and restored on page exit; manual filter selection and AUTO pause/resume do not lose the saved bandwidth. External mode changes still avoid applying an FM filter while in AM/SSB.
- Display refinements: bandwidth indicator moved upward; MHz baseline adjusted lower alongside the frequency; PI/PTY values aligned to their labels; RadioText remains left-aligned and is positioned within the text panel using the larger default font; footer spacing added between FILTER and the bandwidth value.
- Scope is development only. No build, GitHub Actions, flash or hardware test was run. Static source review only; user will run the OSPI-DE build manually. Main untouched.
