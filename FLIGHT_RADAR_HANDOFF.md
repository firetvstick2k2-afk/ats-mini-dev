# Flight Radar Handoff

## 2026-11-24 – Build #89 / Backup / nächster Hardwaretest

- Build #89 (GitHub Actions Run-ID 45490746057) wurde auf development mit Commit b1a3303fc9e42c7af615885f54000efc53ab2e3f erfolgreich abgeschlossen: https://github.com/firetvstick2k2-afk/ats-mini-dev/actions/runs/45490746057
- Das Repository ist für den Test öffentlich gestellt worden. Das veröffentlichte Artefakt heißt Mini_OS-build-89-ospi-de (Artefakt-ID 14965992323); laut Nutzer sind BIN, ELF und MAP enthalten. Für Log-/Backtrace-Analysen ausschließlich die zu genau dieser BIN gehörenden ELF-/MAP-Dateien verwenden.
- Nach erfolgreichem Build wurde backup/development-2026-11-24 angelegt und verifiziert. Er zeigt auf denselben Commit wie development und bleibt unangetastet.
- Alle weiteren Arbeiten erfolgen ausschließlich auf development; main bleibt unangetastet. Keine weiteren Branches erstellen.
- Geplanter Hardwaretest des Nutzers am 2026-11-25: Build #89 flashen, Verhalten prüfen, anschließend Logging aktivieren und das Log kontrollieren. Bei einem Absturz Log sowie ELF/MAP aus Build #89 sichern. Der Assistent hat weder geflasht noch einen Hardwaretest durchgeführt.
- JS8 inklusive Lizenz wurde entfernt; der erfolgreiche Build bestätigt den aktuellen bereinigten Stand. FT8/FT4 bleibt unangetastet.

---

## WLAN-UI-Fortsetzung – 2026-11-24 (aktueller Stand: WLAN-Info im WLAN-Menü)

- Status-Popups in `ats-mini/Draw.cpp` wurden kompakter, dynamisch breit und zentriert gestaltet. Der Hintergrund wird nicht gelöscht; das Radio-Layout bzw. Menü bleibt rundherum sichtbar.
- `WLAN-MODUS` (EN: `Wi-Fi Mode`) enthält die vorhandene Betriebsmodus-Auswahl plus `WLAN-INFO` als letzten Auswahlpunkt (EN: `Wi-Fi Info`, IT: `Info Wi-Fi`). Der separate Eintrag in den allgemeinen Einstellungen wurde entfernt. Die Info-Auswahl öffnet eine read-only Statusanzeige; ein eigener Menüindex hält die WLAN-Modusnummer unverändert.
- Die Info-Anzeige zeigt tatsächlichen WLAN-Modus, Verbindungsstatus, SSID, IP, bei AP zusätzlich AP-IP und Clientzahl, bei STA RSSI sowie den Hostnamen `mini-os.local`. Klick kehrt ins WLAN-Untermenü zurück; der allgemeine Menü-Timeout schließt die Anzeige nicht.
- `statusTick(millis())` wird bereits am Ende von `loop()` aufgerufen. Statusverwaltung und Netzwerk-/Verbindungslogik in `Network.cpp` blieben unverändert.
- Änderungen nur in `Draw.cpp`, `Menu.h`, den drei Sprach-Menüvorlagen und `ats-mini.ino`. Flight-Radar-Funktion und deren Datenpfad nicht verändert.
- Die WLAN-Menü-/WLAN-Info-Integration und die Popup-Auswahlkorrektur sind inzwischen erledigt. Die frühere offene Prüfliste ist geschlossen; die Korrektur ist im Abschnitt unten als erledigt dokumentiert.

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

- The repository owner changed repository visibility to **public** for the successful Build #89 test.
- All development remains on branch `development`; `main` remains protected.

## WLAN-Menü-Popup-Korrektur – ERLEDIGT (2026-11-24)

- Ursache: Das WLAN-Modus-Popup verwendete den übernommenen Index `wifiModeIdx` statt des temporären Navigationsindex `wifiMenuIdx`. Dadurch änderte der Encoder die interne Auswahl, während die sichtbare Popup-Zeile stehen blieb.
- Korrektur ausschließlich in den drei Sprachvorlagen im Zweig `CMD_WIFIMODE`: Die Anzeige verwendet jetzt `wifiMenuIdx`; der Sonderindex `ITEM_COUNT(wifiModeDesc)` wird vor dem Arrayzugriff abgefangen und zeigt das jeweilige lokalisierte Info-Label.
- Deutsch: `ats-mini/Menu_german.cpp.src` enthält die Korrektur bereits (aktueller Blob-SHA: `f363a715c350b3de2b64b7f1ce04c82a8e7570cf`). Englisch und Italienisch wurden jetzt korrigiert: `ats-mini/Menu_english.cpp.src` Commit `598576c272263d86aff0a1d1a5e542e19ec775a8`; `ats-mini/Menu_italian.cpp.src` Commit `784786bd7f71600d0127a17cadd7ef469ef96089`.
- Labels für den Sonderindex: Deutsch `WLAN-INFO`, Englisch `Wi-Fi Info`, Italienisch `Info Wi-Fi`.
- Keine Änderungen an `Network.cpp`, `Draw.cpp`, WLAN-Verbindungslogik, Flight Radar oder FT8/FT4.
- Korrektur abgeschlossen und als erledigt dokumentiert. Der frühere offene Diagnose-/Arbeitsauftrag wurde entfernt. Keine Änderungen an `Network.cpp`, `Draw.cpp`, WLAN-Verbindungslogik, Flight Radar oder FT8/FT4.
