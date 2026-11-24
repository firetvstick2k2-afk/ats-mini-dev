# Flight Radar Handoff

## Status – 2026-11-24

- The `development` branch has been reset to the last user-confirmed hardware-working baseline, Build #69 (`7825f22feda8d095a542bf819a07f9bf62c23a7c`).
- Flight Radar changes introduced after that baseline are intentionally not present in the current test state. The prior Flight Radar implementation and its later NVS-load adjustment must not be assumed to exist in this branch state.
- `main` was not changed. No build, flash, or hardware test was initiated by the assistant.
- The only firmware commit layered on top of Build #69 is the isolated About branding update `cb0b305acdbb9fc7d79d2b7d0c8d0901fb510b7a`; it does not modify Flight Radar.


## 2026-11-24 – About-Anzeige: Dispatch-Reihenfolge korrigiert

- Ursache: `drawScreen()` behandelte `CMD_ABOUT` bereits in der allgemeinen Settings-Popup-Abfrage (`currentCmd <= CMD_ABOUT`). Dadurch wurde `drawSettingsPopup()` mit der alten Beschriftung `ATS MINI V4` ausgegeben; der spätere Aufruf von `drawAbout()` war unerreichbar.
- Korrektur in `ats-mini/Draw.cpp`: obere Grenze der allgemeinen Settings-Abfrage von `<= CMD_ABOUT` auf `< CMD_ABOUT` geändert. `CMD_ABOUT` erreicht nun den dedizierten grafischen About-Renderer in `About.cpp`.
- Nur `Draw.cpp` und diese Übergabedokumentation geändert. Kein Build, kein Flash und kein Hardwaretest durch den Assistenten; Nutzer soll den nächsten OSPI-DE-Build selbst starten und die About-Seite prüfen.
