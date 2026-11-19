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
stand_before_door(1.0)
-- walk in with W
test.key("w", true)
test.wait(2.5)
test.key("w", false)
test.frames(5)
test.check(player.inside(), "walked through the open door into the house")
local name, ci, cj = house.room()
test.check(name == "cabin" and ci == 0 and cj == 0, "first room is the cabin, got " .. tostring(name))
test.screenshot("house_cabin")
-- the window beside the front door shows the real view
player.set_pos(2.0, 0, 4.0)
player.yaw(0.35, 0.02)
test.frames(5)
test.screenshot("house_cabin_window")
player.yaw(-math.pi / 2, -0.1)
test.frames(5)
test.screenshot("house_cabin_left")

-- the cabin has doorways on three sides: walk through each into a different room
local dirs = {east = {1, 0}, west = {-1, 0}, north = {0, 1}, south = {0, -1}}
local function yaw_of(d) return math.atan(d[1], -d[2]) end
local function walk_to(ci, cj, side)
  local r = house.room_at(ci, cj)
  local d = dirs[side]
  player.set_pos(r.x, 0, r.z)
  player.yaw(yaw_of(d), 0)
  test.frames(2)
  test.key("left shift", true)
  test.key("w", true)
  local reached = false
  for _ = 1, 60 do
    test.wait(0.1)
    local _, i, j = house.room()
    if i == ci + d[1] and j == cj + d[2] then
      local n = house.room_at(i, j)
      local px, _, pz = player.pos()
      if math.abs(px - n.x) + math.abs(pz - n.z) < 4 then reached = true; break end
    end
  end
  test.key("w", false)
  test.key("left shift", false)
  test.frames(3)
  return reached
end
local cabin = house.room_at(0, 0)
test.check(#cabin.doors == 3 and cabin.window_side == "south", "cabin: doors on three sides, the front door south")
local names = {}
for _, side in ipairs(cabin.doors) do
  local ok = walk_to(0, 0, side)
  local n2, i2, j2, balcony = house.room()
  test.check(ok, "walked " .. side .. " from the cabin into room " .. tostring(i2) .. "," .. tostring(j2))
  names[n2] = true
  player.yaw(yaw_of(dirs[house.room_at(i2, j2).window_side]) + 0.3, 0.05)
  test.frames(4)
  test.screenshot("house_room_" .. side .. "_" .. tostring(n2):gsub(" ", "_"))
end
local kinds = 0
for _ in pairs(names) do kinds = kinds + 1 end
test.check(kinds == 3, "three different rooms around the cabin")

-- a balcony somewhere near: step out onto it
local bal
for r = 1, 4 do
  for i = -r, r do
    for j = -r, r do
      if not bal and math.abs(i) + math.abs(j) == r and house.room_at(i, j).balcony then bal = {i, j} end
    end
  end
end
test.check(bal ~= nil, "a room with a balcony nearby")
if bal then
  local r = house.room_at(bal[1], bal[2])
  local d = dirs[r.window_side]
  -- just inside the balcony door (nothing stands in front of a doorway)
  player.set_pos(r.x + d[1] * (r.width / 2 - 1.0), 0, r.z + d[2] * (r.width / 2 - 1.0))
  player.yaw(yaw_of(d), 0)
  test.frames(3)
  test.screenshot("house_balcony_door")
  test.key("w", true)
  test.wait(1.6)
  test.key("w", false)
  test.wait(1.0)
  local px, _, pz = player.pos()
  local out = (px - r.x) * d[1] + (pz - r.z) * d[2]
  test.check(out > r.width / 2 + 0.3, "out on the balcony (" .. out .. ")")
  test.check(out < r.width / 2 + 2.0, "the railing keeps the player on the balcony (" .. out .. ")")
  test.screenshot("house_balcony")
  player.yaw(yaw_of(d) + 1.2, -0.45)
  test.frames(4)
  test.screenshot("house_balcony_down")
end

-- forty rooms to the east, stepping around closed walls: always a way onward
local i, j = 0, 0
local visited, sizes, kinds40 = 0, {}, {}
while i < 40 do
  local r = house.room_at(i, j)
  local has = {}
  for _, s in ipairs(r.doors) do has[s] = true end
  test.check(#r.doors >= 3, "room " .. i .. "," .. j .. " has doors on three sides")
  if has.east then i = i + 1 elseif has.north then j = j + 1 else j = j - 1 end
  local n = house.room_at(i, j)
  player.set_pos(n.x, 0, n.z)
  test.frames(1)
  local nm, ri, rj = house.room()
  if ri ~= i or rj ~= j then test.check(false, "arrived in room " .. i .. "," .. j) end
  visited = visited + 1
  sizes[math.floor(n.width * n.length / 15)] = true
  kinds40[nm] = true
end
local nsizes, nkinds = 0, 0
for _ in pairs(sizes) do nsizes = nsizes + 1 end
for _ in pairs(kinds40) do nkinds = nkinds + 1 end
test.check(visited >= 40, "walked through " .. visited .. " rooms")
test.check(nsizes >= 3 and nkinds >= 5, "rooms differ: " .. nsizes .. " sizes, " .. nkinds .. " kinds")
local st = test.stats()
engine.log("far away", i, j, house.room(), st and st.draw_calls)
test.screenshot("house_far_room")

-- one press of the house key: back at the front door
tap("h")
test.frames(3)
local bn, bi, bj = house.room()
local px, _, pz = player.pos()
test.check(bn == "cabin" and bi == 0 and bj == 0 and math.abs(pz - 1.5) < 0.3, "back to the door with H")
test.screenshot("house_back_at_door")
-- and with the touch button
player.set_pos(house.room_at(1, 0).x, 0, house.room_at(1, 0).z)
test.frames(2)
test.touch("down", 9, 0.5, 0.5); test.frames(2); test.touch("up", 9, 0.5, 0.5); test.frames(2)
local hx, hy = test.touch_button("house")
test.check(hx ~= nil, "touch button for the house")
if hx then
  test.touch("down", 3, hx, hy); test.frames(2); test.touch("up", 3, hx, hy); test.frames(3)
  local tn, ti, tj = house.room()
  test.check(tn == "cabin" and ti == 0 and tj == 0, "back to the door with the touch button")
end
-- out of the front door
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
