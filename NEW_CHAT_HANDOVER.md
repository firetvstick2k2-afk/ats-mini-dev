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
