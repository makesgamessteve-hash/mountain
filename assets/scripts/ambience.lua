-- Mountain: ambient soundscape mixed from the current context. MIT License.
local ambience = {}
local owl_timer = 30
local near_water = 0
local water_check = 0

local function water_nearby()
  local x, _, z = player.pos()
  for i = 0, 7 do
    local a = i * math.pi / 4
    for _, r in ipairs({8, 25}) do
      if world.biome(x + math.cos(a) * r, z + math.sin(a) * r) == "water" then return 1 end
    end
  end
  return 0
end

function ambience.update(dt)
  if not world.exists() then return end
  local _, wx = weather.get()
  local inside = player.inside()
  local day = time.daylight()
  local _, seasons = time.season()
  local alt = world.altitude(player.altitude())
  local room = select(2, house.room())
  water_check = water_check - dt
  if water_check <= 0 then
    water_check = 1.0
    near_water = inside and 0 or water_nearby()
  end
  local muffle = inside and 0.25 or 1.0
  audio.ambient("wind_loop", (0.12 + 0.55 * wx.wind + 0.35 * alt) * muffle, 3)
  audio.ambient("rain_loop", (wx.snowing and 0 or wx.precipitation) * 0.7 * (inside and 0.35 or 1), 4)
  local birds = day > 0.5 and seasons.winter < 0.5 and wx.precipitation < 0.2 and alt < 0.75
  audio.ambient("birds_loop", (birds and 0.35 or 0) * muffle, 5)
  local crickets = day < 0.2 and seasons.summer > 0.4 and wx.precipitation < 0.2 and alt < 0.6
  audio.ambient("crickets_loop", (crickets and 0.3 or 0) * muffle, 5)
  audio.ambient("stream_loop", near_water * 0.4, 3)
  audio.ambient("fire_loop", (inside and room == "cabin") and 0.5 or 0, 2)
  if not inside and day < 0.15 then
    owl_timer = owl_timer - dt
    if owl_timer <= 0 then
      audio.play("owl", 0.35, math.random() * 2 - 1)
      owl_timer = 40 + math.random() * 60
    end
  end
end

return ambience
