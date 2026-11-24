# Flight Radar Handoff

## WLAN-UI-Fortsetzung – 2026-11-24 (aktueller Stand: WLAN-Info im WLAN-Menü)

- Status-Popups in `ats-mini/Draw.cpp` wurden kompakter, dynamisch breit und zentriert gestaltet. Der Hintergrund wird nicht gelöscht; das Radio-Layout bzw. Menü bleibt rundherum sichtbar.
- `WLAN-MODUS` (EN: `Wi-Fi Mode`) enthält die vorhandene Betriebsmodus-Auswahl plus `WLAN-INFO` als letzten Auswahlpunkt (EN: `Wi-Fi Info`, IT: `Info Wi-Fi`). Der separate Eintrag in den allgemeinen Einstellungen wurde entfernt. Die Info-Auswahl öffnet eine read-only Statusanzeige; ein eigener Menüindex hält die WLAN-Modusnummer unverändert.
- Die Info-Anzeige zeigt tatsächlichen WLAN-Modus, Verbindungsstatus, SSID, IP, bei AP zusätzlich AP-IP und Clientzahl, bei STA RSSI sowie den Hostnamen `mini-os.local`. Klick kehrt ins WLAN-Untermenü zurück; der allgemeine Menü-Timeout schließt die Anzeige nicht.
- `statusTick(millis())` wird bereits am Ende von `loop()` aufgerufen. Statusverwaltung und Netzwerk-/Verbindungslogik in `Network.cpp` blieben unverändert.
- Änderungen nur in `Draw.cpp`, `Menu.h`, den drei Sprach-Menüvorlagen und `ats-mini.ino`. Flight-Radar-Funktion und deren Datenpfad nicht verändert.
- Der Nutzer hat Build #83 gestartet und die Status-Popups bereits als funktionierend und optisch gelungen bestätigt. Die Verschiebung von `WLAN-INFO` ins WLAN-Untermenü ist noch nicht durch einen neuen Build/Hardwaretest bestätigt.

---

## Status – 2026-11-24

- The `development` branch has been reset to the last user-confirmed hardware-working baseline, Build #69 (`7825f22feda8d095a542bf819a07f9bf62c23a7c`).
- Flight Radar changes introduced after that baseline are intentionally not present in the current test state. The prior Flight Radar implementation and its later NVS-load adjustment must not be assumed to exist in this branch state.
- `main` was not changed. No build, flash, or hardware test was initiated by the assistant.
- The only firmware commit layered on top of Build #69 is the isolated About branding update `cb0b305acdbb9fc7d79d2b7d0c8d0901fb510b7a`; it does not modify Flight Radar.


## 2026-11-24 – About-Anzeige: Dispatch-Reihenfolge korrigiert

- Ursache: `drawScreen()` behandelte `CMD_ABOUT` bereits in der allgemeinen Settings-Popup-Abfrage (`currentCmd <= CMD_ABOUT`). Dadurch wurde `drawSettingsPopup()` mit der alten Beschriftung `ATS MINI V4` ausgegeben; der spätere Aufruf von `drawAbout()` war unerreichbar.
- Korrektur in `ats-mini/Draw.cpp`: obere Grenze der allgemeinen Settings-Abfrage von `<= CMD_ABOUT` auf `< CMD_ABOUT` geändert. `CMD_ABOUT` erreicht nun den dedizierten grafischen About-Renderer in `About.cpp`.
- Nur `Draw.cpp` und diese Übergabedokumentation geändert. Kein Build, kein Flash und kein Hardwaretest durch den Assistenten; Nutzer soll den nächsten OSPI-DE-Build selbst starten und die About-Seite prüfen.


