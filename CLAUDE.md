# CLAUDE.md — Pebble Round 2 Apps (Monorepo Root)

This repo contains multiple Pebble apps and watchfaces targeting **Pebble Round 2** (platform: **gabbro**, 260×260 px, 64-color e-paper, round display). Each app lives in its own subfolder with its own `package.json` and optional app-level `CLAUDE.md`.

## Coding Guidelines

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

### 1. Think Before Coding

Don't assume. Don't hide confusion. Surface tradeoffs.

- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them — don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

### 2. Simplicity First

Minimum code that solves the problem. Nothing speculative.

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

### 3. Surgical Changes

Touch only what you must. Clean up only your own mess.

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it — don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: every changed line should trace directly to the user's request.

### 4. Goal-Driven Execution

Define success criteria. Loop until verified.

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

---

## Platform: Pebble Round 2 (gabbro)

All apps in this repo target gabbro unless their app-level CLAUDE.md says otherwise.

| Property       | Value                                           |
| -------------- | ----------------------------------------------- |
| Platform       | `gabbro`                                        |
| Display        | Round, 260×260 px, 64 colors                   |
| Buttons        | 4 (back, up, select, down)                      |
| Touch          | Yes                                             |
| Sensors        | Accelerometer, compass, HR, ambient light, mic  |
| Language       | **Alloy (JavaScript)** — ES2025, ES modules     |
| UI frameworks  | Piu (declarative) or Poco (procedural graphics) |

### Round display rules

- The screen is circular. Content at corners is clipped — keep text and UI elements within the inscribed circle.
- Use radial/centered layouts by default. Rectangular grid layouts waste space.
- Test edge-case positions near the bezel — anything past ~120 px from center risks clipping.
- For watchfaces, ensure the time remains readable at a glance; decorative elements go toward the edges.

### Alloy project structure

```
my-app/
  src/
    embeddedjs/
      main.js           # Watch code (runs on Pebble)
    pkjs/
      index.js          # Phone code (networking, location)
    c/
      mdbl.c            # C entry point for embeddedjs (usually don't modify)
  resources/            # Images, fonts
  package.json          # App manifest
```

- `embeddedjs/` runs ON the watch (Moddable XS engine).
- `pkjs/` runs on the PHONE — use it to proxy network requests via `fetch()` or WebSocket, get GPS location, etc. The watch has no direct internet.
- To create a new app: `pebble new-project --alloy <name>`
- To set app kind (watchface vs watchapp): edit `package.json` → `"watchapp": { "watchface": true }`

### Key Alloy APIs and patterns

**Networking:** The watch cannot make HTTP requests directly. Use the phone-side `pkjs/index.js` to call APIs via `fetch()`, then relay data to the watch via AppMessage. Alternatively, Alloy supports `fetch()` in embeddedjs that transparently proxies through the phone.

**Storage:** Use `localStorage` for simple string persistence, ECMA-419 key-value storage for structured data, or the file system API for larger blobs. Data persists across app launches.

**Sensors:** Accelerometer, battery, compass, heart rate, and ambient light are available via ECMA-419 sensor APIs. Button input uses the Pebble Button class.

**Touch:** Pebble Round 2 supports touch events — use Poco's drag events or Piu Behavior touch handlers.

**Dictation:** Speech-to-text is available via the dictation API. There is no raw audio capture.

**Vibration:** Haptic feedback via the vibration motor API.

**Wakeups:** Schedule the app to launch at a future time.

**FFI:** Call native C functions from JavaScript when Alloy APIs don't cover something.

### Common pitfalls

- **No floating point in C layer.** If you drop into FFI or write C, use `sin_lookup()` / `cos_lookup()` with `TRIG_MAX_ANGLE = 65536` for a full circle.
- **Memory is tight.** Don't create large arrays or deep object hierarchies. Pre-allocate where possible.
- **Battery awareness.** Throttle animations and sensor polling when battery is low. Check `battery.chargePercent`.
- **Resource cleanup.** Destroy all resources (layers, bitmaps, fonts) in unload/cleanup handlers.
- **No `www` fetch from watch.** All network requests go through the phone companion. If the phone is disconnected, network calls fail silently or error.

---

## Repo structure

```
pebble-apps/                  ← you are here
├── CLAUDE.md                 ← this file (shared conventions)
├── .claude/
│   └── skills/
│       └── pebble-watchface/ ← clone from coredevices/pebble-watchface-agent-skill
│           ├── SKILL.md
│           ├── reference/
│           ├── samples/
│           ├── scripts/
│           └── templates/
├── <app-1>/
│   ├── CLAUDE.md             ← app-specific context (what it does, special APIs used)
│   ├── package.json
│   └── src/
├── <app-2>/
│   ├── CLAUDE.md
│   ├── package.json
│   └── src/
└── ...
```

Each app is a standalone Pebble project with its own `package.json`. To build/test an app, `cd` into its folder first.

---

## Workflow

1. **Code** — Claude Code (web) writes/edits Alloy JS in the app's subfolder, committed to this GitHub repo.
2. **Build & test** — Open [cloudpebble.repebble.com](https://cloudpebble.repebble.com), import/pull the app's subfolder, build, and run the gabbro emulator in-browser.
3. **On-device test** — Enable Dev Connect in the Pebble mobile app → sideload the `.pbw` to a real Round 2.
4. **Publish** — Use `pebble publish` from CLI, or publish directly from CloudPebble. This generates screenshots/GIFs for all platforms automatically.

### Publishing checklist

Before publishing, ensure:
- Unique UUID in `package.json` (generate with `pebble generate-uuid`)
- App built with a non-beta SDK
- Large and small icons provided
- At least one `.pbw` release uploaded
- App kind set correctly (watchface vs watchapp)
- Quick View support added for watchfaces (see C tutorial / Alloy tutorial)

---

## References

- Developer docs: https://developer.repebble.com/
- Alloy guide: https://developer.repebble.com/guides/alloy/
- Alloy examples: https://github.com/Moddable-OpenSource/pebble-examples
- Watchface agent skill: https://github.com/coredevices/pebble-watchface-agent-skill
- CloudPebble: https://cloudpebble.repebble.com
- SDK install: https://developer.repebble.com/sdk/
- Publishing: https://developer.repebble.com/guides/appstore-publishing/publishing-an-app/
- FAQ: https://developer.repebble.com/faqs/
- Rebble Discord: #sdk-dev channel for help

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, clarifying questions come before implementation, apps build on first try in CloudPebble, and round-display clipping issues are caught before device testing.
