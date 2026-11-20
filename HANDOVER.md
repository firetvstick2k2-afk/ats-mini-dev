# HANDOVER – ATS Mini Dev

## Projektziel und verbindliche Reihenfolge

**Zielrepository:** `firetvstick2k2-afk/ats-mini-dev`  
**Zielbranch:** `main`  
**Quellrepository:** `firetvstick2k2-afk/Mini_OS`  
**Quellbranch:** `dev/first-build-preparation`

Neue bzw. in Mini_OS vorhandene Funktionen sollen kontrolliert in ATS Mini Dev integriert werden. ATS Mini Dev bleibt das Zielprojekt; Mini_OS ist die Quelle für die neuen bzw. modularisierten Implementierungen.

Verbindliche Prioritäten:
1. **FT8-Decoder – höchste Priorität.** Zuerst technisch vollständig untersuchen und stabil portieren.
2. **DAB – zweite Priorität.** Nach FT8 angehen.
3. Weitere Funktionen erst danach und nach gemeinsamer Priorisierung.

## Arbeitsregeln

- Vor jeder Änderung dieses Dokument und den aktuellen Branch-/Dateistand lesen.
- Vor einem Port die Quell- und Zielimplementierung sowie deren Aufrufstellen, Hardwareannahmen, Abhängigkeiten und Ressourcenbedarf prüfen.
- Bestehende ATS-Mini-Dev-Funktionen nicht unnötig ersetzen oder parallel nachbauen.
- Änderungen klein und nachvollziehbar halten; nach jeder Codeänderung dieses Handover aktualisieren.
- Keine GitHub Actions starten, keine Builds auslösen und nicht flashen. Builds und Hardwaretests führt der Nutzer selbst aus.
- Build-, Laufzeit- und Hardwareerfolg nur dokumentieren, wenn sie tatsächlich nachgewiesen bzw. vom Nutzer bestätigt wurden.
- Keine unbelegten Annahmen über Audio-Gain, Lautstärke, UTC-Zeit oder Resetursachen als Fakten behandeln.

## FT8-Port – Startanalyse (2026-11-20)

### Mini_OS-Quellstand

Untersuchte Dateien auf `firetvstick2k2-afk/Mini_OS`, Branch `dev/first-build-preparation`:

- `firmware/ats-mini-os/src/digital/ftx/FtxDecoder.h`
- `firmware/ats-mini-os/src/digital/ftx/FtxDecoder.cpp`
- `firmware/ats-mini-os/src/digital/ftx/FtxDecoderService.h`
- `firmware/ats-mini-os/src/digital/ftx/FtxDecoderService.cpp`
- `firmware/ats-mini-os/src/audio/AudioCaptureService.h`
- `firmware/ats-mini-os/src/audio/AudioCaptureService.cpp`

Die Decoder-Engine basiert auf der eingebundenen `ft8_lib`-Bibliothek (`ft8_lib/common/monitor.h`, FT8/FT4-Protokoll). `FtxDecoder` ist als DSP-/Slot-Engine gekapselt und greift selbst nicht auf GPIO, ADC, WLAN oder Display zu. Der Aufrufer liefert 4-kHz-Samples und startet Empfangsslots an UTC-Grenzen.

### Beobachtete Architektur und Schnittstellen

- `AudioCaptureService`: ATS-Mini-Audioeingang an GPIO11, 4.000 Samples/s, Blöcke zu 24 Samples. Ein Capture-Task verteilt die Daten an Subscriber. Die Block-Zeitstempel werden aus `gettimeofday()` abgeleitet.
- `FtxDecoderService`: FreeRTOS-Queue mit 160 Audio-Blöcken, eigener Worker-Task (Stack 8192 Byte, Priorität 2, Core 1), Engine-/Result-Mutex und Diagnosezähler.
- Die Audio-Callback-Funktion kopiert die Samples in einen Queue-Chunk und berechnet zusätzlich den AC-RMS-Pegel. Queue-Überläufe werden über `droppedBlocks()` gezählt.
- FT8-Slotlänge: 15 Sekunden; FT4: 7,5 Sekunden. Blöcke, die eine Slotgrenze überqueren, werden samplegenau geteilt.
- Der Service verlangt derzeit `wifiOff == true` und `utcSynchronized == true`; das sind Eintrittsbedingungen des Mini_OS-Dienstes, keine intrinsischen Anforderungen der DSP-Engine.
- Der Audio-Capture-Service verweigert den Start im Buildprofil `LILYGO_SI473X`, da GPIO11 dort SPI-MOSI und kein ATS-Mini-Audioeingang ist.
- Die FT8-Anzeige in Mini_OS wurde zuletzt auf ein großes, wasserfallfreies Textlayout umgestellt. Der Decoder-Datenpfad und die interne Waterfall-Erfassung bestehen weiterhin.
- Die Audio-LED zeigt nur ADC-Aktivität (RMS), nicht FT8-Synchronisation oder erfolgreichen Decode.

