# Design

## Context

See proposal.md (Why) for motivation and the capability specs under `specs/` for the required behaviour; this document only explains how the first version is built.

The repository is a greenfield scaffold: `engine/{include/mtn,src/{core,render,platform,script}}`, `assets/{scripts,shaders,textures,sounds,fonts}`, `tests/{unit,game}`, `web/`, `cmake/` (a MinGW-w64 cross toolchain), `tools/font8x8_basic.h` (public-domain 8x8 font) and vendored `third_party/lua` (Lua 5.4) and `third_party/stb` (`stb_image`, `stb_image_write`). Constraints from the project context: C17, SDL3, OpenGL 3.3 core on desktop and OpenGL ES 3.0 / WebGL2 on the web, Lua 5.4 game scripts, assets in folders beside the binary, at least 95 percent line coverage of engine sources, automated headless game tests, MIT code and CC BY 4.0 assets.

## Goals / Non-Goals

**Goals:**

- A platform-independent, deterministic engine core that contains nearly all logic and can be unit tested without a window, GPU or audio device.
- One renderer code path and one set of shader sources for OpenGL 3.3 core, OpenGL ES 3.0 and WebGL2.
- Content and game rules as data and Lua scripts, so new biomes, species, planets, room types and menus need no engine change.
- The real game is drivable headless with scripted input, so platform, render and script layers are covered by game tests rather than mocks.
- Same seed gives bit-identical worlds on Windows, Linux and the web.

**Non-Goals:**

- Save games beyond settings, the last seed and photos (the world is regenerated from its seed; position, time and house are not saved in this version).
- macOS, iOS or Android native builds; multiplayer or networking; localisation (English only).
- Beaches/sea level, alien or magic planet content, and a mod manager UI (only the data hooks they need; see Future work).
- Testing WebXR on real headsets in CI.

## Decisions

### 1. Layered engine: core, render, platform, script

- **core** (`engine/src/core`, public headers in `engine/include/mtn`): pure C17, no SDL or GL includes. Modules: math (vectors, matrices, quaternions, frustum), RNG (PCG32 plus a stateless integer hash for per-cell decisions), noise (value/gradient noise built on the integer hash), terrain (heightfield, ridged and domain-warped noise for the main mountain and ranges, particle-based hydraulic erosion, priority-flood lake filling, biome classification, trail A* with slope/water cost and cairn spacing, viewpoint scoring), clock and celestial model (calendar, sun, moons, sidereal rotation), sky model (gradient colours, fog colour, light colours), weather (Markov chain, blending, temperature, wind, lightning scheduler), seasons (year phase, foliage keys, snowline, snow cover field), flora (placement, LOD selection, ground-cover cells), fauna (agents, steering state machines, spawn ring), player (fixed-step controller, slope limit, colliders), house (site analysis, variant builder, door state, interior room chain, portal transforms), mesh and voxel builders (CPU-side vertex buffers), PNG encoder, audio mixer (voices, ramps, pan/attenuation), input (action map state, touch layout and hit testing, joystick dead zones), UI batching (pixel-font glyph quads, panels) and a virtual file system.
- **render** (`engine/src/render`): OpenGL resource management and draw passes; consumes CPU buffers built by core.
- **platform** (`engine/src/platform`): SDL3 window and GL context, event pump translated into core input, touch, gamepads (including SDL virtual joysticks for tests), audio stream fed by the core mixer, paths (`SDL_GetBasePath` for assets, `SDL_GetPrefPath` for user data), and the Emscripten glue.
- **script** (`engine/src/script`): Lua 5.4 state, bindings, sandbox, error overlay hand-off and hot reload.
- Alternatives considered: a single monolithic game module (rejected: untestable without GL), or C++ with an existing engine (rejected: project constraint is C and a custom retro look).

### 2. Determinism

