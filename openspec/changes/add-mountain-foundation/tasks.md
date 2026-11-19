# Tasks

## 1. Project scaffolding and licensing

- [ ] 1.1 Create the CMake project with targets `mtn_core` (static library from `engine/src/core`), `mountain` (executable linking core, render, platform and script layers) and `mtn_unit_tests`, using C17 and strict FP flags (`-ffp-contract=off`, `/fp:precise`); verify `cmake -B build && cmake --build build` succeeds on Linux
- [ ] 1.2 Add vendored Lua 5.4 and stb as third-party CMake targets compiled without coverage instrumentation, and obtain SDL3 at a pinned release; verify a clean configure and build links all three
- [ ] 1.3 Add a post-build step that copies `assets/` beside the executable; verify the build output directory contains `mountain` and `assets/scripts/main.lua`
- [ ] 1.4 Add `LICENSE` (MIT) for code, `assets/LICENSE` (CC BY 4.0) with an attribution list, and `THIRD_PARTY_NOTICES` for Lua, stb, SDL3 and font8x8; verify all files exist and name their licences

## 2. Engine core foundations

- [ ] 2.1 Implement core math (vectors, matrices, quaternions, frustum, plane clipping) and verify unit tests cover each function including degenerate inputs
- [ ] 2.2 Implement PCG32 RNG and the stateless integer hash; verify unit tests pin golden output sequences
- [ ] 2.3 Implement value/gradient noise and fractal helpers without libm transcendentals; verify unit tests pin golden samples and range bounds
- [ ] 2.4 Implement the virtual file system (mount order `mods/` then `assets/`, path normalisation, `..` rejection, user-data writes only); verify unit tests for override, add-only files, missing files and escape attempts
- [ ] 2.5 Implement the PNG encoder and verify unit tests decode its output with `stb_image` and compare pixels and dimensions
- [ ] 2.6 Implement the fixed 60 Hz timestep accumulator and verify unit tests show identical simulation steps for 30 and 60 fps frame sequences

## 3. Platform layer and game-test mode

- [ ] 3.1 Implement the SDL3 window and GL context creation (GL 3.3 core desktop, GLES 3.0 web), resize handling and F11 fullscreen; verify the game opens a window on Linux under Xvfb
- [ ] 3.2 Implement event translation into the core input module (keyboard, mouse with pointer capture, finger events, gamepads with hot-plug) and the named action map; verify unit tests for action state and dead zones
- [ ] 3.3 Implement user data and settings persistence (per-user folder on desktop, settings file for resolution, volume, time speed and last seed); verify a unit test round-trips settings and a game test sees a saved volume after restart
- [ ] 3.4 Implement the SDL audio stream fed by the core mixer; verify the game runs silently with no audio device (dummy driver) in CI
- [ ] 3.5 Implement `--test <script>` mode: scenario coroutine, event injection helpers (keys, mouse, touch, SDL virtual joystick), waits on frames and in-game time, assertions, non-zero exit on failure, error or timeout; verify a passing and a deliberately failing sample test give exit codes 0 and non-zero

## 4. Scripting layer and modding

- [ ] 4.1 Embed Lua 5.4, load `scripts/main.lua` through the VFS and call `init`, `update`, `action` and `ui`; verify a game test observes the callbacks being called
- [ ] 4.2 Load content data tables (planet, biomes, seasons, weather, flora, fauna, rooms, input, ui) into engine structures with validation messages; verify unit tests for valid and malformed tables
- [ ] 4.3 Apply the sandbox (no `os.execute`, `io`, native loading; VFS-only `require`/file access); verify a game test that a blocked call raises a script error and runs nothing
- [ ] 4.4 Implement protected calls with tracebacks and the on-screen error overlay for missing scripts, syntax errors and run-time errors; verify game tests for each case keep the process running and show file, line and message
- [ ] 4.5 Implement F5 hot reload keeping seed, clock, weather, player and house state; verify a game test fixes a broken script, reloads and checks the error is cleared and the position unchanged
- [ ] 4.6 Verify the `mods/` override with a game test that loads a modded `scripts/planet.lua` with two moons
- [ ] 4.7 Register the API from one descriptor table, write `docs/script-api.md`, and add a unit test that fails when a registered name is undocumented; verify the test passes

