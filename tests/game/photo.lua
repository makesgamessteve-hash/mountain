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
player.set_pos(dx + fx * 1.6, math.max(dy, world.height(dx + fx * 1.6, dz + fz * 1.6)) + 0.05, dz + fz * 1.6)
player.yaw(yaw, 0)
test.frames(3)
tap("e")
test.wait(1.2)
test.key("w", true); test.wait(1.5); test.key("w", false); test.frames(3)
test.check(player.inside(), "inside the house")
local found = false
for z = 0, 9 * 12, 9 do
  player.set_pos(0, 0, z + 4.5)
  test.frames(1)
  if select(2, house.room()) == "photo gallery" then found = true; break end
end
test.check(found, "found the photo gallery room")
if found then
  player.set_pos(0, 0, select(3, player.pos()))
  test.wait(0.1)
  player.yaw(-math.pi / 2, 0.1)
  test.frames(4)
  test.screenshot("photo_gallery_room")
end
test.finish(true)
