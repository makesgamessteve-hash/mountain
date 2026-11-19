-- Mountain: heads-up display, menu, photo gallery and help. MIT License.
local util = require("util")
local hud = {}

local WHITE = {1, 1, 1, 1}
local DIM = {0.85, 0.85, 0.8, 0.9}
local PANEL = {0.08, 0.06, 0.05, 0.78}
local GOLD = {1.0, 0.85, 0.5, 1}

local function panel(x, y, w, h)
  ui.rect(x, y, w, h, PANEL)
  ui.rect(x, y, w, 1, {1, 0.85, 0.6, 0.35})
end

local function centered(y, text, color, size)
  local w = ui.size()
  size = size or 1
  ui.text(math.floor((w - ui.text_width(text, size)) / 2), y, text, color or WHITE, size)
end

local function status_line()
  local name, wx = weather.get()
  local season = time.season()
  local alt = player.altitude()
  local s = string.format("Day %d  %s  %s  %s  %d C  %dm", time.day(), util.clock_string(time.hour()),
    util.capitalize(season), util.capitalize(name), math.floor(wx.temperature + 0.5), math.floor(alt))
  ui.text(6, 6, s, DIM)
end

local function prompts()
  local w, h = ui.size()
  local touch = input.mode() == "touch"
  local dist, door = house.distance()
  local open = house.door()
  local p
  if player.inside() then
    local idx, name = house.room()
    centered(20, util.capitalize(name or "room") .. (idx and idx > 0 and ("  -  room " .. idx) or ""), GOLD)
    if door and door < 2.5 then p = open and "E: close the door" or "E: open the door" end
  elseif dist then
    if door and door < 3 then p = open and "E: close the door - walk in" or "E: open the door"
    elseif house.can_unsummon() then p = "H: send the house away" end
  end
  if p and not touch then centered(h - 28, p, WHITE) end
  local msg = ui.message()
  if msg then
    local tw = ui.text_width(msg)
    panel(math.floor((w - tw) / 2) - 6, h - 52, tw + 12, 16)
    centered(h - 48, msg, WHITE)
  end
end

local function help(state)
  if state.help <= 0 or input.mode() == "touch" then return end
  local w = ui.size()
  local lines = {
    "WASD walk  Shift run  Space jump  mouse look",
    "H summon the magic house   E open its door",
    "C camera (wheel zoom, F filter, click shoot)",
    "G photo gallery   Esc menu   F11 fullscreen",
  }
  panel(w - 372, 22, 366, 52)
  for i, l in ipairs(lines) do ui.text(w - 366, 16 + i * 11, l, DIM) end
end

local function camera_overlay(state)
  local w, h = ui.size()
  ui.text(8, h - 18, string.format("Zoom %d  Filter: %s", math.floor(camera.fov()), camera.filter()), WHITE)
  if input.mode() ~= "touch" then
    ui.text(w - 256, h - 18, "Click/Enter shoot  F filter  C exit", DIM)
  end
end