## 5. Renderer and retro pipeline

- [ ] 5.1 Implement the shader loader that reads `assets/shaders/` and prepends the GL 3.3 or GLES 3.0 version header and precision; verify all shaders compile under Mesa llvmpipe in a game test
- [ ] 5.2 Implement the low-resolution framebuffer (default 216 lines, options 180/216/270/360, width from window aspect) with colour, depth and stencil; verify unit tests for size computation and a game test for switching to 360 lines
- [ ] 5.3 Implement the post pass (photo filter, 32-level quantisation, 4x4 Bayer dither) and nearest upscaling to the window; verify a game test reads back the frame and checks quantised levels and 5x5 pixel blocks at 1920x1080
- [ ] 5.4 Implement distance fog matched to the horizon sky colour with weather-driven density; verify a unit test of the fog colour/density functions
- [ ] 5.5 Implement mesh, voxel (greedy-meshed) and sprite batches with shared lighting and fog; verify unit tests of the voxel mesher and sprite quad builder
- [ ] 5.6 Generate the pixel font asset from `font8x8_basic`, implement UI batching with integer scaling, and the F3 frame-time overlay; verify unit tests for glyph layout and a game test that toggles the overlay

## 6. Web build foundation

- [ ] 6.1 Add the Emscripten build (WebGL2 only, `assets` preloaded as a data package beside the wasm, memory growth, IDBFS user data) and the `web/` shell with loading progress and a no-WebGL2 message; verify `emcmake cmake` and the build produce `index.html`, `.js`, `.wasm` and `.data`
- [ ] 6.2 Add the custom JS library for touch-primary detection, Blob downloads and audio resume on first gesture; verify the page loads in a headless browser without console errors

## 7. Continuous integration and delivery

- [ ] 7.1 Add the Linux CI job: coverage build, unit tests, game tests under Xvfb with Mesa llvmpipe, and `gcovr --fail-under-line 95` excluding third-party code; verify the job fails when coverage drops below 95 percent
- [ ] 7.2 Add the Windows CI job (MSVC, CMake, unit tests) and verify it runs on every push and pull request
- [ ] 7.3 Add the Web CI job (emsdk build, headless-browser smoke test with touch and WebXR emulation) and verify it fails on a page error
- [ ] 7.4 Add the deploy job that publishes only the web build, licence notices and `.nojekyll` to `gh-pages` as an orphan commit after all jobs succeed on `main`; verify the branch listing contains no sources and a failed run does not deploy
- [ ] 7.5 Add a cross-platform determinism check that runs the golden world-checksum test in the Linux, Windows and Web jobs; verify all three report the same checksum

## 8. World generation

- [ ] 8.1 Implement base heights (central main mountain, surrounding ranges, ~3 km square) and verify unit tests that the summit is the highest point and lies in the central third
- [ ] 8.2 Implement deterministic hydraulic erosion and verify a unit test that steepest descent from sampled points ends in a lake or at the edge
- [ ] 8.3 Implement priority-flood lake filling (depressions deeper than 0.5 m) and verify unit tests on synthetic basins for flat surfaces at spill height
- [ ] 8.4 Implement moisture and script-driven biome classification and verify unit tests for the default rules and a changed tree line
- [ ] 8.5 Implement trailhead selection, trail A*, smoothing and cairns every ~80 m; verify unit tests over several seeds for walkability, no water, average climb at most 25 degrees, cairn spacing 60–100 m and summit reach
- [ ] 8.6 Implement viewpoint scoring and selection; verify unit tests for at least 8 viewpoints, 150 m spacing and stable ranking
- [ ] 8.7 Implement the world checksum and verify the golden checksum test for seed 12345 and different checksums for seeds 1 and 2
- [ ] 8.8 Implement chunked terrain meshes with LOD and edge scenery, and verify a unit test that chunk borders share vertices without cracks
- [ ] 8.9 Implement the soft boundary and trailhead start pose; verify a game test running at the edge for 2 minutes stays inside and a new world starts at the trailhead facing the trail
- [ ] 8.10 Add menu entries for the current seed, "New random mountain" and typed seed with validation; verify a game test regenerates seed 42 and rejects invalid input

