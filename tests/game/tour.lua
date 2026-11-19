-- Automated game test: a visual tour (screenshots) through time, seasons,
-- weather and the magic house. Runs headless in CI. MIT License.
local function look_at(x, y, z)
  local px, py, pz = player.pos()
  local dx, dy, dz = x - px, y - (py + 1.7), z - pz
  player.yaw(math.atan(dx, -dz), math.atan(dy, math.sqrt(dx * dx + dz * dz)))
end

test.check(world.exists(), "a world was generated")
time.pause(true)
time.set_hour(10)
test.wait(0.5)
test.screenshot("tour_01_trailhead")

-- view towards the summit
local sx, sy, sz = world.summit()
look_at(sx, sy, sz)
test.wait(0.5)
test.screenshot("tour_02_summit_view")

-- golden hour and night
time.set_hour(19.2)
test.wait(0.3)
test.screenshot("tour_03_sunset")
time.set_hour(23)
test.wait(0.3)
test.screenshot("tour_04_night")
time.set_hour(10)

-- seasons
time.set_season("autumn")
test.wait(0.5)
test.screenshot("tour_05_autumn")
time.set_season("winter")
test.wait(0.5)
test.screenshot("tour_06_winter")
time.set_season("summer")

-- weather
weather.set("storm", true, true)
test.wait(1.5)
test.screenshot("tour_07_storm")
weather.set("fog", true, true)
test.wait(0.5)
test.screenshot("tour_08_fog")
weather.set("clear", true, true)
test.wait(0.2)

-- the magic house
player.yaw(0.3, -0.05)
local ok = house.summon()
test.check(ok, "house summoned")
test.wait(2.5)
local state, variant = house.state()
test.check(state == "present", "house present, got " .. tostring(state))
engine.log("house variant", variant)
test.screenshot("tour_09_house")
test.finish(true)
