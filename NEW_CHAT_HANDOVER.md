# ATS Mini Dev — Übergabe für neuen Chat

Stand: 20.11.2026, abends  
Repository: `firetvstick2k2-afk/ats-mini-dev` (privat)  
Arbeitsbranch: `development`  
Letzter bestätigter Commit: `2daa5a1a6df3b34e76d9466f03df21c51bfde989` (aktualisiertes Handover; der vorherige Code-/DAB-Stand ist in den darunter genannten Commits enthalten)

## 1. Wichtige Arbeitsregeln

- Ausschließlich im privaten Repo `firetvstick2k2-afk/ats-mini-dev` auf Branch `development` arbeiten.
- `main` und das öffentliche Repo `13HN2709/ats-mini` niemals ändern, außer der Nutzer fordert es ausdrücklich.
- Vor Codeänderungen die aktuellen Dateien auf `development` lesen. Keine Änderungen aus alten Chat-Snippets blind übernehmen.
- Möglichst kleine, isolierte Änderungen. Bestehende funktionierende Features nicht unnötig refaktorisieren.
- **TEF6686-Layout ist das Lieblingslayout und bleibt unangetastet**, außer bei ausdrücklichem Auftrag.
- LittleFS-Mount-/Formatierungslogik ist funktionierend und geschützt. Keine Formatierung, kein Löschen und keine Änderung an der Mount-Strategie ohne ausdrücklichen Auftrag.
- Der Nutzer startet GitHub Actions manuell und flasht/testet das ATS Mini V4 selbst. Niemals Build- oder Hardware-Erfolg behaupten, bevor der Nutzer bzw. ein verlässlicher Lauf dies bestätigt.
- Nach jeder sinnvollen Änderung `DEVELOPMENT_HANDOFF.md` aktualisieren. Dieses Dokument ist die ausführliche technische Langzeitdokumentation.

## 2. Aktueller Stand / letzter Auftrag

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
- Nur FT8 und FT4 öffnen den Decoder. CW/RTTY bleiben normale Hör-/Frequenz-Presets; keine CW-/RTTY-Decoder starten.
- UTC kann offline über gültige RDS CT gesetzt werden; Decoder-Einstieg wartet auf `clockAvailable()`.
- Decoder-Waterfall ist noch nicht implementiert.
- Empfang/Decodierung auf echter Hardware ist nicht bestätigt. Timing, Audio-Pfad und UTC müssen nach erfolgreichem Build weiter geprüft werden.

### Decoder-Frequenzkatalog
- 11 Kategorien und 75 Presets sind importiert.
- Navigation: Kategorien → Presets; 2-Sekunden-Vorschau stimmt Frequenz ab.
- FT8/FT4-Presets öffnen den Decoder, normale Presets kehren zur aktiven Layoutanzeige zurück.
- Keine Presets stillschweigend entfernen oder die Quellanzahl auf 74 ändern.

## 7. Nächste Prioritäten — ausdrücklich vereinbart

Der Nutzer möchte zunächst die **Grundfunktionen stabilisieren**, bevor neue Extras dazukommen:

1. **Speichermanagement prüfen und verbessern:** LittleFS-Nutzung, Logos, Speicherbedarf, Dateizugriffe, Fehlerfälle und mögliche Speicherknappheit analysieren. Bestehende Nutzerdaten schützen; keine Formatierung.
2. **Halbfertige Übernahmen richtigstellen:** Übernommene Mini-OS-/Decoder-Funktionen einzeln inventarisieren, tatsächliche Integration prüfen, fehlende Verbindungen/Fehler sauber fertigstellen und als integriert / teilweise integriert / offen kennzeichnen.
3. Danach Kernfunktionen systematisch testen und erst dann weitere Komfortfunktionen/Layouts angehen.

Keine größere neue Funktion anfangen, bevor diese Basis mit dem Nutzer abgestimmt und die aktuellen offenen Punkte priorisiert wurden.

## 8. Unmittelbare nächste Schritte im neuen Chat

1. Nutzer führt den korrigierten manuellen OSPI-DE-Build auf `development` aus und meldet das Ergebnis.
2. Bei erfolgreichem Build: Hardwaretest DAB Style FM → Nicht-FM/TinyRadio → FM sowie Logo-/Farbprüfung abwarten.
3. Anschließend `DEVELOPMENT_HANDOFF.md` mit echtem Build-/Hardware-Ergebnis ergänzen.
4. Danach mit dem Nutzer den ersten Schritt der Grundfunktionen beginnen: strukturierte Bestandsaufnahme des Speichermanagements und LittleFS, ohne Änderungen an funktionierender Format-/Mount-Logik.
5. Bei jeder Codeänderung ausschließlich `development` verwenden; `main` bleibt unangetastet und das ausführliche Handover wird aktualisiert.

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