## 9. Sky and time

- [ ] 9.1 Implement the clock (1 in-game hour per real minute default, 08:00 day 1 start, day rollover, planet hours per day) and verify unit tests
- [ ] 9.2 Add menu time controls (paused, 1x, 10x, 60x, set hour); verify a game test of each setting
- [ ] 9.3 Implement the sun model (latitude 46 degrees north, axial tilt, solstice at mid-summer); verify unit tests for 15–16 h summer and 8–9 h winter days and due-south noon
- [ ] 9.4 Implement moons from planet data with orbits and sun-lit phases; verify unit tests for full moon opposite the sun, phase after half a cycle and two independent moons
- [ ] 9.5 Render stars and the milky way with sidereal rotation and fading; verify unit tests of rotation (about 90 degrees in 6 h) and visibility by sun altitude
- [ ] 9.6 Implement sky gradient, sunset glow and planet tint; verify unit tests comparing colours at sun altitudes 30 and 0 degrees and with a violet tint
- [ ] 9.7 Render the cloud layer driven by weather coverage and wind; verify a unit test of coverage per weather state
- [ ] 9.8 Implement scene lighting from sun and moons with a minimum night ambient; verify unit tests that full-moon light exceeds new-moon light and ambient never drops below the minimum
- [ ] 9.9 Write `assets/scripts/planet.lua` (Earth default, one moon with an 8-day cycle) and document planet fields in `docs/script-api.md`; verify the default game test sees one moon and a 24-hour day

## 10. Seasons

- [ ] 10.1 Implement the four-season calendar with configurable length (default 7 days) and the menu season jump; verify unit tests for boundaries and wrap-around and a game test jumping to autumn
- [ ] 10.2 Implement continuous foliage colour keys per species (spring blossoms, autumn colours, bare winter deciduous, evergreen conifers); verify unit tests for each season and small steps across boundaries
- [ ] 10.3 Implement the seasonal snowline curve; verify unit tests that the summer line is above 80 percent and the mid-winter line below 40 percent of the height range
- [ ] 10.4 Implement the fresh-snow cover grid (full after ~2 in-game hours, melt over ~6); verify unit tests of accumulation and melting
- [ ] 10.5 Implement flower bloom amount by date; verify unit tests for none in winter and peak in summer

## 11. Weather

- [ ] 11.1 Implement weather states with parameter vectors and the season-weighted Markov chain from `weather.lua` (2–8 h durations, always a different next state); verify unit tests of season shares with a seeded RNG
- [ ] 11.2 Implement 30–60 s real-time blending that gates the next transition; verify unit tests at 1x and 60x time speed
- [ ] 11.3 Implement temperature (season, time of day, 6.5 degrees per 1000 m) and rain/snow selection below 1 degree Celsius; verify unit tests for summit snow and valley rain
- [ ] 11.4 Implement wind (speed, direction, smooth changes) feeding sway, clouds, precipitation slant and wind audio; verify unit tests of wind per state
- [ ] 11.5 Implement precipitation particles, lightning scheduling (10–40 s) and delayed thunder; verify unit tests of the thunder delay and no lightning outside storms
- [ ] 11.6 Add the weather lock menu (automatic or a fixed state); verify a game test locking fog for 3 in-game days at 60x

## 12. Flora