## WLAN/Netzwerk – AP-Adresse und mDNS-Hostname wiederhergestellt (2026-11-24)
- Auf `development` wurde in `ats-mini/Network.cpp` die Access-Point-Adresse einschließlich Gateway von `10.1.1.1` auf `192.168.4.1` geändert.
- Der mDNS-Hostname wurde von `atsmini.local` auf `mini-os.local` umgestellt: `MDNS.begin("mini-os")`; der HTTP-Dienst bleibt auf TCP-Port 80 registriert.
- Die vorhandenen WLAN-Status-Popups bleiben ausdrücklich erhalten: Verbindungsversuch, erfolgreiche WLAN-Verbindung mit IP/Hostname, AP-SSID mit IP/Hostname sowie Fehlermeldung bei fehlender Verbindung. Diese Popups waren bereits im Build-#69-Baseline-Quelltext vorhanden; die spätere Änderung zum Entfernen der Popups wird NICHT übernommen.
- WLAN-Zugangsdaten, Verbindungsmodi, Webserver, NTP-Ablauf und sonstige Netzwerk-/Speicherfunktionen wurden nicht geändert.
- Speicher-/LittleFS-Migration und Flight Radar bleiben bewusst außen vor.
- Code-Commit: `441093f6eab03b65b5e8f26e484c85c8edf00a0e`.
- Kein Build, kein GitHub-Actions-Lauf, kein Compile, kein Flash und kein Hardware-/Netzwerktest durch den Assistenten. Nutzer startet den nächsten Build manuell.


## 2026-11-24 – WLAN-Status als kompaktes Overlay

- `ats-mini/Draw.cpp` ergänzt `drawStatusOverlay()`: WLAN-/Betriebsstatusmeldungen werden nach dem Zeichnen des aktiven Radio-Layouts bzw. des Vollbild-Menüs als kleines, zentriertes Kästchen über den bestehenden Bildinhalt gelegt.
- Der bisherige Bildschirmhintergrund wird für diese Statusmeldung nicht gelöscht; Layout bzw. Menü bleiben außerhalb des Overlays sichtbar.
- Die vorhandene Statusverwaltung (`statusShow()` / `statusTick()`) und die vorhandenen WLAN-Meldungstexte in `Network.cpp` bleiben unverändert.
- Änderung committed auf `development`: `0d9b8941d5fee27fc6f7d12b2355c4db8faa1c3a`.
- Der vorhandene WLAN-Modus-Menüeintrag wurde in diesem Commit noch nicht funktional erweitert; ein separates WLAN-Info-Menü ist weiterhin offen.
- Kein Build, kein GitHub-Actions-Lauf, kein Compile, kein Flash und kein Hardwaretest gestartet. Der Nutzer startet den nächsten Build manuell.

## Cross-project repository note — 2026-11-24

- The repository owner has changed repository visibility to **private**.
- All development remains on branch `development`; `main` remains protected.
- No Flight Radar source, API, UI, image, or LittleFS behavior was changed in the JS8 GPL/LDPC milestone.
- JS8 work is isolated under `ats-mini/src/js8/`; its GPLv3 notice and license are documented separately. This does not alter the Flight Radar implementation.

## JS8 status cross-reference — 2026-11-24

A follow-up JS8-only commit added a known-start/known-frequency NORMAL 8-FSK soft demodulator and a synthetic end-to-end host test. This did not modify Flight Radar code, assets, APIs, UI, or scheduling. See `JS8CALL_RECEIVE_DECODER.md` for the current JS8 state and GPL notices.


## 2026-11-24 — bounded JS8 NORMAL acquisition candidate search

- Added `Js8NormalAcquire.h/.cpp`: bounded coarse search over caller-specified frame-start and common-frequency-offset ranges. Search is allocation-free and capped by `maxCandidates`; it rejects tone grids outside the 4 kHz Nyquist band.
- Candidate ranking uses the 21 NORMAL Costas pilots. The result is explicitly only a synchronization candidate; it is not a valid-frame lock and must still pass the existing LDPC decoder and JS8 CRC before any text is emitted.
- Added `tests/js8_normal_acquire_test.cpp`. Host test compiled with `g++ -std=c++17 -O2 -Wall -Wextra -Werror` and passed: synthetic NORMAL pilot frame found with a 17-sample start displacement and +1.25 Hz offset; silence rejected; candidate budget respected.
- This is a coarse bounded search primitive, not yet a full 15-second rolling-slot scanner. Search bounds/step sizes and thresholds are provisional; CPU cost, noise/interference false positives, timing refinement, and real JS8Call interoperability remain to be measured.
- No firmware build, GitHub Actions run, hardware test, live AudioCaptureService integration, or full JS8 message decode was performed. FT8/FT4 and LILYGO GPIO11 guard remain untouched.
- Licensing state: JS8 LDPC/payload component is adapted from GPL-3.0 JS8Call source and includes its license/provenance files. The repository remains private by user decision. Private visibility does not remove GPL obligations if the combined work is later conveyed/distributed; review those obligations before any external distribution.


