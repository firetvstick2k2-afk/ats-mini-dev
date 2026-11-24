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
