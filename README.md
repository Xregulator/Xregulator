# Xregulator (Public Mirror)

### [www.xengineering.net](https://www.xengineering.net)

**Product information, documentation, ordering, and support: [www.xengineering.net](https://www.xengineering.net)**

Xregulator is the first product sold by X Engineering. This repository is the open source code it runs on. If you are looking for what the device does, how to install it, wiring diagrams, or how to buy one, start at [www.xengineering.net](https://www.xengineering.net).

---

## Overview

This repository is a public mirror of the Xregulator project.

It contains:
- Core firmware that runs on ESP32 (`.ino` files)
- Web interface files (`web_src/`)

It intentionally excludes other (irrelevant) files present in the private repository.

## Firmware Structure

The starting point is:

`Xregulator.ino`

This file contains:
- All global variables
- `setup()`
- `loop()`

All primary program flow and state live here, and it's just like the layout of pretty much every beginner Arduino program.

## Supporting Files

The remaining `function_X.ino` files contain supporting functions, loosely divided by purpose.

The numeric prefixes exist because the Arduino IDE uses a tabbed project model, and file order can affect how things are compiled and presented. The `2_`, `3_`, `4_`, etc naming is mainly there to keep a fixed order inside the Arduino IDE environment.

## How to Work With the Code

Anyone who has:
- run the Arduino `Blink` example
- programmed in basically any language

should be able to understand and modify this code with AI help.  ClaudeCode has been the best for me (mid 2026).

Suggested workflow is:
1. Download repository
2. Open a ClaudeCode instance on that folder
3. Point it to docs.xengineering.net, but when in doubt, the code is the ultimate source of truth
4. Ask your questions/make your requests
5. Submit any feature changes or bug fixes to me either here with a pull request or by email joe@xengineering.net (It is preferable, if at all possible, not to create your own branches, as they will be unsupported.  It's of course too difficult to maintain infinite branches, at least with today's tools.)

This is what I do, and it works well!

## Web Interface (`web_src/` folder)

This folder contains the web files served by the ESP32 over WiFi to a client device on the same network, such as your phone or laptop.

Like many web projects, it contains:
- `index.html` for displayed structure/content
- `script.js` for programmatic behavior
- `styles.css` for visual styling

Additional supporting web assets may also be present.  Workflow for the web files is much the same as with the ESP32 firmware.  Often you'll end up needing to work with several files in parallel to accomplish goals.

## Design Philosophy

The codebase is intentionally kept as a monolith, and maybe 90% of variables are global.

This goes against conventional software engineering practices, but it has advantages for this project:
- easier navigation by beginners
- easier AI-assisted development (lower token spend)

The latter matters more than just about anything else!
