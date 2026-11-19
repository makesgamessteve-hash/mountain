-- Mountain: game entry point. The C engine calls init(reloaded), update(dt),
-- action(name, pressed) and draw() - everything else (content, rules, menus) lives
-- in these scripts and can be changed or replaced by mods. MIT License.

local earth = require("content.earth")
local rooms = require("content.rooms")
local util = require("util")
local hud = require("hud")
local ambience = require("ambience")

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

function init(reloaded)
  load_settings()
  earth.planet()
  earth.flora()
  earth.fauna()
  rooms.define()
  if not world.exists() then
    local seed = engine.test_mode() and 20261119 or nil
    if engine.platform() == "web" then world.params{ size = 385, cell = 7.0, erosion_drops = 40000 } end
    world.generate(seed)
    time.set_hour(8.5)
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
  if house.can_unsummon() then
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
    if photos.count() > 0 then
      photos.delete(state.gallery_index)
      state.gallery_index = math.max(1, math.min(state.gallery_index, photos.count() - 1))
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
  shoot = function() camera.mode(true) end,
}

local global_actions = {
  debug = function() state.debug = not state.debug end,
  fullscreen = function() state.fullscreen = not state.fullscreen; engine.fullscreen(state.fullscreen) end,
  reload = function() if engine.platform() ~= "web" then engine.reload() end end,
}

function action(name, pressed)
  if not pressed then return end
  if name == "photo_saved" then
    if engine.platform() == "web" then ui.message("Photo saved - open the gallery (G) to download it", 3)
    else ui.message("Photo saved", 2) end
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
  ambience.update(dt)
end

function draw()
  hud.draw(state)
end
