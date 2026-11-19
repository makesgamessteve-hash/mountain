# Design

## Context

See proposal.md (Why) for motivation and the capability specs under `specs/` for the required behaviour; this document explains how the first version is built.

Repository layout: `engine/include/mtn` (public headers), `engine/src/{core,script,render,platform}`, `assets/{scripts,shaders,textures,sounds,fonts}` (copied beside the executable), `tests/{unit,game,web}`, `web/` (Emscripten shell, pre-run script and JS library), `tools/` (asset generator, coverage and game-test runners, API doc check), `cmake/` (MinGW-w64 cross toolchain), `docs/` and vendored `third_party/lua` (Lua 5.4.8) and `third_party/stb` (`stb_image`, `stb_image_write`). Constraints: C17, SDL3, OpenGL 3.3 core on desktop and OpenGL ES 3.0 / WebGL2 on the web, Lua 5.4 game scripts, assets in folders beside the binary, at least 95 percent line coverage of engine sources, automated headless game tests, MIT code and CC BY 4.0 assets.

## Goals / Non-Goals

**Goals:**

- A platform-independent engine core that contains nearly all logic and is unit tested without a window, GPU or audio device.
- One renderer code path and one set of shader sources for OpenGL 3.3 core, OpenGL ES 3.0 and WebGL2.
- Content and game rules as data and Lua scripts, so new biomes, species, planets, room types and menus need no engine change.
- The real game is drivable headless with scripted input, so the platform and render layers are covered by game tests rather than mocks.

**Non-Goals:**

- Save games beyond settings, the last seed, the time speed and photos (the world is regenerated from its seed; position, time and house are not saved in this version).
- Bit-identical worlds across platforms (generation uses libm; worlds match in layout, not in every bit).
- macOS, iOS or Android native builds; multiplayer; localisation (English only).
- Beaches/sea level, alien or magic planet content, and a mod manager UI (only the data hooks they need; see Future Work).
- Testing WebXR on real headsets in CI.

## Decisions

### 1. Layered engine: core, script, render, platform

- **core** (`engine/src/core`, headers in `engine/include/mtn`): pure C17 without SDL or GL. Modules: `math` (vectors, matrices, frustum, oblique near-plane clipping, portal transforms), `rng` (PCG32, integer hashes, gradient/fBm/ridged noise), `terrain`, `sky` (planet, clock, seasons, sun and moons, sky colours), `weather`, `flora`, `fauna`, `player`, `house` and `room` (magic house and its interior), `mesh` (vertex buffers, boxes, cylinders, billboards, rocks, voxels with ambient occlusion), `photo` (filters, scaling, PNG via `stb_image_write`, gallery list), `input` (actions, touch layout, immediate-mode UI with keyboard/gamepad focus), `audio` (WAV parser, software mixer with fades and panning), `vfs` (assets/mods resolution) and `game` (the simulation step tying everything together).
- **script** (`engine/src/script/lua_api.c`): the Lua state, the restricted environment and about a hundred API functions in sixteen libraries. It depends only on core, so the whole API is unit tested.
- **render** (`engine/src/render`): GL function loading and one renderer file that turns core state into draw calls.
- **platform** (`engine/src/platform/app.c`): SDL3 window, GL context, events, audio stream, user folders, photo files, the Emscripten/WebXR glue and the game-test driver.
- Alternative considered: a single game module (rejected: untestable without GL).

### 2. Determinism

World generation is a pure function of the seed and parameters: PCG32 sequences and integer hashes of `(seed, coordinates)` for per-cell decisions, fixed grids and `-ffp-contract=off` (`/fp:precise` on MSVC). A world checksum (FNV-1a over heights quantised to 1 cm, water, biomes, trail, cairns and viewpoints) is exposed to scripts and pinned by unit tests on the same platform. Generation uses libm (`powf`, `atan2f`, `logf`...), so different platforms can differ in the last bits; the layout is the same. The simulation uses the real frame time clamped to 0.1 s; game tests run with a fixed 1/60 s step so they are reproducible.

### 3. World generation pipeline

