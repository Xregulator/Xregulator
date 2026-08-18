# Xregulator (Public Mirror)

### [www.xengineering.net](https://www.xengineering.net)

**Product information, documentation, ordering, and support: [www.xengineering.net](https://www.xengineering.net)**

Xregulator is a real product built and sold by X Engineering. This repository is the open source code that runs on it. If you are looking for what the device does, how to install it, wiring diagrams, or how to buy one, start at [www.xengineering.net](https://www.xengineering.net) — not here.

---

## Overview

This repository is a  public mirror of the Xregulator project.

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

should be able to understand and modify this code with AI help.  Claude has been the best for me (early 2026).

My typical workflow is:
1. Paste `Xregulator.ino` into Claude with whatever my request is
2. Ask what other files/functions it needs in order to answer questions or make modifications
3. Paste the additional info as needed.  The codebase is too large to upload all at once, it's something like 50k lines not including libraries.

Because the code is organized in a predictable, mostly flat way, the above works well.

## Web Interface (`web_src/` folder)

This folder contains the web files served by the ESP32 over WiFi to a client device on the same network, such as your phone or laptop.

Like many web projects, it contains:
- `index.html` for displayed structure/content
- `script.js` for programmatic behavior
- `styles.css` for visual styling

Additional supporting web assets may also be present.  Workflow for the web files is much the same as with the ESP32 firmware.  Often you'll end up needing to work with several files in parallel to accomplish goals. 

## Design Philosophy

The codebase is intentionally kept as a monolith, and mabye 90% of variables are global.

This goes against conventional software engineering practices, but it has advantages for this project:
- easier navigation by beginners
- easier AI-assisted development (lower token spend)

The latter matters more than just about anything else! 

The goal was fast iteration, as an individual project, but is now moving to clarity, as ideally the open community starts to help.

In the meantime, please feel free to email me (joe@xengineering.net) with any bugs, feature requests, comments.  Or use the built in tools here on github. I haven't figured those out yet, but someday..
