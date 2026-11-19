-- Automated game test: summon the magic house on different kinds of ground,
-- open the door, look inside, walk through the endless rooms and leave again.
-- Uses real keyboard events injected into SDL. MIT License.
local function look_at(x, y, z)
  local px, py, pz = player.pos()
  local dx, dy, dz = x - px, y - (py + 1.7), z - pz
  player.yaw(math.atan(dx, -dz), math.atan(dy, math.sqrt(dx * dx + dz * dz)))
end

local function tap(key)
  test.key(key, true)
  test.frames(2)
  test.key(key, false)
  test.frames(2)
end

-- find a spot of a given kind near the trail (flat open meadow / steep slope)
local function find_spot(pred)
  local sx, sy, sz = world.summit()
  for r = 60, 900, 25 do
    for i = 0, 23 do
      local a = i * math.pi / 12
      local x, z = sx + math.cos(a) * r, sz + math.sin(a) * r
      if pred(x, z) then return x, z end
    end
  end
end

time.pause(true)
time.set_hour(11)
weather.set("clear", true, true)

local variants_seen = {}
local function stand_before_door(dist)
  local dx, dy, dz, yaw = house.door_pos()
  local fx, fz = -math.sin(yaw), math.cos(yaw)   -- the direction the door faces
  local px, pz = dx + fx * dist, dz + fz * dist
  local gy = math.max(world.height(px, pz), dy)
  player.set_pos(px, gy + 0.05, pz)
  player.yaw(yaw, 0)
  return dx, dy, dz, yaw
end

local function summon_and_view(name, x, z)
  player.set_pos(x, world.height(x, z), z)
  player.yaw(0, 0)
  test.frames(3)
  local ok, msg = house.summon()
  test.wait(2.2)
  local st, variant = house.state()
  engine.log(name, ok, msg, st, variant)
  if ok then
    variants_seen[variant] = true
    local dx, dy, dz = house.door_pos()
    player.set_pos(x - 1.5, world.height(x - 1.5, z + 5) + 0.5, z + 5)
    look_at(dx, dy + 1.0, dz)
    test.frames(4)
    test.screenshot("house_" .. name)
  end
  return ok
end

local function open_ground(x, z)
  return flora.near(x, z - 8, 14) == 0 and flora.near(x, z + 5, 6) == 0
end
local mx, mz = find_spot(function(x, z)
  return world.slope(x, z - 8) < 0.12 and not world.water(x, z - 8) and open_ground(x, z)
end)
if mx then summon_and_view("meadow", mx, mz) end
house.unsummon()
test.wait(1.5)
local cx, cz = find_spot(function(x, z)
  local s = world.slope(x, z - 8)
  return s > 0.32 and s < 0.48 and world.biome(x, z - 8) ~= "water" and open_ground(x, z)
end)
if cx then summon_and_view("slope", cx, cz) end
house.unsummon()
test.wait(1.5)

-- the main test: house near the trailhead, open the door and walk in
player.trailhead()
test.frames(2)
local ok = house.summon()
test.check(ok, "house summoned near the trailhead")
test.wait(2.0)
test.check(house.exists(), "house exists")
stand_before_door(2.2)
test.frames(10)
local _, door = house.distance()
test.check(door and door < 3.0, "standing in front of the door (" .. tostring(door) .. ")")
test.screenshot("house_door_closed")
tap("e")
test.wait(1.2)
local open = house.door()
test.check(open, "door opened with E")
stand_before_door(5)
test.frames(3)
test.screenshot("house_door_open")
stand_before_door(1.5)
-- walk in with W
test.key("w", true)
test.wait(2.5)
test.key("w", false)
test.frames(5)
test.check(player.inside(), "walked through the open door into the house")
local idx, name = house.room()
test.check(idx == 0 and name == "cabin", "first room is the cabin, got " .. tostring(name))
test.screenshot("house_cabin")
-- look at the big window (+x wall)
player.yaw(math.pi / 2, 0.05)
test.frames(5)
test.screenshot("house_cabin_window")
player.yaw(-math.pi / 2, -0.1)
test.frames(5)
test.screenshot("house_cabin_left")
-- walk to the back door and into the next rooms
for room = 1, 3 do
  player.set_pos(0, 0, (room - 1) * 9 + 7.5)
  player.yaw(math.pi, 0)
  test.key("w", true)
  test.wait(1.4)
  test.key("w", false)
  test.frames(3)
  local i2, n2 = house.room()
  test.check(i2 == room, "walked into room " .. room .. " (got " .. tostring(i2) .. ")")
  player.yaw(math.pi / 2 + 0.3, 0.05)
  test.frames(4)
  test.screenshot("house_room_" .. room .. "_" .. tostring(n2):gsub(" ", "_"))
end
-- far room index jump (endless)
player.set_pos(0, 0, 9 * 40 + 4)
test.frames(4)
test.check(house.room() == 40, "room 40 exists")
-- back to the cabin and out of the front door
player.set_pos(0, 0, 2.0)
player.yaw(0, 0)
test.frames(3)
local unsum = house.unsummon()
test.check(not unsum, "cannot unsummon from inside")
test.key("w", true)
test.wait(1.5)
test.key("w", false)
test.frames(3)
test.check(not player.inside(), "left the house through the front door")
test.screenshot("house_outside_again")
tap("e")
test.wait(1)
test.check(not house.door(), "door closed again")
tap("h")
test.wait(1.6)
test.check(not house.exists(), "house sent away from outside")
test.finish(true)
