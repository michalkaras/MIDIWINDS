**Purpose**
- **Intent:** Help AI coding agents be immediately productive editing this Arduino-based instrument project (main sketch: [ButtonsAI/ButtonsAI.ino](ButtonsAI/ButtonsAI.ino)).
- **Scope:** Focus on build/upload, hardware-aware coding patterns, and repository-specific conventions.

**Big Picture**
- **Entry Point:** `ButtonsAI/ButtonsAI.ino` is the main Arduino sketch and contains pin mappings, setup/loop, and input handling.
- **Layers:** Hardware layer (pin defs, debouncing, interrupts) → Input processing (button state / sensors) → Control/state machine (note selection, timing) → Output (MIDI, DAC, speaker, serial).
- **Why structure matters:** Low-latency and deterministic behavior are required (keep ISRs short, avoid blocking calls in `loop()`).

**Developer workflows**
- **Build & upload (arduino-cli):**
```bash
arduino-cli compile --fqbn arduino:avr:uno ButtonsAI
arduino-cli upload -p /dev/tty.usbmodemXXXX --fqbn arduino:avr:uno ButtonsAI
```
- **Alternative (PlatformIO):** If `platformio.ini` exists, use:
```bash
pio run -t upload
```
- **Serial debug:** Use `arduino-cli monitor -p /dev/tty.usbmodemXXXX -b 115200` or PlatformIO Serial Monitor.
- **Search-commands:** Look for `pinMode`, `attachInterrupt`, `volatile`, `millis`, `debounce`, `PROGMEM` to find hardware-critical patterns.

**Project-specific conventions**
- **Single-sketch focus:** Keep `*.ino` as the runnable sketch; helper `.h/.cpp` files (if present) live alongside or under `src/`.
- **Pin definitions:** Defined at the top of `ButtonsAI/ButtonsAI.ino`. Update wiring diagrams and comments here when changing hardware.
- **No dynamic memory:** Prefer static buffers and avoid `new`/`malloc` on AVR-class MCUs.
- **Timing:** Use `millis()`-based timers instead of `delay()` for responsiveness.

**Concurrency and ISR rules**
- **ISR brevity:** ISRs must only set flags or counters (use `volatile` for shared variables).
- **Main-loop processing:** Poll flags set by ISRs inside `loop()` and run state transitions there.
- **Debounce:** Prefer debounce via state-machine or timer, not blocking waits.

**Libraries and dependencies**
- **Locate libs:** Check `lib/`, `libraries/`, `platformio.ini`, or `library.properties` for third-party dependencies. Example candidates: MIDI libraries, I2C sensor drivers.
- **Pin/port optimizations:** If the code uses direct port manipulation, preserve that pattern for performance—refactor only when you understand timing constraints.

**Testing and validation**
- **Hardware-in-loop:** Functional tests are manual with hardware connected. Use `Serial` logs and known input sequences to validate behavior.
- **Unit tests:** Only if `platformio.ini` or a test folder exists; otherwise, prefer careful simulation via small desktop harnesses.

**Change guidance for contributors**
- **When editing hardware logic:** Update top-of-file pin map and any README wiring diagram.
- **When adding features:** Add non-blocking timers, keep ISR changes minimal, and document expected electrical behavior (pull-ups, active-low).
- **When refactoring:** Run static searches for `volatile`, `cli()/sei()` (if used), and `noInterrupts()` — preserve critical sections.

**Helpful searches & examples**
- **Pin + ISR usage:** `grep -n "attachInterrupt\|pinMode" -R`
- **Timing patterns:** `grep -n "millis()\|delay(" -R`
- **Shared state:** `grep -n "volatile " -R`

**What to ask the maintainer**
- Exact target board / FQBN for `arduino-cli`.
- Any required external hardware (MIDI interface, DAC, sensors) and wiring diagrams.
- Preferred upload workflow (Arduino IDE, `arduino-cli`, or PlatformIO).

---

If you'd like, I can commit this file to git and push to a remote, or adjust wording first.
# Copilot / AI Agent Instructions — ButtonsAI

Short: this repo implements an ESP32-S3 BLE-MIDI wind-controller using button/touch fingering, a breath sensor, and optional display/IMU.

