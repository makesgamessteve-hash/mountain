# Mountain script API

The game logic of *Mountain* lives in Lua 5.4 scripts in `assets/scripts/`
beside the executable. A file with the same relative path in a `mods/` folder
beside the executable replaces the original (for example
`mods/scripts/content/earth.lua`). The engine (C) calls four global functions:

| Callback | When |
|---|---|
| `init(reloaded)` | once after `scripts/main.lua` ran; `reloaded` is `true` after F5 |
| `update(dt)` | every frame, `dt` in seconds |
| `action(name, pressed)` | when an input action is pressed (`true`) or released (`false`); also `"photo_saved"` |
| `draw()` | every frame to build the interface with the `ui` library |

Scripts run in a restricted environment: `base`, `table`, `string`, `math`,
`utf8` and `coroutine` are available, `io`, `os.execute`, `dofile`,
`loadfile` and native modules are not. `require("a.b")` loads
`scripts/a/b.lua` through the asset folders. A script error is shown on screen
instead of crashing the game. Colours are `{r, g, b[, a]}` with components in
0..1 (values above 1 are read as 0..255) or a packed `0xAABBGGRR` integer.

Input action names: `forward back left right run jump interact house camera
shoot gallery menu zoom_in zoom_out filter prev next delete debug fullscreen
reload turn_left turn_right download`.

## engine

| Function | Description |
|---|---|
| `engine.log(...)` | print to the log (`print` is the same) |
| `engine.quit()` | quit the game (desktop) |
| `engine.platform()` | `"linux"`, `"windows"`, `"web"` (or `"test"` in unit tests) |
| `engine.version()` | engine version string |
| `engine.fps()` | smoothed frames per second |
| `engine.time()` | real seconds since start |
| `engine.frame()` | frame counter |
| `engine.test_mode()` | `true` while an automated game test runs |
| `engine.fullscreen(on)` | switch fullscreen on/off |
| `engine.reload()` | reload the scripts (like F5) |
| `engine.settings([t])` | read/change settings: `mouse_sensitivity`, `invert_y`, `volume` (0..1), `pixel_lines` (180/216/270/360), `fov` (50..100), `show_fps`; returns the current table |
| `engine.xr()` | `supported, active` for WebXR immersive VR |

## world

| Function | Description |
|---|---|
| `world.params{...}` | terrain settings for the next generation: `size` (grid points), `cell` (m), `peak_height`, `range_height`, `hill_height`, `ridge_strength`, `erosion_drops`, `lake_min_depth`, `sea_level`, `tree_line`, `snow_line`, `playable_radius` |
| `world.generate([seed])` | generate a new world (random seed if omitted); returns the seed |
| `world.seed()` | seed of the current world |
| `world.exists()` | `true` when a world has been generated |
| `world.height(x, z)` | ground height in metres |
| `world.biome(x, z)` | `water forest meadow alpine scree rock snow` |
| `world.slope(x, z)` | 0 (flat) .. 1 (vertical) |
| `world.water(x, z)` | lake surface height or `nil` |
| `world.summit()` | `x, y, z` of the summit |
| `world.trailhead()` | `x, y, z` where the hiking trail starts |
| `world.viewpoint(i)` | `x, y, z, yaw` of scenic viewpoint `i` (1-based) or `nil` |
| `world.altitude(h)` | height `h` as a fraction 0..1 of the terrain range |
| `world.checksum()` | FNV-1a checksum of the generated world (heights, water, biomes, trail, cairns, viewpoints) |
| `world.info()` | table: `size min_height max_height lakes trail_points cairns viewpoints plants snowline` |

## planet

| Function | Description |
|---|---|
| `planet.set{...}` | `name`, `day_length` (real seconds per day), `days_per_season`, `latitude`, `axial_tilt`, `sun_size`, `sun_color`, `day_zenith`, `day_horizon`, `sunset_horizon`, `night_zenith`, `night_horizon`, `moons = {{period, phase, size, inclination, color}, ...}` (up to 4) |
| `planet.name()` | planet name |