- World generation runs on fixed-size grids (heightfield of about 512 x 512 cells over the ~3 km square, about 6 m per cell, refined by bicubic interpolation for rendering) and uses only the integer RNG/hash, IEEE single-precision arithmetic, no `-ffast-math`, `-ffp-contract=off` (and `/fp:precise` on MSVC), and no libm transcendental calls in generation code (own polynomial approximations where needed).
- Per-object decisions (tree in cell, ground-cover tuft, room type n, house candidate) hash `(seed, layer, coordinates)` instead of advancing a shared RNG, so results are independent of visiting order.
- A world checksum (FNV-1a over heights quantised to 1 cm, water, biome ids, trail points, cairns and viewpoints) is exposed to tests and the debug overlay; a unit test pins a golden checksum for seed 12345 and runs on all three CI platforms.
- Simulation uses a fixed 60 Hz timestep with an accumulator; rendering interpolates. This gives frame-rate independence (exploration spec) and reproducible game tests.

### 3. World generation pipeline

Base heights (radial main-mountain mask plus ridged ranges) → hydraulic erosion (droplet simulation with a deterministic droplet order) → priority-flood depression filling, where filled cells deeper than 0.5 m become lakes at spill height → flow accumulation and moisture → biome classification from script biome data → trailhead selection (lowest third of heights, 100 m inside the edge, reachable) → trail A* over the grid with cost on slope (blocked above 50 degrees, strongly penalised above 25), water blocked, then path smoothing and cairn placement every ~80 m of arc length → viewpoint scoring (horizon openness from a 32-direction ray march, relief variance in view), greedy selection with 150 m spacing. Terrain renders as chunked meshes with distance LOD and skirts. The soft boundary is a radial damping of outward velocity in the last 50 m plus a hard clamp at the edge; terrain beyond the edge is generated as low-detail scenery only.

### 4. Sky, time, seasons and weather

- Calendar: year phase `p = day_of_year / days_per_year`; solar declination from axial tilt with the summer solstice at mid-summer; hour angle from local solar time (noon at 12:00); sidereal rotation uses `days_per_year / (days_per_year + 1)` of a solar day. Moons have a phase cycle in days and a phase offset; a moon's sky position is the sun's ecliptic position offset by its phase angle; the shader lights the moon disc from the true sun direction.
- Sky: analytic gradient (zenith/horizon colours as functions of sun altitude, sunset glow lobe around the sun azimuth) multiplied by the planet sky tint; stars and milky way are a precomputed star list and band texture rotated by sidereal time and faded by sun altitude and cloud cover. Clouds are a scrolling noise layer on a sky dome, coverage driven by weather.
- Seasons: foliage colour per species is a list of year-phase keys in script data, sampled with smooth interpolation each frame; snowline is a script curve over the year; fresh snow is a coarse snow-cover grid updated in in-game time (accumulate while snowing below 1 degree Celsius, melt above) and sampled by terrain, plant and roof shaders.
- Weather: a Markov chain over the seven states with per-season weights from `weather.lua`; state duration 2–8 in-game hours; blends interpolate a parameter vector (cloud cover, fog density, light, precipitation, wind) over 30–60 real seconds and gate the next transition. Temperature = seasonal base + diurnal swing − 6.5 degrees per 1000 m. Lightning strikes are scheduled in real time during storms; thunder is queued with a delay of distance / 343 m/s.

### 5. Flora and fauna

