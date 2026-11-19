# Tasks

## 1. Project scaffolding and licensing

- [x] 1.1 CMake project with `mtn_core` (core + script), `mountain` and `mtn_unit_tests`, C17, strict FP flags; SDL3 from the system or fetched; assets copied beside the executable
- [x] 1.2 Vendor Lua 5.4 and stb as third-party targets without coverage instrumentation
- [x] 1.3 `LICENSE` (MIT for code) and `assets/LICENSE.md` (CC BY 4.0 for art and sounds, third-party notices)
- [x] 1.4 MinGW-w64 cross toolchain for Windows builds from Linux

## 2. Engine core

- [x] 2.1 Math (vectors, matrices, frustum, oblique near plane, portal transforms), PCG32, hashes and noise, with unit tests
- [x] 2.2 Terrain generation (shape, outflow valleys, erosion, lakes, moisture, biomes, trail, cairns, viewpoints, checksum) with unit tests
- [x] 2.3 Planet, clock, seasons, sun and moons, sky colours; weather states, transitions, temperature, lightning and fresh snow, with unit tests
- [x] 2.4 Flora species, placement, seasonal colours, ground cover, LOD meshes, house clearing and collisions, with unit tests
- [x] 2.5 Fauna species, spawning, behaviours, comfort distance, meshes and calls, with unit tests
- [x] 2.6 Player movement with slope limit and sliding, jumping, collisions and footsteps, with unit tests
- [x] 2.7 Magic house: site analysis, four variants, exterior meshes, door, collisions, portals; endless rooms with furniture prefabs, lights and colliders, with unit tests
- [x] 2.8 Mesh and voxel builders, photo pipeline and gallery, WAV parsing and mixer, input actions, touch layout, UI with focus navigation, virtual file system, with unit tests
- [x] 2.9 Game state: simulation step, house flow (enter/leave through the door), soft playable edge, positional sounds, with unit tests

## 3. Scripting and content

- [x] 3.1 Lua API (engine, world, planet, time, weather, flora, fauna, player, house, camera, photos, ui, input, audio, storage, os) in a restricted environment, with unit tests
- [x] 3.2 Content scripts: Earth planet, biomes, plants and animals; house rooms; key map; weather table
- [x] 3.3 Game flow scripts: HUD, menu (time, seasons, weather, pixel size, volume, trailhead, new mountain, typed seed), gallery, camera mode, ambience, persistence of settings, seed and time speed
- [x] 3.4 Script errors shown on screen; F5 soft reload
- [x] 3.5 `docs/script-api.md` and the automated completeness check

## 4. Renderer and platform

- [x] 4.1 Renderer: low-resolution target, dithered post pass, sky, terrain LOD, water, trail and cairns, flora chunks, ground cover, fauna, house, portals, particles, UI, photo capture
- [x] 4.2 SDL3 platform layer: window and GL context, keyboard, mouse capture, touch, gamepads, audio stream, user folders, photo files, missing-asset errors
- [x] 4.3 Game-test mode with real SDL event injection, virtual gamepad, screenshots and exit codes
- [x] 4.4 Generated CC BY 4.0 assets (`tools/gen_assets.py`): foliage, materials, terrain, moon, icons, font, 24 sounds

## 5. Web and WebXR

- [x] 5.1 Emscripten build with WebGL2, preloaded asset package beside the wasm, IndexedDB user folder
- [x] 5.2 Shell page with loading progress, WebGL2 check and Enter VR button; touch detection and switching
- [x] 5.3 WebXR session handling, stereo rendering into the XR layer, controller locomotion, snap turn, interact and photo
- [x] 5.4 Photo downloads through the browser

## 6. Quality and delivery

- [x] 6.1 Game tests: tour (time, seasons, weather), house (variants, door, rooms, leaving), input (keyboard, mouse, touch, gamepad, menu navigation), photo (camera, filters, gallery, gallery room), menu, asset failures
- [x] 6.2 Coverage script and 95 percent gate over `engine/` (unit and game tests combined)
- [x] 6.3 Headless browser smoke test (desktop, emulated phone with touch, mocked WebXR)
- [x] 6.4 GitHub Actions: Linux, Windows, Web, OpenSpec, and deploy of the web build to `gh-pages`
- [x] 6.5 README with features, controls, build steps, modding and tests
