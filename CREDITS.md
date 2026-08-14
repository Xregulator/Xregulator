# Credits & Third-Party Assets

## Images

### Loading-screen sailboat — `web_assets/boat_sloop.png`

- **Title:** "Sloop under full sail, close reaching, showing its fractional rig"
- **Author:** Stefan Ivanovich
- **Source:** <https://commons.wikimedia.org/wiki/File:Sloop_under_full_sail,_close_reaching,_showing_its_fractional_rig.jpg>
- **License:** CC BY-SA 3.0 — <https://creativecommons.org/licenses/by-sa/3.0/>
- **Modifications:** Background removed (cut out for use as the app's loading-screen graphic).

This project is open source. Per the Share-Alike (SA) term of CC BY-SA 3.0, this
derived image asset is likewise made available under CC BY-SA 3.0.

## Firmware libraries (custom `_xeng` forks, in `~/Documents/Arduino/libraries/`)

### VE.Direct frame handler — `VeDirectFrameHandler_xeng`

- **Origin:** VeDirectFrameHandler by Chris Terwilliger, derived from the Victron Energy
  reference implementation
- **Source:** <https://github.com/cterwilliger/VeDirectFrameHandler>
- **License:** MIT (Copyright (c) 2019 Victron Energy BV; portions Copyright (c) 2020
  Chris Terwilliger) — full notice kept in the library source and LICENSE file.
- **Modifications:** frameCounter for checksum-valid new-frame gating; corrupted-stream
  bounds fixes (frameIndex clamp in textRxEvent, overlong-name truncate-terminate,
  mName/mValue zero-init).

### PID controller — `PID_v1_xeng`

- **Origin:** Arduino PID Library v1.2.2 by Brett Beauregard
- **Source:** <https://github.com/br3ttb/Arduino-PID-Library>
- **License:** MIT — notice kept in the library source.
- **Modifications:** actuator-aware tracking anti-windup methods (described in the
  library source).