- [ ] 12.1 Write `flora.lua` with the default species (conifer, deciduous, birch, bushes, grass, flowers, ferns, mushrooms, boulders) and verify a game test that all occur in a generated world
- [ ] 12.2 Implement deterministic rule-based placement (biome, altitude, slope, moisture, no water, trail corridor clear); verify unit tests for rules and identical placement across two generations
- [ ] 12.3 Implement tree meshes with leaf-cluster sprites, three LOD levels and dithered cross-fades; verify unit tests of LOD selection by distance
- [ ] 12.4 Implement hashed ground-cover cells around the player with distance thinning; verify unit tests that a cell regenerates identically
- [ ] 12.5 Implement wind sway in the vegetation shaders and verify a unit test of the sway amplitude function for calm and storm wind
- [ ] 12.6 Implement the house exclusion footprint (hide, not delete; keep the host tree); verify unit tests that plants are hidden while summoned and restored after

## 13. Fauna

- [ ] 13.1 Write `fauna.lua` with deer, chamois/ibex, marmots, foxes, rabbits, songbirds, eagle, butterflies and fireflies, including habitat, hours, seasons and comfort distances; verify a unit test loads all species
- [ ] 13.2 Implement agents with wander, graze, rest, calm-flee and flying/circling behaviour constrained to terrain, water and slope; verify unit tests of each state transition
- [ ] 13.3 Implement comfort-distance avoidance (never toward the player) and the marmot whistle; verify unit tests that a deer at 20 m ends at least 25 m away
- [ ] 13.4 Implement the spawn ring (at least 40 m or out of view), caps and removal beyond 300 m with habitat, hour and season gating; verify unit tests including no butterflies at night and no marmots in winter

## 14. Exploration and player

- [ ] 14.1 Implement the fixed-step first-person controller (eye 1.7 m, walk 2.5 m/s, run 5 m/s, terrain following); verify unit tests for distances over 4 s at 30 and 60 fps
- [ ] 14.2 Implement the 50-degree slope limit with sliding and the single jump of about 1 m; verify unit tests on synthetic slopes and double-press jumps
- [ ] 14.3 Implement colliders for trunks, boulders, cairns, house walls, furniture and closed doors; verify unit tests of blocking and sliding and that grass does not block
- [ ] 14.4 Implement surface detection and footstep timing (grass, rock, snow, wood); verify unit tests of surface mapping and step rate by speed
- [ ] 14.5 Write `input.lua` with the keyboard/mouse and gamepad bindings and menu navigation; verify game tests for walking with keys and with a virtual gamepad, hot-plug and D-pad menu selection
- [ ] 14.6 Add the "Return to trailhead" menu entry and ensure no failure states (cliff falls, water); verify a game test that walks off a cliff and returns to the trailhead

## 15. Magic house

- [ ] 15.1 Implement site analysis and variant scoring (hillside, treehouse, lakeside stilts, cottage; water, boundary and rock rejection; deterministic per pose); verify unit tests on synthetic terrains for each variant and the no-site case
- [ ] 15.2 Build the exterior variants with grounded foundations, stilts and stairs, rock outcrop, weathering, roof snow, night glow, lantern and chimney smoke; verify unit tests that support bottoms reach the terrain
- [ ] 15.3 Implement summon (about 8 m ahead, door facing the player, sparkle and sound) and the gentle no-site message; verify a game test pressing H on a meadow
- [ ] 15.4 Implement the door state, interaction prompt and door sound; verify a game test that opens and closes the door with E and with gamepad X
- [ ] 15.5 Implement the interior space with the cozy main cabin (bed, kitchen, desk with journal, bookshelf, fireplace, armchair, rug, plants, lamps, large window); verify a game test enumerating the furniture
- [ ] 15.6 Implement stencil portals with oblique near planes and recursion limits for the front door, back doors and windows; verify unit tests of portal matrices and a game test that renders the interior through the open door
- [ ] 15.7 Implement portal traversal for the open door and blocking for the closed door; verify a game test walking in, out and into a closed door
- [ ] 15.8 Implement the endless room chain from `rooms.lua` (library, greenhouse, observatory, tea room, bath, music room, photo gallery room; no repeat of the previous type; bounded resident window); verify a game test walking through 100 rooms with bounded memory and stable rooms when walking back
- [ ] 15.9 Map room windows to ranked viewpoints (consecutive rooms differ) rendered live; verify unit tests of the mapping and a game test of different views in rooms 1 and 2
- [ ] 15.10 Implement the photo gallery room with the most recent photos framed and empty placeholders; verify a game test after taking 3 photos
- [ ] 15.11 Implement unsummon within 25 m outside, re-summon beyond 25 m and refusal inside with a gentle message; verify unit tests of the rules and a game test of each case

