-- Automated game test: the hiking trail is a level, walkable, marked path from
-- the trailhead to the summit. MIT License.
local function look_at(x, y, z)
  local px, py, pz = player.pos()
  local dx, dy, dz = x - px, y - (py + 1.7), z - pz
  player.yaw(math.atan(dx, -dz), math.atan(dy, math.sqrt(dx * dx + dz * dz)))
end

time.pause(true)
time.set_hour(10)
local info = world.info()
test.check(info.trail_points > 200, "a long trail, got " .. info.trail_points)
test.check(info.trail_posts > 10, "painted trail posts, got " .. tostring(info.trail_posts))
test.check(world.trail_point(0) == nil and world.trail_point(info.trail_points + 1) == nil, "no points past the ends")

-- the player starts on the path, at the trailhead
local x1, y1, z1 = world.trail_point(1)
local px, py, pz = player.pos()
test.check(math.abs(px - x1) < 0.5 and math.abs(pz - z1) < 0.5, "player starts at the first trail point")

-- standing anywhere on the path, the player stands on its level surface
local function stand(i, name)
  local x, y, z = world.trail_point(i)
  local nx, ny, nz = world.trail_point(math.min(i + 12, info.trail_points))
  player.set_pos(x, y + 0.3, z)
  test.wait(0.5)
  local _, qy = player.pos()
  test.check(math.abs(qy - y) < 0.12, name .. ": on the path surface (" .. qy .. " vs " .. y .. ")")
  look_at(nx, ny, nz)
  test.screenshot(name)
end
stand(1, "trail_head")
stand(math.floor(info.trail_points * 0.35), "trail_forest")
stand(math.floor(info.trail_points * 0.8), "trail_high")
stand(info.trail_points - 6, "trail_summit")

-- walking forward along the path keeps the player on it
local i = math.floor(info.trail_points * 0.5)
local x, y, z = world.trail_point(i)
local nx, ny, nz = world.trail_point(i + 4)
player.set_pos(x, y + 0.2, z)
test.wait(0.3)
look_at(nx, ny + 1.7, nz)
test.key("w", true)
test.wait(1.0)
test.key("w", false)
test.wait(0.3)
local wx, wy, wz = player.pos()
local moved = math.sqrt((wx - x) ^ 2 + (wz - z) ^ 2)
test.check(moved > 1.5, "walked along the path, moved " .. moved)
test.finish(true)