local function menu(state)
  local w, h = ui.size()
  local pw, ph = 300, 236
  local x, y = math.floor((w - pw) / 2), math.floor((h - ph) / 2)
  panel(x, y, pw, ph)
  ui.text(x + 10, y + 8, "Mountain", GOLD, 2)
  ui.text(x + 150, y + 14, "seed " .. world.seed(), DIM)
  local by = y + 34
  local function row(label) ui.text(x + 10, by + 5, label, DIM) end
  if ui.button("resume", x + 10, by, 84, 18, "Resume", 14) then state.menu = false; ui.modal(false) end
  if ui.button("newworld", x + 98, by, 122, 18, "New mountain", 33) then
    new_world(); state.menu = false; ui.modal(false)
  end
  if ui.button("seed", x + 224, by, 66, 18, "Seed", 25) then state.seed_entry = "" end
  by = by + 24
  row("Time")
  if ui.button("h-", x + 70, by, 24, 18, "-") then time.set_hour(time.hour() - 1) end
  ui.text(x + 100, by + 5, util.clock_string(time.hour()), WHITE)
  if ui.button("h+", x + 146, by, 24, 18, "+") then time.set_hour(time.hour() + 1) end
  local speeds = {{"||", 0}, {"1x", 1}, {"10x", 10}, {"60x", 60}}
  for i, s in ipairs(speeds) do
    if ui.button("speed" .. i, x + 176 + (i - 1) * 29, by, 27, 18, s[1]) then
      if s[2] == 0 then time.pause(true) else time.pause(false); time.speed(s[2]) end
      if not engine.test_mode() then save_world() end
    end
  end
  by = by + 22
  row("Season")
  for i, s in ipairs({"spring", "summer", "autumn", "winter"}) do
    if ui.button("season" .. i, x + 70 + (i - 1) * 55, by, 53, 18, s:sub(1, 6)) then time.set_season(s) end
  end
  by = by + 22
  row("Weather")
  local wxs = {"clear", "cloudy", "fog", "rain", "storm", "snow"}
  for i, s in ipairs(wxs) do
    local col = (i - 1) % 3
    local rowi = math.floor((i - 1) / 3)
    if ui.button("wx" .. i, x + 70 + col * 73, by + rowi * 20, 71, 18, s) then weather.set(s, true) end
  end
  by = by + 40
  if ui.button("wxauto", x + 70, by, 140, 18, "Changing weather") then weather.unlock() end
  by = by + 24
  local st = engine.settings()
  row("Pixels")
  for i, l in ipairs({180, 216, 270, 360}) do
    local label = (st.pixel_lines == l and "*" or "") .. l
    if ui.button("px" .. l, x + 70 + (i - 1) * 46, by, 44, 18, label) then engine.settings{ pixel_lines = l }; save_settings() end
  end
  by = by + 22
  row("Volume")
  if ui.button("vol-", x + 70, by, 24, 18, "-") then engine.settings{ volume = st.volume - 0.1 }; save_settings() end
  ui.text(x + 100, by + 5, string.format("%d%%", math.floor(st.volume * 100 + 0.5)), WHITE)
  if ui.button("vol+", x + 140, by, 24, 18, "+") then engine.settings{ volume = st.volume + 0.1 }; save_settings() end
  if ui.button("trail", x + 176, by, 114, 18, "To trailhead", 34) then player.trailhead(); state.menu = false; ui.modal(false) end
  by = by + 24
  local supported = engine.xr()
  if supported then ui.text(x + 10, by + 5, "VR: use the Enter VR button", GOLD) end
  if engine.platform() ~= "web" and ui.button("quit", x + 190, by, 100, 18, "Quit", 22) then save_settings(); engine.quit() end
end

local function seed_entry(state)
  local w, h = ui.size()
  local pw, ph = 168, 150
  local x, y = math.floor((w - pw) / 2), math.floor((h - ph) / 2)
  panel(x, y, pw, ph)
  ui.text(x + 8, y + 6, "Seed:", GOLD)
  ui.rect(x + 52, y + 4, pw - 60, 12, {0, 0, 0, 0.5})
  ui.text(x + 55, y + 6, state.seed_entry .. "_", WHITE)
  local keys = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "<", "0", "OK"}
  for i, k in ipairs(keys) do
    local col, row = (i - 1) % 3, math.floor((i - 1) / 3)
    if ui.button("key" .. k, x + 8 + col * 52, y + 22 + row * 24, 48, 20, k) then
      if k == "<" then
        state.seed_entry = state.seed_entry:sub(1, -2)
      elseif k == "OK" then
        local n = tonumber(state.seed_entry)
        if n and n >= 0 and n <= 4294967295 and math.floor(n) == n then
          new_world(math.floor(n)); state.seed_entry = nil; state.menu = false; ui.modal(false)
        else
          ui.message("A seed is a whole number from 0 to 4294967295", 3)
        end
      elseif #state.seed_entry < 10 then
        state.seed_entry = state.seed_entry .. k
      end
    end
  end
  if ui.button("seedcancel", x + 8, y + ph - 24, pw - 16, 18, "Cancel") then state.seed_entry = nil end
