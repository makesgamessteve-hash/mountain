-- Automated game test: camera mode, zoom, filters, taking photos, the
-- gallery (browse, open folder message, delete) and the photo gallery room
-- of the magic house that shows the latest photos on its walls. MIT License.
local function tap(key)
  test.key(key, true); test.frames(2); test.key(key, false); test.frames(3)
end
time.pause(true)
time.set_hour(15)
weather.set("clear", true, true)
local n0 = photos.count()
tap("c")
test.check(camera.mode(), "C enters camera mode")
tap("="); tap("-"); test.wheel(1); test.frames(2)
local fov = camera.fov()
test.check(fov < 60, "zoomed in (" .. fov .. ")")
tap("f")
test.check(camera.filter() == "sepia", "F changes the filter")
test.screenshot("photo_viewfinder")
tap("return")
test.wait(0.3)
test.check(photos.count() == n0 + 1, "a photo was saved")
for _, f in ipairs({"bw", "vivid", "dusk", "none"}) do
  camera.filter(f); camera.shoot(); test.frames(3)
end
test.check(photos.count() == n0 + 5, "five photos")
tap("right"); tap("left")
tap("c")
test.check(not camera.mode(), "C leaves camera mode")
-- gallery
tap("g")
test.check(ui.modal(), "gallery open")
test.wait(0.3)
test.screenshot("photo_gallery")
tap("right"); tap("right"); tap("left")
tap("x")           -- desktop: shows where photos are saved
tap("delete")
tap("delete")         -- confirm
test.check(photos.count() == n0 + 4, "a photo was deleted")
tap("g")
test.check(not ui.modal(), "gallery closed")
-- photos hang in the gallery room of the house
local ok = false
for i = 1, 24 do
  ok = house.summon()
  if ok then break end
  player.yaw(i * 0.5, 0)
end
test.check(ok, "house summoned")
test.wait(2)
-- walk in through the door
local dx, dy, dz, yaw = house.door_pos()
local fx, fz = -math.sin(yaw), math.cos(yaw)
player.set_pos(dx + fx * 1.0, math.max(dy, world.height(dx + fx * 1.0, dz + fz * 1.0)) + 0.05, dz + fz * 1.0)
player.yaw(yaw, 0)
test.frames(3)
tap("e")
test.wait(1.2)
test.key("w", true); test.wait(1.5); test.key("w", false); test.frames(3)
test.check(player.inside(), "inside the house")
-- find the nearest photo gallery in the grid of rooms
local found
for r = 1, 8 do
  for i = -r, r do
    for j = -r, r do
      if not found and math.abs(i) + math.abs(j) == r and house.room_at(i, j).name == "photo gallery" then found = {i, j} end
    end
  end
end
test.check(found ~= nil, "found a photo gallery room")
if found then
  local r = house.room_at(found[1], found[2])
  player.set_pos(r.x, 0, r.z)
  test.frames(2)
  test.check(house.room() == "photo gallery", "standing in the photo gallery")
  -- photos hang on the back wall, opposite the window
  local back = {east = -math.pi / 2, west = math.pi / 2, north = 0, south = math.pi}
  player.yaw(back[r.window_side], 0.1)
  test.frames(4)
  test.screenshot("photo_gallery_room")
end
test.finish(true)