## time

| Function | Description |
|---|---|
| `time.hour()` / `time.set_hour(h)` | time of day 0..24 |
| `time.day()` | day number (1-based) |
| `time.speed([s])` | game time multiplier (0..3600) |
| `time.pause([on])` | pause/resume the clock |
| `time.season()` | dominant season name and a table of weights `{spring, summer, autumn, winter}` |
| `time.set_season(name)` | jump to the middle of a season |
| `time.year()` | year fraction 0..1 (0 = start of spring) |
| `time.daylight()` | 0 (night) .. 1 (day) |
| `time.moon([i])` | phase 0..1 and lit fraction of moon `i` |
| `time.day_length()` | hours of daylight today |
| `time.sun()` | direction towards the sun |

## weather

| Function | Description |
|---|---|
| `weather.get()` | state name and a table `clouds precipitation snowing wind fog snow_ground temperature locked target` |
| `weather.set(name[, lock[, instant]])` | `clear cloudy overcast fog rain storm snow`; locked by default |
| `weather.table{...}` | season-weighted transition table: `{spring = {clear = 3, cloudy = 3, ...}, summer = {...}, autumn = {...}, winter = {...}}` |
| `weather.unlock()` | let the weather change by itself again |
| `weather.snow(amount)` | set the fresh snow cover 0..1 |

## flora

| Function | Description |
|---|---|
| `flora.species{...}` | define a plant species (before `world.generate`): `name`, `kind` (`tree bush ground boulder`), `crown` (`cone round oval umbrella`), `deciduous`, `height_min`, `height_max`, `trunk_radius`, `crown_width`, `tiles` (foliage atlas tiles), `bark_tile`, `bark_color`, `colors = {spring, summer, autumn, winter}`, `blossom_tile`, `bloom` (seasons), `alt_min`, `alt_max`, `slope_max`, `moist_min`, `moist_max`, `biomes`, `density`, `patchiness`; returns its index |
| `flora.clear()` | remove all species definitions |
| `flora.count()` | number of placed plants and of species |
| `flora.near(x, z, r)` | number of trees, bushes and boulders within `r` metres |

## fauna

| Function | Description |
|---|---|
| `fauna.species{...}` | define an animal: `name`, `kind` (`quadruped bird soaring butterfly firefly`), `size`, `leg`, `neck`, `head`, `color`, `color2`, `horns` (0 none, 1 antlers, 2 horns, 3 long ears), `tail`, `walk_speed`, `run_speed`, `comfort` (m), `alt_min`, `alt_max`, `biomes`, `from`, `to` (active hours), `seasons`, `group_min`, `group_max`, `max_groups`, `chance`, `tile` (sprite), `sound` |
| `fauna.clear()` | remove all species and animals |
| `fauna.count([name])` | living animals (of a species) |
| `fauna.spawn(name, x, z)` | spawn a group; returns how many |

## player

| Function | Description |
|---|---|
| `player.pos()` / `player.set_pos(x, y, z)` | feet position (interior coordinates while inside the house) |
| `player.yaw([yaw[, pitch]])` | view direction in radians |
| `player.trailhead()` | teleport to the start of the trail |
| `player.inside()` | `true` inside the magic house |
| `player.surface()` | `grass rock snow wood water` |
| `player.altitude()` | height in metres (of the house while inside) |
| `player.speeds([walk[, run]])` | walking and running speed in m/s |

## house

