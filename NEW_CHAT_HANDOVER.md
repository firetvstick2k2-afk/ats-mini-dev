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