## JS8 NORMAL acquisition refinement cross-reference — 2026-11-24

The JS8-only subsystem gained bounded local refinement around a coarse NORMAL Costas candidate (`Js8NormalAcquire.h/.cpp`) and a host test source (`tests/js8_normal_refine_test.cpp`). This did not modify Flight Radar code, APIs, UI, assets, scheduling, or LittleFS behavior. The new host test has not been executed; no firmware build or hardware test was performed. See `JS8CALL_RECEIVE_DECODER.md` for JS8 status and licensing notes.
\n\n---\n\n## PRIORITY 1 — WLAN-Menüanzeige vor jeder weiteren JS8-Arbeit prüfen (Nutzer-Hardwaretest, 2026-11-24)

**Gemeldeter Fehler (vom Nutzer am Gerät reproduziert):**
- Im WLAN-MODUS-Menü ist zunächst „Connect“ sichtbar.
- Ein Dreh nach rechts ändert intern die Auswahl; auf dem Display bleibt jedoch weiterhin „Connect“ stehen.
- Auch weitere Drehschritte werden intern übernommen, während die sichtbare Auswahl nicht mitwandert.
- Ein Druck auf OK führt den tatsächlich intern ausgewählten, aber nicht angezeigten Menüpunkt aus.
- Beim erneuten Öffnen des WLAN-Menüs stimmt die Anzeige wieder und zeigt die nun gespeicherte Auswahl.
- Das ist ein Anzeige-/Auswahl-Synchronisationsfehler, kein vom Nutzer gemeldeter WLAN-Verbindungsfehler.

**Code-Ursache bereits im aktuellen Branch lokalisiert (noch NICHT korrigiert):**
- `ats-mini/Menu_german.cpp.src`, `Menu_english.cpp.src` und `Menu_italian.cpp.src`: `doWiFiMode()` verändert `wifiMenuIdx`; `drawWiFiMode()` zeichnet ebenfalls anhand von `wifiMenuIdx`.
- Beim Öffnen des WLAN-Menüs wird `wifiMenuIdx = wifiModeIdx` gesetzt.
- Die Bestätigung über `clickHandler(CMD_WIFIMODE)` übergibt `wifiMenuIdx` an `clickWiFiMode()`, die den Wert nach `wifiModeIdx` übernimmt. Deshalb wird beim OK die unsichtbar weitergedrehte Auswahl tatsächlich ausgeführt.
- **Fehlerhafte Popup-Anzeige:** In allen drei Sprachvorlagen liest der `CMD_WIFIMODE`-Zweig von `drawSettingsPopup()` derzeit `wifiModeDesc[wifiModeIdx]` statt des temporären Auswahlindex `wifiMenuIdx`. Das Popup zeigt also den zuletzt übernommenen Modus („Connect“) statt der gerade gedrehten Auswahl. Das deckt sich exakt mit dem Hardware-Symptom.
- **Wichtig beim Fix:** `wifiMenuIdx` kann auch `ITEM_COUNT(wifiModeDesc)` annehmen. Dieser zusätzliche Index steht für `WLAN-INFO` und ist kein gültiger Index in `wifiModeDesc[]`. Eine einfache Ersetzung durch `wifiModeDesc[wifiMenuIdx]` wäre daher bei WLAN-INFO ein Out-of-Bounds-Zugriff. Die Anzeige muss diesen Sonderfall explizit als „WLAN-INFO“ (bzw. vorhandene lokalisierte Bezeichnung) behandeln.
- Den Auswahlindex `wifiMenuIdx` und den tatsächlich aktiven/übernommenen Modus `wifiModeIdx` weiterhin getrennt halten. WLAN-Info bleibt read-only und muss sich weiterhin öffnen lassen.