What to know up front
- Hardware: ESP32-S3, MPXV7002DP breath sensor (analog), MPU-6050 (I2C), optional SSD1306 OLED.
- Primary sketches: [ButtonsAI.ino](ButtonsAI.ino) and [ButtonsAI-1.ino](ButtonsAI-1.ino). Treat these as the canonical implementations.
- BLE MIDI: uses `BLEMidi`/NimBLE (not standard Serial MIDI). Libraries referenced in headers and comments: NimBLE-Arduino 1.4.3 and ESP32-BLE-MIDI 0.3.2.

Architecture / patterns
- Single-file Arduino sketches with `setup()` and `loop()` as entry points. Logic is immediate-mode: read sensors each loop, compute state, send MIDI events.
- Fingering table: a fixed exact-match mapping `FINGERINGS[]` (type `FingerMapEntry`) where `mask` is a bitmask of touch sensors and `pitch[3]` holds MIDI notes for three ranges. The agent must not change this mapping silently — it is data the user will tune.
- Touch-to-mask mapping: `TOUCH_ORDER[]` defines bit positions for elements of the mask. When modifying fingering logic, update both `TOUCH_ORDER` and `FINGERINGS` consistently.
- Breath handling: raw analog read from `BREATH_PIN` is converted by `breathToMidi()` into 0..127; breath-on uses `BREATH_THRESHOLD` (default 24). `offset` is used to calibrate ADC baseline.
- MIDI: calls go through `BLEMidiServer.noteOn(...)` / `noteOff(...)`. There are two channels used for main and aux duplicate notes (channel 0 and 1).

Important functions & symbols to inspect
- `readFingeringMask()` — how the current touch combination is read (uses `touchInterruptGetLastStatus`).
- `lookupPitchForMask(mask, rangeMode)` — exact table lookup for masked fingering.
- `breathToMidi(raw)` — ADC->MIDI mapping; uses `offset` and `map()`; change carefully for other ADC ranges.
- `FINGERINGS[]` and `TOUCH_ORDER[]` — core domain data; modifying these changes audible behavior.
- `BLEMidiServer` callbacks in `ButtonsAI-1.ino` (`connected`, `onControlChange`) — used to receive remote CCs (range/instrument) and reflect state.

Build / debug workflows
- Preferred: open the sketch in Arduino IDE or PlatformIO/arduino-cli for proper ESP32 board configuration. The project does not include `platformio.ini` or board FQBNs.
- Quick local compile: workspace contains a VS Code task that runs `clang` on the active file (label: "C/C++: clang sestavit aktivní soubor"). This is useful for quick syntax checks but is NOT a substitute for an Arduino/ESP32 toolchain.
- Runtime debugging: instrument with `Serial.print` (already present). BLE MIDI traffic is observable with a BLE MIDI host/app.
- Serial commands: sending newline-terminated text to the serial monitor supports quick range switching: `r0`, `r1`, `r2` map to the three ranges.

Project-specific conventions
- Fingering masks are exact matches — there is no fuzzy/fallback matching. If you need partial matching, implement it explicitly and document changes.
- Aux behavior: `AUX1_PIN` is a momentary switch (active-low). Holding it at attack duplicates the main note on channel 1.
- Delay and scan: main loop uses `delay(20)` for scanning; when tuning latency, change this first.

If you change things, update these places
- If you alter touch ordering or number of sensors: update `TOUCH_ORDER`, `TOUCH_COUNT`, and `FINGERINGS` together.
- If you change ADC scaling or sensor hardware: update `offset` and `breathToMidi()` and document the ADC expected range.

Examples to copy when making edits
- Add a new fingering: append an entry to `FINGERINGS[]` matching the existing `FingerMapEntry` shape.
- Add a new BLE CC handler: follow `onControlChange()` in [ButtonsAI-1.ino](ButtonsAI-1.ino) and use `BLEMidiServer.setControlChangeCallback()`.

Merge guidance for agents
- If a `.github/copilot-instructions.md` already exists, preserve any project-specific pointers and only add missing items from this file. Do not remove human-written rationale or calibration notes.
- Avoid proposing changes that alter hardware constants (pin numbers, offsets, thresholds) without explicit user confirmation.

Where to ask the human
- If a fingering mapping looks incomplete or ambiguous, ask: "Which physical touch sensor corresponds to `T1..T13`?" and "Do you expect masks to be exact or support fallbacks?"

Done. File maintained by human owner; ask before making hardware-related changes.