A domain-warped radial main mountain with ridged noise, surrounding ridged ranges, two or three seeded outflow valleys that cut through the ranges (preventing a moat around the peak) and a regional tilt → particle hydraulic erosion (brush radius 2, deterministic droplet order) → priority-flood filling; basins deeper than 1.5 m and larger than 24 cells become lakes at their spill height → flow accumulation on the filled surface for moisture → biomes from altitude fraction, slope, moisture and noise, with tree line and snow line from scripts (`world.params`) → trailhead: lowest gentle dry point on rings at 86/68/50 percent of the playable radius → A* on a half-resolution grid with 16 neighbours (switchbacks), cost growing with the square of the grade, heavy penalties for very steep cells, water and leaving the playable disc → Chaikin smoothing, resampling every 2 m, a distance field to the trail for terrain shading and plant clearance → cairns at the trailhead, every 80 m beside the path and on the summit → viewpoints: openness along 16 directions (24-step ray march over 900 m), altitude and a bonus for facing the summit, greedy selection with 220 m spacing, summit first. The playable area is a disc of 82 percent of the half size; outward movement is damped over its last 50 m and clamped at the edge; beyond it the full terrain continues as scenery.

### 4. Sky, time, seasons and weather

- Clock in game days; one game day lasts `day_length` real seconds (default 1440). The year starts with spring; four seasons of `days_per_season` days. Season weights are smooth triangular blends around each season's middle. Solar declination peaks in the middle of summer; sun direction from latitude, declination and hour angle (noon due south). Moons have a period and phase; their position is the sun's hour angle shifted by the phase, and the sky shader lights the moon disc from the real sun direction, which yields correct phases. Stars are hashed per direction in the shader and rotated about the celestial pole by sidereal time; the milky way is a noise band.
- Sky colours come from planet data (day, sunset, night zenith and horizon) blended by sun elevation, desaturated and darkened by cloud cover; clouds are a noise layer projected on a plane, moved by the wind and tinted by sunset.
- Seasonal looks: per-species leaf colours for the four seasons blended by the season weights; deciduous leaves vanish in winter, blossoms appear in spring, flowers follow their bloom seasons. The snow line is a per-season table (66/82/74/36 percent of the height range) shifted by the scripted summer snow line and lowered towards zero by fresh snow (2 game hours to full cover, melting over 6 hours, 24 in winter).
- Weather: seven states with a parameter vector each (cloud cover, precipitation, wind, fog, lightning, darkness), season-weighted next-state tables from `content/weather.lua` (never the same state twice), durations of 2–8 game hours, blends over 45 real seconds at any time speed. Temperature = seasonal base + diurnal swing − 16 degrees over the height range; precipitation is snow below 1 degree. Lightning every 10–40 real seconds in storms, thunder 1–4 s later from a random side.

### 5. Flora and fauna

- Flora: separate jittered-grid passes for trees (8 m), bushes (11 m) and boulders (17 m) within the playable disc plus 350 m; each candidate picks a species by suitability (habitat × density × patch noise). Plants are sorted into 96 m chunks. The renderer builds chunk meshes lazily, nearest first, at three levels: full trunks, branches and many leaf-cluster billboards below 160 m; simple trunks and a few big clusters below 520 m; one or two upright sprites beyond. Ground cover (grass, flowers, ferns, mushrooms, stones) is generated per 8 m cell from the cell hash within about 48 m of the camera and thins out from 30 m. Wind sway is done in the vertex shader. The house hides plants within 6.5 m (except its tree) and unhides them when it leaves.
- Fauna: up to 96 animals; every 1.5 s one species may spawn a group if active (hours, seasons, no rain for insects) and below its group cap. Ground animals and birds spawn 45–250 m away, insects 4–30 m away with a fade-in; animals beyond 300 m or out of season fade out. Quadrupeds idle, graze and walk to nearby habitat points, and trot away when the player is closer than their comfort distance, detouring around water and cliffs; birds circle in flocks, the eagle soars high, butterflies flutter, fireflies blink. Quadrupeds and birds are low-poly meshes built every frame; insects are sprites.

### 6. Player and input

A first-person walker (eye 1.7 m, walk 2.6 m/s, run 5.2 m/s, about 1 m jump) on a pluggable collider: ground height and surface, push-out and walkability callbacks. Outside, ground is the terrain or the house's porch, stairs and platform; obstacles are trunks, boulders, cairns and the house walls (open in the doorway when the door is open); deep water is not walkable. Inside, the floor is flat and walls, furniture and the front door are boxes. Moves that would climb steeper than about 50 degrees lose their uphill part and slide along the slope. Input is a set of named actions; the key map comes from `content/controls.lua`, gamepad, touch and XR mappings are fixed in the engine. Touch controls (floating stick, look drag, buttons) are laid out and hit-tested in core.

### 7. Magic house