end

local function gallery(state)
  local w, h = ui.size()
  panel(8, 8, w - 16, h - 16)
  local n = photos.count()
  ui.text(16, 14, string.format("Photos (%d)", n), GOLD)
  if ui.button("gclose", w - 40, 12, 24, 18, "", 22) then state.gallery = false; ui.modal(false) end
  if n == 0 then
    centered(h / 2, "No photos yet - press C to take one.", DIM)
    return
  end
  state.gallery_index = math.max(1, math.min(state.gallery_index, n))
  -- large view of the selected photo
  local bw = math.min(w - 140, math.floor((h - 70) * 16 / 9))
  local bh = math.floor(bw * 9 / 16)
  ui.photo(state.gallery_index, 16, 34, bw, bh)
  ui.text(16, 38 + bh, photos.name(state.gallery_index) or "", DIM)
  -- thumbnails strip
  local tx = 24 + bw
  local first = math.max(1, state.gallery_index - 2)
  for i = first, math.min(n, first + 4) do
    local ty = 34 + (i - first) * 40
    if i == state.gallery_index then ui.rect(tx - 2, ty - 2, 68, 40, GOLD) end
    ui.photo(i, tx, ty, 64, 36)
    if ui.button("thumb" .. i, tx, ty, 64, 36, "") then state.gallery_index = i end
  end
  local by = h - 34
  if ui.button("gprev", 16, by, 60, 18, "Prev", 16) then state.gallery_index = math.max(1, state.gallery_index - 1) end
  if ui.button("gnext", 80, by, 60, 18, "Next", 17) then state.gallery_index = math.min(n, state.gallery_index + 1) end
  local dl = engine.platform() == "web" and "Download" or "Folder"
  if ui.button("gdl", 144, by, 92, 18, dl, 23) then photos.download(state.gallery_index) end
  local sure = state.confirm_delete and state.confirm_delete > 0
  if ui.button("gdel", 240, by, 76, 18, sure and "Sure?" or "Delete", 39) then
    if sure then
      photos.delete(state.gallery_index)
      state.gallery_index = math.max(1, state.gallery_index - 1)
      state.confirm_delete = 0
    else
      state.confirm_delete = 3
    end
  end
  if engine.platform() ~= "web" then ui.text(324, by + 5, photos.dir(), DIM) end
end

local function debug_overlay()
  local w = ui.size()
  local x, y, z = player.pos()
  local info = world.info()
  local fps = math.max(engine.fps(), 1)
  ui.text(w - 210, 6, string.format("%3d fps %5.1f ms  %s", math.floor(fps), 1000 / fps, engine.platform()), WHITE)
  ui.text(w - 210, 18, string.format("%.0f %.0f %.0f", x, y, z), DIM)
  ui.text(w - 210, 30, string.format("plants %d  lakes %d", info.plants, info.lakes), DIM)
  ui.text(w - 210, 42, string.format("biome %s  %s", world.biome(x, z), player.surface()), DIM)
end

function hud.draw(state)
  if not world.exists() then return end
  if state.gallery then gallery(state); return end
  if camera.mode() then camera_overlay(state); prompts(); return end
  status_line()
  prompts()
  help(state)
  if state.debug or engine.settings().show_fps then debug_overlay() end
  if state.menu then
    if state.seed_entry then seed_entry(state) else menu(state) end
  end
  if engine.platform() == "web" and state.recent_photo and state.recent_photo > 0 and photos.count() > 0 and not state.menu then
    local w = ui.size()
    if ui.button("dlrecent", w - 132, 22, 124, 18, "Download photo", 23) then photos.download(1); state.recent_photo = 0 end
  end
end

return hud
