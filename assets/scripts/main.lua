-- Mountain: game entry point. The C engine calls init(reloaded), update(dt),
-- action(name, pressed) and draw() - everything else (content, rules, menus) lives
-- in these scripts and can be changed or replaced by mods. MIT License.

local earth = require("content.earth")
local rooms = require("content.rooms")
local util = require("util")
local hud = require("hud")
local ambience = require("ambience")
local controls = require("content.controls")
local weather_data = require("content.weather")

state = state or {
  menu = false, gallery = false, gallery_index = 1, help = 14, debug = false,
  zoom_hold = 0, filters = {"none", "sepia", "bw", "vivid", "dusk"}, filter_index = 1,
}

local function load_settings()
  local s = util.deserialize(storage.load("settings"))
  if s then engine.settings(s) end
end

function save_settings()
  storage.save("settings", util.serialize(engine.settings()))
end

-- the current mountain and time speed are remembered between sessions
function save_world()
  storage.save("world", util.serialize{ seed = world.seed(), speed = time.speed() })
end

function new_world(seed)
  local s = world.generate(seed)
  time.set_hour(8)
  if not engine.test_mode() then save_world() end
  return s
end

function init(reloaded)
  load_settings()
  earth.planet()
  earth.biomes()
  earth.flora()
  earth.fauna()
  rooms.define()
  controls.apply()
  weather_data.apply()
  if not world.exists() then
    local saved = (not engine.test_mode()) and util.deserialize(storage.load("world")) or nil
    local seed = engine.test_mode() and 20261119 or (saved and saved.seed)
    if engine.platform() == "web" then world.params{ size = 385, cell = 7.0, erosion_drops = 40000 } end
    new_world(seed)
    if saved and saved.speed then time.speed(saved.speed) end
  end
  if engine.test_mode() then state.help = 0 end
  if reloaded then ui.message("Scripts reloaded", 2) end
end

local function set_filter(i)
  state.filter_index = (i - 1) % #state.filters + 1
  camera.filter(state.filters[state.filter_index])
end

local function close_overlays()
  state.menu = false
  state.gallery = false
  ui.modal(false)
end

local function house_action()
  if player.inside() then
    house.entrance() -- back to the front door of the cabin
  elseif house.can_unsummon() then
    house.unsummon()
  else
    local ok, msg = house.summon()
    if not ok and msg ~= "" then ui.message(msg, 3) end
  end
end

local gallery_actions = {
  prev = function() state.gallery_index = math.max(1, state.gallery_index - 1) end,
  next = function() state.gallery_index = math.min(math.max(1, photos.count()), state.gallery_index + 1) end,
  delete = function()
    if photos.count() == 0 then return end
    if state.confirm_delete and state.confirm_delete > 0 then
      photos.delete(state.gallery_index)
      state.gallery_index = math.max(1, math.min(state.gallery_index, photos.count() - 1))
      state.confirm_delete = 0
    else
      state.confirm_delete = 3
      ui.message("Delete this photo? Press Delete again.", 3)
    end
  end,
  download = function() if photos.count() > 0 then photos.download(state.gallery_index) end end,
  gallery = close_overlays,
  menu = close_overlays,
}

local camera_actions = {
  shoot = function() camera.shoot() end,
  zoom_in = function() camera.fov(camera.fov() - 5) end,
  zoom_out = function() camera.fov(camera.fov() + 5) end,
  filter = function() set_filter(state.filter_index + 1) end,
  next = function() set_filter(state.filter_index + 1) end,
  prev = function() set_filter(state.filter_index - 1) end,
  camera = function() camera.mode(false) end,
  menu = function() camera.mode(false) end,
  gallery = function() camera.mode(false); state.gallery = true; ui.modal(true) end,
}

local walk_actions = {
  house = house_action,
  interact = function() house.interact() end,
  camera = function() camera.mode(true); state.help = 0 end,
  gallery = function() state.gallery = true; state.gallery_index = 1; ui.modal(true) end,
  menu = function() state.menu = true; ui.modal(true) end,
  shoot = function()
    local _, active = engine.xr()
    if active then camera.shoot() else camera.mode(true) end
  end,
}

local global_actions = {
  debug = function() state.debug = not state.debug end,
  fullscreen = function() state.fullscreen = not state.fullscreen; engine.fullscreen(state.fullscreen) end,
  reload = function() if engine.platform() ~= "web" then engine.reload() end end,
}

function action(name, pressed)
  if not pressed then return end
  if name == "photo_saved" then
    ui.message("Photo saved", 2)
    state.recent_photo = 6   -- offer a download button for the new photo (web)
    return
  end
  if global_actions[name] then return global_actions[name]() end
  local handlers
  if state.gallery then handlers = gallery_actions
  elseif state.menu then handlers = { menu = close_overlays }
  elseif camera.mode() then handlers = camera_actions
  else handlers = walk_actions end
  local f = handlers[name]
  if f then f() end
end

function update(dt)
  if state.help > 0 then state.help = state.help - dt end
  if state.recent_photo and state.recent_photo > 0 then state.recent_photo = state.recent_photo - dt end
  if state.confirm_delete and state.confirm_delete > 0 then state.confirm_delete = state.confirm_delete - dt end
  ambience.update(dt)
end

function draw()
  hud.draw(state)
end