### Bekannte offene Risiken aus dem bisherigen Mini_OS-Teststand

- Der Nutzer berichtete, dass der FT8-Bildschirm startet, der Decoder aber nach etwa 20–30 Sekunden zurücksetzt. Ursache ist nicht geklärt; ein serieller Reset-/Crash-Log ist erforderlich.
- Es gibt noch keinen bestätigten Build- oder Hardwaretest des aktuellen Mini_OS-FT8-Stands.
- Diamond-Radio-Menübeobachtung: Nach einer Erhöhung von „Volume“ erschienen FT8-Nachrichten. Ob „Volume“ nur den Lautsprecher oder auch den ADC-/Decoder-Signalweg beeinflusst, ist unbekannt. Keine Gain-Kopplung unterstellen.
- Ziel bleibt ein vollständig lokaler/offline FT8-Betrieb ohne WLAN/Internet/Smartphone. UTC soll aus einer bereits verfügbaren lokalen Zeitquelle kommen, sofern sie zuverlässig ist.
- Das neue Großtextlayout soll erhalten bleiben.

### ATS Mini Dev – bisheriger Abgleich

- README des Zielprojekts beschreibt ATS Mini V4 / SI4732 / ESP32-S3 und die bestehenden Erweiterungen bis Firmware V1.31.
- Eine GitHub-Code-Suche nach `FT8`, `ft8`, `FTX` und Decoder-/Audio-Begriffen lieferte im Zielrepository keine Treffer. Das ist **kein Beweis**, dass keine passende Implementierung oder verwandte Audio-/DSP-Infrastruktur existiert; die Suche kann unvollständig sein.
- Eine Datei `HANDOVER.md` war im Zielrepository auf `main` nicht vorhanden.
- Vor dem eigentlichen Port müssen Zielstruktur, Board-/GPIO-Konfiguration, Audio-/ADC-Pfad, Zeitquelle, Task-/Heap-Strategie und Menü-/Display-Einstiegspunkte direkt aus dem Zielcode ermittelt werden.

### Portierungsplan – FT8

1. Zielrepository vollständig nach Audioeingang/ADC, GPIO11, Zeit-/UTC-Service, FreeRTOS-Tasks, Displayseiten, Menüeinträgen, Decoder-/DSP-Bibliotheken und Build-Konfiguration durchsuchen.
2. ATS Mini Dev Hardwarepfad mit Mini_OS `AudioCaptureService` vergleichen. Insbesondere Pinbelegung, ADC-Auflösung/Referenz, Sample-Timing, Eingangsfilterung und Zeitstempel verifizieren.
3. Prüfen, ob `ft8_lib` samt Lizenz, Build-Einbindung, Konfiguration und Abhängigkeiten bereits vorhanden ist oder sauber übernommen werden muss.
4. Decoder-Engine zunächst isoliert integrieren; erst danach Capture-Service, UTC-Slotsteuerung, Ergebnisübergabe und Anzeige anbinden.
5. Speicher- und Laufzeitrisiken untersuchen: interne SRAM-/PSRAM-Belegung, Queue-Größe, Task-Stack, FFT-/Waterfall-Puffer, Decoder-Spitzenlast und Exit-/Restart-Lifecycle.
6. Resetursache aus Mini_OS anhand serieller Logs untersuchen; Fehler nicht blind in ATS Mini Dev übertragen.
7. Erst nach statischer Prüfung dem Nutzer einen Build-/Hardwaretest vorschlagen. Keine Actions/Builds/Flash durch den Assistenten.

**Portstatus:** begonnen – Quellarchitektur von Mini_OS teilweise analysiert; Zielarchitektur und Abhängigkeiten noch nicht ausreichend geprüft. Noch keine FT8-Codeänderung im Zielrepository.

## DAB – zweite Priorität

DAB ist ausdrücklich als nächster Port nach Abschluss/Stabilisierung von FT8 vorgemerkt. Implementierungsdetails, Quellpfade, Hardwareanforderungen und Portierungsumfang sind noch zu inventarisieren. Bis dahin keine DAB-Codeänderungen.

## Letzter dokumentierter Stand

- Datum: 2026-11-20
- Zielbranch: `main`
- Quellbranch: `dev/first-build-preparation`
- Änderungen am ATS Mini Dev Code: keine im Rahmen der Startanalyse.
- Builds / GitHub Actions / Flash: nicht ausgeführt.