**Nächster Arbeitsauftrag für den neuen Chat — zuerst und ausschließlich WLAN:**
1. Vor jeder Änderung aktuelle Inhalte und SHAs aller betroffenen Dateien erneut aus GitHub holen.
2. Den Popup-Wert in allen drei Sprachvorlagen konsistent korrigieren: Anzeige folgt `wifiMenuIdx`; Sonderindex `ITEM_COUNT(wifiModeDesc)` zeigt „WLAN-INFO“ und greift nicht auf das Array zu.
3. Den gesamten Ablauf prüfen: Menü öffnen (Index auf aktiven Modus initialisieren), ein Drehschritt, mehrere Drehschritte, beide Drehrichtungen, Wrap-around, OK auf jeden der fünf WLAN-Modi, OK auf WLAN-INFO, Zurück aus WLAN-INFO und erneutes Öffnen. Prüfen, dass WLAN-Modus erst bei OK übernommen wird und WLAN-INFO nicht versehentlich `netInit()` auslöst.
4. Nur die minimal erforderlichen Menü-/Vorlagendateien ändern. Keine WLAN-Verbindungslogik, Zugangsdaten, Netzwerkdienste, Status-Popups oder sonstigen Einstellungen anfassen.
5. Die betroffenen Handoff-Dateien nach dem Fix aktualisieren. Erst danach den Nutzer um seinen Build-/Hardwaretest bitten; keinen Build und keine GitHub Actions selbst starten, solange der Nutzer das nicht ausdrücklich verlangt.

**Stand:** Diagnose anhand des aktuellen Quelltexts plausibel und direkt passend zum beobachteten Fehler; Fix noch nicht committet. Der Nutzer hat den Fehler am Gerät getestet und meldet ihn als reproduzierbar. JS8-Arbeit ist bis zur erfolgreichen WLAN-Korrektur und Nutzerbestätigung pausiert. FT8/FT4 bleibt unangetastet.


## WLAN-Menü-Popup-Korrektur (2026-11-24)

- Ursache: Das WLAN-Modus-Popup verwendete den übernommenen Index `wifiModeIdx` statt des temporären Navigationsindex `wifiMenuIdx`. Dadurch änderte der Encoder die interne Auswahl, während die sichtbare Popup-Zeile stehen blieb.
- Korrektur ausschließlich in den drei Sprachvorlagen im Zweig `CMD_WIFIMODE`: Die Anzeige verwendet jetzt `wifiMenuIdx`; der Sonderindex `ITEM_COUNT(wifiModeDesc)` wird vor dem Arrayzugriff abgefangen und zeigt das jeweilige lokalisierte Info-Label.
- Deutsch: `ats-mini/Menu_german.cpp.src` enthält die Korrektur bereits (aktueller Blob-SHA: `f363a715c350b3de2b64b7f1ce04c82a8e7570cf`). Englisch und Italienisch wurden jetzt korrigiert: `ats-mini/Menu_english.cpp.src` Commit `598576c272263d86aff0a1d1a5e542e19ec775a8`; `ats-mini/Menu_italian.cpp.src` Commit `784786bd7f71600d0127a17cadd7ef469ef96089`.
- Labels für den Sonderindex: Deutsch `WLAN-INFO`, Englisch `Wi-Fi Info`, Italienisch `Info Wi-Fi`.
- Keine Änderungen an `Network.cpp`, `Draw.cpp`, WLAN-Verbindungslogik, Flight Radar, JS8 oder FT8/FT4.
- Noch kein Build, keine GitHub Actions und kein Hardwaretest durch ChatGPT gestartet. Nutzer muss den Firmware-Build und den Encoder-/Popup-Ablauf am Gerät prüfen. JS8 bleibt bis zur Nutzerbestätigung pausiert; FT8/FT4 bleibt unangetastet.