- Flora: each species layer scatters candidates on a jittered grid; a candidate survives if its biome, altitude, slope and moisture match the script rules, it is not in water and not within the trail corridor. Trees are instanced: near LOD = trunk/branch mesh + leaf-cluster sprites, mid LOD = simplified trunk + fewer, larger clusters, far LOD = one camera-facing impostor; LOD changes cross-fade with screen-door dithering, which suits the dithered look. Ground cover is generated per 8 m cell from the cell hash in a ring around the player and thinned with distance. Wind sway is a vertex-shader displacement scaled by per-vertex flexibility and the weather wind vector. The house registers an exclusion footprint that flora queries at draw time (plants are hidden, not deleted, so they reappear unchanged).
- Fauna: agents with a small state machine (wander, graze, rest, flee-calmly, fly/circle) and steering constrained to the heightfield, water mask and per-species maximum slope. A spawn ring (40–250 m, preferring outside the frustum) fills to a per-species and global cap; agents beyond 300 m are removed. Species parameters (habitat, hours, seasons, comfort distance, speeds, sounds, sprite or mesh) come from `fauna.lua`.

### 6. Player and input

The player is a vertical capsule on the heightfield with circle colliders (trunks, boulders, cairns) and boxes (house walls, furniture, closed doors). Uphill motion is cancelled when the ground normal exceeds 50 degrees and the remaining motion is projected along the slope. Physical inputs are mapped to named actions (`move`, `look`, `run`, `jump`, `interact`, `house`, `camera`, `gallery`, `menu`, `shutter`, `zoom`, `filter`) by a binding table in `input.lua`; the core input module keeps action state and the touch layout (joystick and button rectangles in normalised screen space, hit testing, two-finger tracking), so touch logic is unit tested. The active scheme (keyboard/mouse, gamepad, touch) switches on the last real input source.

### 7. Magic house

- Site analysis samples a fan of candidate footprints 6–14 m ahead of the player (quantised player pose, so the same pose gives the same site), rejects footprints in water, outside the playable area or blocked by rock faces, and scores variants: hillside (uphill gradient behind the footprint above ~30 degrees), treehouse (a tree above a trunk-size threshold within reach), lakeside stilts (dry footprint within ~8 m of a shore), cottage (gentle footprint). The best score wins; no valid candidate yields the gentle message.
- The exterior is assembled from script-described parts by the mesh and voxel builders: foundations, stilts and stairs extend down to the lowest terrain sample under each support; hillside variants sink the back into the slope and add a voxel rock outcrop; weathering (moss, lichen, roof snow) is a shader blend from biome, moisture and snow cover; window glow, lantern and chimney smoke follow the sun altitude.
- The interior lives in a separate interior space far from the terrain: room 0 (the cabin) and rooms 1, 2, ... laid end to end along one axis, each built from its room type in `rooms.lua`. Room type n is picked by `hash(seed, n)` among types that differ from room n−1. Only a window of rooms around the player (for example n−2 to n+2) is kept in memory; others are rebuilt on demand from the hash, which keeps memory bounded and rooms stable.
- Unsummon/re-summon rules (outside within 25 m, outside beyond 25 m, inside) live in the core house module and are called by the script action handler.

### 8. Portal rendering for doors and windows