## 16. Photography

- [ ] 16.1 Implement camera mode (HUD hidden, minimal viewfinder, leave with C, Esc or B); verify a game test entering and leaving with keyboard and gamepad
- [ ] 16.2 Implement zoom (20–90 degree field of view) with wheel, D-pad and pinch; verify unit tests of clamping
- [ ] 16.3 Implement the five filters in the post pass with live preview; verify a game test that a black-and-white photo has equal channels
- [ ] 16.4 Implement shooting (shutter sound, flash excluded from the photo, no UI in the image), integer upscaling to at least 720 lines and file naming with seed, timestamp and collision suffix; verify unit tests for sizes at each resolution and names, and a game test that saves a photo
- [ ] 16.5 Implement the gallery (thumbnails newest first, large view, delete with confirmation, empty message, desktop folder path); verify a game test that deletes a photo
- [ ] 16.6 Implement web downloads (per photo in the gallery and a button for at least 5 s after shooting) and IDBFS sync; verify the headless browser test receives a download

## 17. Audio ambience

- [ ] 17.1 Implement the core mixer with looping voices, at-least-1-second volume ramps, click-free start/stop, master volume and pan/attenuation; verify unit tests on rendered sample buffers
- [ ] 17.2 Implement context mixing rules (wind by altitude and wind speed, birds, crickets, rain, water, fireplace, indoor muffling); verify unit tests of target volumes per context
- [ ] 17.3 Add one-shot sounds (footsteps, door, shutter, summon, thunder, owl, marmot whistle) and CC BY 4.0 sound assets with attributions; verify a game test that triggers each without errors
- [ ] 17.4 Add the master volume menu entry and web audio start on first gesture; verify a game test that volume 0 produces silent mixer output

## 18. Touch controls and WebXR

- [ ] 18.1 Implement the touch layout (bottom-left joystick with run at full deflection, right-half look, interact, jump, house, camera, gallery, menu buttons; shutter, filter and pinch in camera mode) and automatic switching between touch and keyboard/mouse; verify unit tests of hit testing and game tests that move and look with two simulated fingers
- [ ] 18.2 Implement WebXR in the JS library and engine (Enter VR button when supported, stereo per-eye rendering, head tracking, frame loop handed to the XR session, return to the normal loop on exit); verify the headless browser test with an emulated XR device starts and ends a session
- [ ] 18.3 Map VR controllers (left stick move, right stick 30-degree snap turn, trigger interact, face button photo); verify unit tests of snap turning and action mapping

## 19. Integration checks

- [ ] 19.1 Run the full game-test suite (walking, summoning, door, rooms, photo, weather/season/time, touch, gamepad) in CI and verify all pass with the coverage gate at or above 95 percent
- [ ] 19.2 Verify the published `gh-pages` web build loads on a desktop browser and a phone, switches to touch controls on the phone, and offers "Enter VR" on a WebXR-capable headset
- [ ] 19.3 Verify a 60-second forest walk at default settings averages at most 16.7 ms per frame on mid-range desktop hardware using the F3 overlay
- [ ] 19.4 Run `openspec validate add-mountain-foundation --strict` and verify it reports the change as valid

## Workflow follow-up

- Archive the change with `openspec archive add-mountain-foundation` after implementation and review are complete.
- Verify the archived main specs under `openspec/specs/` contain all fourteen capabilities with their Purpose sections.
