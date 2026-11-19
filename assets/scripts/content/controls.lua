-- Mountain: keyboard map (SDL key names). Gamepad, touch and VR mappings are
-- fixed in the engine and documented in the README. MIT License.
local controls = {}

controls.keys = {
  forward = {"W", "Up"}, back = {"S", "Down"}, left = {"A", "Left"}, right = {"D", "Right"},
  prev = {"Left"}, next = {"Right"}, run = {"Left Shift", "Right Shift"}, jump = {"Space"},
  interact = {"E"}, house = {"H"}, camera = {"C"}, shoot = {"Return"}, gallery = {"G"}, menu = {"Escape"},
  zoom_in = {"=", "Keypad +"}, zoom_out = {"-", "Keypad -"}, filter = {"F"}, delete = {"Delete"},
  debug = {"F3"}, fullscreen = {"F11"}, reload = {"F5"}, turn_left = {"Q"}, download = {"X"},
}

function controls.apply()
  input.unbind_all()
  for action, keys in pairs(controls.keys) do
    for _, key in ipairs(keys) do input.bind(action, key) end
  end
end

return controls
