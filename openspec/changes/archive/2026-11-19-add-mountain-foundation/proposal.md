# Proposal

## Why

Mountain is a relaxing game about hiking on a procedurally generated mountain, where time of day, sky, moons, seasons, weather, flora and fauna compose views worth photographing, and a summonable magic house offers an endless string of cozy rooms with windows onto those views. The repository is empty apart from scaffolding, so this change defines the whole first playable version as one coherent baseline that every later change (beaches, alien or magic planets, mods) builds on.

## What Changes

- Add a deterministic, seed-based world generator: a central main mountain with surrounding ranges on a ~3 km square, eroded valleys, lakes, altitude/slope biomes, a cairn-marked hiking trail from a trailhead to the summit, detected scenic viewpoints and a soft boundary.
- Add an in-game clock with adjustable speed, a latitude/day-of-year sun, one or more lit and phased moons, a rotating star field and milky way, sky gradients, sunrise/sunset colours and a cloud layer, all driven by script-defined planet data.
- Add a year of four configurable seasons that blend foliage colours, move the snowline, bloom flowers, accumulate and melt fresh snow and gate seasonal fauna.
- Add a weather system (clear, cloudy, overcast, fog, rain, storm, snow) with season-weighted automatic transitions, smooth blending, temperature-dependent precipitation, wind, lightning and thunder, and a menu lock.
- Add script-defined flora (trees, bushes, grass, flowers, ferns, mushrooms, boulders) placed deterministically by rules, with distance detail levels, dense near-player grass and wind sway.
- Add script-defined, never-hostile fauna with habitats, active hours and seasons that wander, graze and calmly keep their distance, spawning and despawning around the player.
- Add first-person exploration with walking, running, jumping, slope limits, collisions, surface footsteps, and full keyboard/mouse and gamepad bindings, without failure states.
- Add the magic house: summoned in front of the player at a site that blends into the surroundings (hillside, treehouse, lake stilts, cottage), a door that must be opened and shows the inside/outside through it, a cozy main cabin and an endless chain of script-defined rooms whose big windows show different real viewpoints, plus unsummon/re-summon rules.
- Add photography: a camera mode with zoom and filters, PNG photos saved to the user data folder, an in-game gallery, and browser downloads on the web.
- Add retro rendering: low internal resolution with nearest upscaling, ordered dithering, distance fog, a mix of 3D meshes, voxel models and sprites, a pixel font, and a 60 fps target.
- Add platform targets: native Windows and Linux builds, a web build with automatic touch controls on mobile, and a WebXR immersive VR mode.
- Add modding and scripting: all content in an `assets/` folder beside the executable, Lua game logic, a `mods/` override folder, on-screen script errors, hot reload, and a documented script API.
- Add audio ambience: context-mixed ambient loops, one-shot effects, a master volume and web audio unlocking.
- Add quality and delivery rules: unit tests with a 95% line coverage gate, automated headless game tests, CI builds for Linux, Windows and Web, publication of the web build to `gh-pages`, and MIT/CC BY 4.0 licensing.

## Capabilities

### New Capabilities

- `world-generation`: Deterministic seed-based terrain, erosion, lakes, biomes, hiking trail, scenic viewpoints, playable-area boundary and new-world generation.
- `sky-and-time`: In-game clock and time controls, sun path, moons and phases, stars and milky way, sky colours and clouds, driven by planet data.
- `seasons`: The four-season year, foliage colour blending, snowline and fresh snow, flower blooming and seasonal fauna gating.
- `weather`: Weather states, season-weighted transitions and blending, rain versus snow, wind, lightning and thunder, and weather locking.
- `flora`: Script-defined plant species, deterministic rule-based placement, tree detail levels, near-player ground cover, wind sway and clearing around the house.
- `fauna`: Script-defined animal species with habitats, hours and seasons, calm wandering and avoidance behaviour, and spawning around the player.
- `exploration`: First-person movement, slope and collision rules, surface footsteps, keyboard/mouse and gamepad bindings, and the absence of failure states.
- `magic-house`: Summoning and site selection, house variants and weathering, the door and portal view, the main cabin, endless script-defined rooms with viewpoint windows, and unsummoning.
- `photography`: Camera mode, zoom and filters, photo files, the in-game gallery and web downloads.
- `retro-rendering`: Low-resolution rendering, upscaling, dithering, fog, mixed meshes/voxels/sprites, pixel font and frame-rate target.
- `platform-targets`: Native Windows/Linux builds, the web build with automatic touch controls, and the WebXR VR mode.
- `modding-and-scripting`: External asset folders, Lua game logic, mod overrides, script error handling, hot reload and the script API documentation.
- `audio-ambience`: Context-mixed ambient loops, one-shot sound effects, master volume and web audio start.
- `quality-and-delivery`: Unit test coverage gate, automated game tests, CI builds, `gh-pages` web publishing and licensing.

### Modified Capabilities

None (no specs exist yet).

## Impact

- New code: the C17 engine under `engine/` (core, render, platform, script layers), Lua scripts and data under `assets/`, unit tests under `tests/unit/`, game-test scenarios under `tests/game/`, web shell files under `web/`, and GitHub Actions workflows under `.github/workflows/`.
- Dependencies: SDL3, OpenGL 3.3 / OpenGL ES 3.0 (WebGL2), vendored Lua 5.4 and stb headers in `third_party/`, Emscripten for the web build, gcovr for coverage, Mesa software rendering and a virtual display for headless game tests.
- Systems: GitHub Actions CI on every push to `main`, and a `gh-pages` branch that contains only the web build.
- Licensing: `LICENSE` (MIT) for code and a CC BY 4.0 declaration for art and sound assets.
