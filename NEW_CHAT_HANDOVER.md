# ATS Mini Dev — Übergabe für neuen Chat

Stand: 21.11.2026  
Repository: `firetvstick2k2-afk/ats-mini-dev` (privat)  
Arbeitsbranch: `development`  
Letzter Code-Stand: CW-Decoder-Port in Arbeit abgeschlossen; Handoff-Dokument wird aktualisiert.

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
- Relevante Code-Commits: `25775d1dfba9519ce04be67e05903340a5b4a54d`, `401eb98d99d5f2219e57496d8d64f9f4b9e44ab3`, `3dbdad4d9925ddd737aa6a18c2f1b55ebb5a965d`, `2d917e58a06943fcf39c68941a6ee169658da470`, `a764c87925b5e0387e234de4ebc5e962ad5f83fb`, `9ac9dc1633ff2c21c73b1d92802a64a2dc239a25`, `965adc959e29ddacf222934c4a6883fd01d3b4d8`, `bbed86448b7b99f61ca02df08d0cef07f32d312c`, `2559340c50e8444459ebc7346da96131a089222d`, `e970b9cf5a71e8e85e5a01ca687bfce9967e3ac2`, `cbda87aa0f2b404b69e34be4bae752644916f09f`, `cdbff5f06915a8934237464aa67b1f1da6dc5a92`, `9b98c518ce01848e2922f3ae69af4b4dad5b1538`, `04d595c9c5205d0dadf5e99a8dc438ff72ef911a`, `9de4ceda973e6e25c1060eaf74670a3bedfddd97`, `faff7cd6ff8695ae5511935d6a9240388d7f4a09f`, `256098b22d8572f5a1d4f309971f816f113f2610`.