- Site search: candidates on rings of 0, 3, 6 and 9 m around a point 8 m ahead of the player (never closer than 5.5 m to the player). Variants: a tree of at least 10 m within 4.5 m → tree house on a platform 3.6 m up; a height range over the footprint above 2.4 m (up to 9 m) → house built into the hillside, facing downhill, with a voxel rock outcrop embedded in the uphill ground; water within 11 m → stilts; else a cottage on a stone foundation. Footprints in water or outside the playable area are rejected with a reason.
- Exterior: textured log walls with door and window holes, protruding log ends, gable roof with shingles, moss or lichen by biome, snow by shader, voxel stone chimney with smoke, glowing windows and lantern, porch, stairs down to the lowest ground, flower beds and stepping stones (cottage), railings and supports (tree, stilts), ivy.
- Interior space: room n spans z from 9n to 9n+9 in its own coordinate system, so neighbouring rooms are physically adjacent and doorways need no portals. Room 0 is the script's cabin type; rooms 1.. are drawn from the other types in per-block shuffled orders that never repeat a type twice in a row. The game keeps the current room and its neighbours built; furniture prefabs in C are placed by script data.

### 8. Portal rendering for the door and windows

A portal is a frame (origin and forward axis) in each of two spaces; `mtn_portal_transform(a, b) = b · rotY(π) · a⁻¹` maps one space into the other. Portals: the exterior door ↔ the cabin's front door, and every room window ↔ a target frame in the world (room 0: the house's side wall; other rooms: scenic viewpoint `(seed + n − 1) mod count`). Rendering: draw the current space, mark the visible opening in the stencil buffer, push depth to the far plane inside it, draw the other space with the transformed view and an oblique near plane on the target portal plane, then restore the opening's depth and clear the stencil. Recursion depth is one. Crossing the open door's plane moves the player through the same transform, so entering and leaving is seamless.

### 9. Retro rendering pipeline

The scene renders into a low-resolution target (RGBA8 + depth24/stencil8) of 180/216/270/360 lines (width from the window aspect): sky as a full-screen triangle; terrain chunks of 32 cells at three detail levels with skirts and per-vertex ambient occlusion, seasonal grass and snow line in the shader; lakes with fresnel and sun glint; trail strip and voxel cairns; flora chunks; ground cover; animals; the house; portals; blended glass, glows, sprites and particles; rain/snow particles wrapped around the camera. One generic world shader handles meshes, voxels and billboards (per-species leaf colour and amount uniforms). A post pass quantises to 32 levels per channel with a 4×4 Bayer ordered dither, previews the camera filter and draws the viewfinder and flash; it is upscaled with nearest filtering. The UI (pixel font and icons, batched quads, integer scale) is drawn afterwards, so photos never contain it. Shaders live in `assets/shaders`; the loader prepends `#version 330 core` or `#version 300 es` with precision qualifiers.

### 10. Photos

The post pass is rendered again into a low-resolution RGBA8 target without overlays, read back, filtered on the CPU, upscaled by the smallest integer factor reaching 720 lines and encoded with `stb_image_write` (PNG, deflate). Files go to `<user data>/photos/mountain-<seed>-<YYYYMMDD-HHMMSS>[-n].png`. The gallery lists files newest first; thumbnails are shrunk by an integer factor and drawn with nearest filtering. On the web the user folder is an IDBFS mount synced after writes and photos are downloaded as Blobs.

### 11. Scripting layer

- Vendored Lua 5.4. `assets/scripts/main.lua` defines `init(reloaded)`, `update(dt)`, `action(name, pressed)` and `draw()`; content lives in `content/earth.lua` (planet, biomes, plants, animals), `content/rooms.lua`, `content/controls.lua`, `content/weather.lua`; `hud.lua` and `ambience.lua` draw the interface and mix the soundscape.
- Restricted environment: base, table, string, math, utf8 and coroutine only, a safe `os` subset, no `io`, `dofile`, `loadfile` or native modules; `require` loads `scripts/<name>.lua` through the asset/mod folders; `storage` writes named text files only in the user folder.
- Every call is protected; errors with tracebacks are shown on screen and the game keeps running. F5 performs a soft reload in the same Lua state (cached modules are dropped), so C state and running game-test coroutines survive.
- `docs/script-api.md` documents every function; `tools/check_api_docs.py` (a CTest) fails when a registered function is missing.

### 12. Game-test mode