| Function | Description |
|---|---|
| `house.summon()` | summon (or move) the magic house in front of the player; returns `ok, message` |
| `house.unsummon()` | send it away (only from outside, within 25 m) |
| `house.exists()` | `true` while summoned |
| `house.state()` | `none appearing present vanishing` and the variant `cottage hillside tree stilts` |
| `house.interact()` | open/close the door when near it |
| `house.door()` | `open, angle` |
| `house.door_pos()` | `x, y, z, yaw` of the door (yaw looks at it from outside) |
| `house.distance()` | distance to the house and to its door (`nil` without a house) |
| `house.can_unsummon()` | whether `unsummon` would work now |
| `house.room()` | room index and type name while inside (`-1` outside) |
| `house.clear_rooms()` | remove all room types |
| `house.define_room{...}` | `name`, `cabin` (the main room), `width`, `height`, `wall_tile`, `floor_tile`, `wall_color`, `floor_color`, `light`, `weight`, `items = {{"prefab", x=, z=, rot=, scale=, arg=}, ...}`; prefabs: `bed kitchen desk bookshelf fireplace armchair rug plant lamp table chair tea_table bathtub piano telescope planter chest candles photo star_chart window_seat barrel basket` |

## camera and photos

| Function | Description |
|---|---|
| `camera.mode([on])` | camera (photo) mode |
| `camera.fov([deg])` | zoom, 20..90 degrees |
| `camera.filter([name])` | `none sepia bw vivid dusk` |
| `camera.shoot()` | take a photo (saved as PNG; action `"photo_saved"` follows) |
| `photos.count()` | number of photos (newest first) |
| `photos.name(i)` | file name of photo `i` |
| `photos.delete(i)` | delete photo `i` |
| `photos.download(i)` | web: download through the browser; desktop: show the folder |
| `photos.dir()` | folder with the photos |

## ui

Coordinates are interface units (the window divided by an integer scale).

| Function | Description |
|---|---|
| `ui.size()` | width, height |
| `ui.text(x, y, text[, color[, size]])` | draw pixel-font text; returns its width |
| `ui.text_width(text[, size])` | width of a text |
| `ui.rect(x, y, w, h[, color])` | filled rectangle |
| `ui.icon(i, x, y[, size[, color]])` | icon from `textures/icons.png` |
| `ui.button(id, x, y, w, h[, label[, icon]])` | returns `true` when clicked/tapped |
| `ui.photo(i, x, y, w, h)` | draw photo `i` |
| `ui.modal([on])` | while on, the player does not walk and touches go to the interface |
| `ui.message([text[, seconds]])` | show a message; returns the current one |
| `ui.pointer()` | pointer `x, y, down` |
| `ui.error()` | the current script error, if any |

## input, audio, storage, os

| Function | Description |
|---|---|
| `input.mode([name])` | `keyboard touch gamepad xr` |
| `input.down(action)` | whether an action is held |
| `input.unbind_all()` | remove all key bindings (scripts then define the whole key map) |
| `input.bind(action, key)` | add a key (SDL key name, e.g. `"J"`) for an action |
| `audio.play(name[, volume[, pan[, pitch]]])` | play `sounds/<name>.wav` |
| `audio.ambient(name, volume[, fade])` | fade an ambient loop to a volume (0 stops it) |
| `audio.loaded()` | number of loaded sounds |
| `storage.save(name, text)` | store text in the user data folder (`[a-z0-9_-]` names) |
| `storage.load(name)` | stored text or `nil` |
| `os.time()`, `os.clock()`, `os.date([fmt[, t]])` | safe subset of the Lua `os` library |

## Game tests

`mountain --test tests/game/<name>.lua --data-dir <dir>` runs a script as a
coroutine with an extra `test` library that injects real SDL events:
`test.wait(s)`, `test.frames(n)`, `test.key(name, down)`, `test.mouse_move(dx, dy[, x, y])`,
`test.mouse_button(down[, x, y[, button]])`, `test.wheel(dy)`, `test.touch(phase, id, x, y)`,
`test.gamepad()`, `test.gamepad_button(name, down)`, `test.gamepad_axis(name, v)`,
`test.gamepad_detach()`, `test.touch_button(action)`, `test.check(cond, msg)`,
`test.screenshot(name)`, `test.resize(w, h)`, `test.stats()`, `test.average_color()`,
`test.render_view()`, `test.data_dir()`, `test.finish(ok)`.
