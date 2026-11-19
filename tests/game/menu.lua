-- Automated game test: the pause menu driven by mouse clicks (time, seasons,
-- weather, pixel size, volume, trailhead, new mountain). MIT License.
local function tap(key)
  test.key(key, true); test.frames(2); test.key(key, false); test.frames(3)
end
local function click(ux, uy)
  -- convert interface units to window pixels
  local w, h = ui.size()
  local sx = 640 / w
  test.mouse_button(true, ux * sx, uy * sx); test.frames(2)
  test.mouse_button(false, ux * sx, uy * sx); test.frames(2)
end
tap("escape")
test.check(ui.modal(), "Esc opens the menu")
test.wait(0.2)
test.screenshot("menu")
local w, h = ui.size()
local x, y = math.floor((w - 300) / 2), math.floor((h - 236) / 2)
local h0 = time.hour()
click(x + 150, y + 58 + 9)          -- hour +
test.check(time.hour() > h0, "hour + button")
click(x + 75, y + 58 + 9)           -- hour -
click(x + 180, y + 58 + 9)          -- pause
test.check(time.pause(), "pause button")
click(x + 240, y + 58 + 9)          -- 10x
test.check(not time.pause() and time.speed() == 10, "10x button")
click(x + 75 + 165, y + 80 + 9)     -- winter
test.check(time.season() == "winter", "winter button")
click(x + 75 + 146, y + 102 + 9)    -- fog
test.check(select(2, weather.get()).target == "fog", "fog button")
click(x + 75 + 146, y + 122 + 9)    -- snow
click(x + 80, y + 142 + 9)          -- changing weather
click(x + 75 + 46, y + 166 + 9)     -- 216 lines
test.check(engine.settings().pixel_lines == 216, "pixel size button")
click(x + 75, y + 188 + 9)          -- volume -
click(x + 145, y + 188 + 9)         -- volume +
click(x + 200, y + 188 + 9)         -- to the trailhead
test.check(not ui.modal(), "trailhead closes the menu")
tap("escape")
local seed = world.seed()
click(x + 160, y + 34 + 9)          -- new mountain
test.wait(0.5)
test.check(world.seed() ~= seed, "a new mountain was generated")
tap("escape")
click(x + 20, y + 34 + 9)           -- resume
test.check(not ui.modal(), "resume closes the menu")
test.finish(true)