`mountain --test <script.lua> --data-dir <dir> [--frames N]` runs the real game with a fixed 1/60 s step and a Lua coroutine scenario. The `test` library pushes real SDL events (keys, mouse motion/buttons/wheel, finger events, an SDL virtual gamepad), waits for frames or seconds, checks conditions, takes screenshots and finishes with a pass/fail exit code; script errors, crashes and the frame limit fail the test. `tools/run_game_test.sh` runs it under Xvfb with Mesa software OpenGL and the dummy audio driver; `tests/game/failures.sh` checks clean failures for broken or missing assets via `mods/` overrides.

### 13. Web and WebXR (Emscripten)

SDL3 is built from source for Emscripten (CMake `find_package` or FetchContent), linked with WebGL2 only; `assets/` is packaged with `--preload-file` into `mountain.data` beside `mountain.wasm`; `web/pre.js` mounts IDBFS at `/user`; `web/mountain_lib.js` provides touch detection, downloads, storage sync and WebXR. The shell fills the page, shows loading progress, explains a missing WebGL2 and offers "Enter VR" when `immersive-vr` is supported. In VR the main loop is paused and `XRSession.requestAnimationFrame` calls `mtn_web_xr_frame` with controller axes/buttons and per-view matrices and viewports; each eye renders to the low-resolution target and is post-processed into the XR layer; the left stick moves relative to the head, the right stick snap-turns 30 degrees. Ending the session resumes the normal loop.

### 14. Coverage and CI

- `tools/coverage.sh` builds with `--coverage`, runs the unit tests, the API doc check and all game tests, and runs `gcovr --filter engine/ --fail-under-line 95` on that build only.
- GitHub Actions: Linux (dependencies, coverage gate, screenshots and report as artifacts, release build), Windows (MSVC, unit tests, packaged build), Web (emsdk, build, Playwright smoke test on desktop, an emulated phone and a mocked WebXR device, site artifact), OpenSpec (`validate --all --strict`), and on `main` a deploy job that needs all others and force-pushes an orphan `gh-pages` commit containing only the web build and the licence notices.

### 15. Defaults

Latitude 46 degrees north; a new world starts at 08:00 on day 1 (spring); one moon with an 8-day phase cycle; walk 2.6 m/s, run 5.2 m/s; weather states 2–8 game hours with 45 s blends; fresh snow 2 hours to full cover; fauna spawn at least 45 m away (insects closer) and leave beyond 300 m; at least 8 viewpoints spaced 220 m apart; 32 colour levels per channel; gamepad left-stick click toggles running, Back opens the gallery, RT shoots, D-pad zooms and changes filters, D-pad/stick move the menu focus; F filter, F3 frame time, F11 fullscreen, F5 reload; a "To trailhead" menu entry; the gallery room frames the six newest photos.

## Risks / Trade-offs

- [Software OpenGL in CI is slow and differs from GPUs] → Game tests assert on game state, run small windows with a fixed timestep, and only use screenshots as artifacts for humans.
- [Floating-point differences across platforms] → Same-platform checksums are exact; cross-platform layouts match but are not bit-identical (accepted).
- [WebXR cannot be tested on real hardware in CI] → The smoke test covers the Enter VR button with a mocked `navigator.xr`; the frame path is shared with the desktop renderer (and exercised by a game test); headset playtests on `gh-pages`.
- [Platform and render code threaten the 95 percent gate] → Thin layers covered by game tests in the coverage run; Emscripten- and Windows-only code is preprocessed out on Linux.
- [Generation time on phones] → The web build uses a smaller grid (385 points, 7 m cells) and fewer erosion droplets; flora chunks build lazily.
- [Low internal resolution in VR] → Each eye uses a third of its native resolution, never fewer than 120 lines.
- [Mods run third-party scripts] → Restricted Lua environment with asset/mod and user-folder access only.
- [Browser storage can be evicted] → A download button right after each photo and in the gallery.

## Migration Plan

Greenfield project: no data migration. Every push to `main` builds all targets and, when all jobs pass, republishes `gh-pages`. Rollback is a revert on `main`.

## Open Questions

- Compressed audio for longer ambience loops (16-bit PCM WAV for now).
- Tuning of erosion, biome thresholds, species densities and comfort distances (script data).

## Future Work

- Beaches: the terrain already has a `sea_level` parameter; add a beach biome and coastal species as script data.
- Alien or magic planets: alternative planet, biome, flora and fauna data sets (several moons, other sky colours and day lengths), selectable in the menu.
- A mod manager to enable, order and disable mods instead of a single `mods/` folder.