Each portal pairs two frames: front door (exterior door frame ↔ room 0 entrance), back doors (room n ↔ room n+1), main window (room 0 window ↔ the house's exterior window frame) and scenic windows (room n window ↔ viewpoint k, with k cycling through the ranked viewpoints so consecutive rooms differ). Rendering: draw the current space, write the visible portal quad into the stencil buffer, then draw the destination space with the camera transformed by the portal matrix and an oblique near plane aligned with the portal plane, restricted by the stencil; recursion depth is limited (1 for windows, 2 for doors) with a closed-door fallback beyond. Traversal: when the player's movement segment crosses an open door's rectangle, the player pose is transformed by the same matrix, giving the seamless entry required by the spec. Alternative considered: render-to-texture per portal (rejected: extra targets per window, wrong parallax without per-eye targets in VR; stencil works the same on GL 3.3, GLES 3.0 and WebGL2).

### 9. Retro rendering pipeline

The scene renders into a low-resolution framebuffer (RGBA8 colour + depth24/stencil8) of 216 lines by default, width = round(lines x window aspect). Passes: sky, terrain chunks, voxel models (greedy-meshed small voxels), meshes, alpha-tested sprites (no blending, so no sorting), particles, then a post pass that applies the photo filter, quantises to 32 levels per channel and applies a 4x4 Bayer ordered dither. The result is drawn to the window with nearest filtering; UI (pixel font generated from `font8x8_basic` into `assets/fonts/`, batched quads) is drawn afterwards at an integer scale of the window, so photos (read back from the post-processed low-res target) never contain UI. Shaders live in `assets/shaders/` in a common GLSL subset; the loader prepends `#version 330 core` on desktop or `#version 300 es` plus precision qualifiers on GLES/WebGL2.

### 10. Photos

The post-processed low-res frame is read back, scaled by the smallest integer factor reaching at least 720 lines, encoded by the core PNG encoder (deflate with stored/fixed-Huffman blocks, CRC and Adler checks; verified in unit tests by decoding with `stb_image`), and written through the VFS to `<user data>/photos/mountain-<seed>-<YYYYMMDD-HHMMSS>[-n].png`. Alternative: `stb_image_write` (kept as a fallback; an own encoder keeps the photo path inside the covered core). On the web the user data directory is an IDBFS mount synced after each write, and the JS library offers the file as a Blob download.

### 11. Scripting layer

- Vendored Lua 5.4 compiled into the executable. Entry point `assets/scripts/main.lua` returns or defines `init()`, `update(dt)`, `action(name, pressed, value)` and `ui()` callbacks; content scripts (`planet.lua`, `biomes.lua`, `seasons.lua`, `weather.lua`, `flora.lua`, `fauna.lua`, `rooms.lua`, `input.lua`, `ui/*.lua`) return data tables read by the engine at init.
- Sandbox: no `os.execute`, `io`, `package.loadlib` or C module loading; `require`, `dofile` and file reads go through the VFS (mount order `mods/` then `assets/`, `..` escapes rejected), writes only under the user data directory.
- Every call into Lua is protected; errors carry a traceback and are shown by the error overlay while the engine keeps running with the last good state. Hot reload (F5, desktop and development builds) creates a fresh Lua state and re-runs `main.lua`; persistent game state (seed, clock, weather, player, house) lives in C, so it survives.
- The API is registered from one table of descriptors; `docs/script-api.md` documents it and a unit test fails if a registered name is missing from the document.

### 12. Game-test mode

`mountain --test tests/game/<name>.lua [--seed N]` runs the real game with a scenario script executed as a Lua coroutine. Helpers push real SDL events (keys, mouse, finger events, and an SDL virtual joystick for gamepad input), wait for frames or in-game time on the fixed timestep, and assert on state exposed by the script API (player position, door state, current room, weather, photo files). Any failed assertion, script error, crash or timeout gives a non-zero exit code. In CI the tests run under a virtual display (Xvfb) with Mesa software OpenGL (llvmpipe).

### 13. Web and WebXR (Emscripten)

The web build links SDL3 for Emscripten with WebGL2 only (`MIN_WEBGL_VERSION=2`), packages `assets/` with `--preload-file` into a data package served beside the `.wasm`, mounts IDBFS for user data, and uses a custom JS library in `web/` for: touch-primary detection (`pointer: coarse`), the photo download, the WebGL2 capability message, audio resume on the first gesture, and WebXR. When `navigator.xr.isSessionSupported('immersive-vr')` resolves true, the shell shows "Enter VR"; on entry the existing WebGL2 context is made XR-compatible, the Emscripten main loop is paused and frames are driven by `XRSession.requestAnimationFrame`, rendering each eye into its own low-res target and blitting with nearest filtering into the XR layer; on `end` the normal main loop resumes. XR controller input is mapped onto the same named actions (snap-turn 30 degrees in core).

### 14. Coverage and CI

- Linux job: GCC with `--coverage`, CTest unit tests, game tests under Xvfb/llvmpipe, then `gcovr --filter engine/ --exclude third_party/ --fail-under-line 95` over the combined data. Platform-specific files that cannot compile on Linux (Emscripten glue, Windows-only code) are kept small and isolated so they do not count.
- Windows job: MSVC with CMake, building the game and running unit tests (the MinGW toolchain in `cmake/` remains for local cross builds).
- Web job: emsdk, build, headless-browser smoke test (Playwright with an injected WebXR emulation and touch emulation), upload of the web build artifact.
- Deploy job: on pushes to `main` after all jobs succeed, publish the web build directory (plus licence notices and `.nojekyll`) to `gh-pages` as an orphan, force-pushed commit so the branch only ever contains the web build.

### 15. Assumed defaults not fixed by the request

Values chosen here and used in the specs, adjustable in later changes: latitude 46 degrees north; new world starts at 08:00 on day 1 (spring); default moon phase cycle 8 in-game days; walk 2.5 m/s, run 5 m/s, jump about 1 m, eye height 1.7 m; weather states last 2–8 in-game hours; snow full cover after about 2 in-game hours and melt over about 6; fauna spawn at least 40 m away and despawn beyond about 300 m; at least 8 viewpoints spaced 150 m apart; 32 colour levels per channel; gamepad left-stick click toggles run, Back/View opens the gallery, RT shoots, D-pad zooms and changes filters; F filter key, F3 frame-time overlay, F11 fullscreen; a "Return to trailhead" menu entry; photo gallery room shows at least 6 photos.

## Risks / Trade-offs

- [Software OpenGL in CI is slow and may differ from GPUs] → Game tests assert on game state rather than pixels, run at low resolution, and use the fixed timestep so slow frames do not change outcomes; image checks are limited to properties such as quantised colour levels and photo dimensions.
- [Floating-point differences break cross-platform determinism] → Strict FP flags, no libm in generation, quantised checksums and a golden-checksum test on Linux, Windows and Web.
- [WebXR cannot be tested on real hardware in CI] → Headless browser test with an emulated XR device covers button visibility, session start/end and the frame loop hand-off; manual headset playtests on `gh-pages` before releases.
- [Platform, render and Emscripten code is hard to unit test, threatening the 95 percent gate] → Keep these layers thin, cover them through game tests in the coverage run, and isolate code that only compiles for the web or Windows.
- [Erosion and vegetation generation may be slow on phones and in WebAssembly] → Bounded grid size, erosion iteration budget, progressive chunk building with a loading indicator, and a generation-time check in game tests.
- [Portal recursion and many windows cost GPU time] → Depth limits, frustum culling of portals, low internal resolution, and distance-based fallback to a closed door.
- [Low internal resolution in VR may be uncomfortable] → VR uses at least 360 lines per eye.
- [Mods run third-party scripts] → Sandboxed Lua with VFS-only file access and no process or native-library access.
- [Browser storage can be evicted] → Offer a download right after each photo and in the gallery; settings fall back to defaults.

## Migration Plan

Greenfield project: no data migration. Delivery is through CI: every push to `main` builds all targets and, when green, republishes `gh-pages`. Rollback is a revert on `main` (or re-running the deploy job for an earlier green commit), which republishes the previous web build.

## Open Questions

- Whether the player may wade into shallow lake water or is stopped at the shoreline (the specs only require that water never harms the player).
- Compressed audio format for larger ambience loops (16-bit PCM WAV first; an Ogg Vorbis decoder can be added later without spec changes).
- Exact tuning values for erosion strength, biome thresholds, species densities and comfort distances, which live in script data.

## Future Work

- Beaches and a sea level: a water-level parameter in world settings plus a beach biome and coastal species as script data.
- Alien or magic planets: alternative `planet.lua`, biome, flora and fauna data sets (several moons, tinted skies, other day lengths), selectable in the menu.
- A mod manager to enable, order and disable mods instead of a single `mods/` folder.
