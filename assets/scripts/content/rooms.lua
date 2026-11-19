-- Mountain: the rooms of the magic house. Room 0 is always the cabin; the
-- endless rooms behind it are picked from the other types, never the same
-- twice in a row. Every room has a big window on its right (+x) wall.
-- Items: {"prefab", x = across (-w/2..w/2), z = along the room (0..9), rot = degrees}.
-- Prefabs: bed kitchen desk bookshelf fireplace armchair rug plant lamp table chair
-- tea_table bathtub piano telescope planter chest candles photo star_chart
-- window_seat barrel basket. MIT License.

local rooms = {}

function rooms.define()
  house.clear_rooms()
  house.define_room{ name = "cabin", cabin = true, width = 8, height = 3.6, wall_tile = 3, floor_tile = 2,
    light = {1.0, 0.78, 0.55},
    items = {
      {"fireplace", x = -3.45, z = 3.2, rot = 90},
      {"armchair", x = -1.7, z = 2.4, rot = 120},
      {"armchair", x = -1.8, z = 4.1, rot = 60},
      {"basket", x = -3.5, z = 1.5},
      {"bed", x = -2.5, z = 6.9, rot = 90},
      {"chest", x = -0.6, z = 6.9, rot = 90},
      {"bookshelf", x = -2.6, z = 8.78, rot = 180},
      {"plant", x = -1.2, z = 8.6},
      {"kitchen", x = 2.3, z = 8.4, rot = 180},
      {"table", x = 1.6, z = 6.0},
      {"chair", x = 0.9, z = 6.6, rot = 160},
      {"chair", x = 2.3, z = 5.4, rot = -20},
      {"desk", x = 2.9, z = 1.9, rot = -90},
      {"window_seat", x = 3.55, z = 4.6, rot = -90, scale = 0.9},
      {"rug", x = 0, z = 4.4, arg = 5},
      {"lamp", x = -3.5, z = 0.7},
      {"plant", x = 3.45, z = 0.5},
    } }
  house.define_room{ name = "library", width = 9, height = 5, wall_tile = 3, floor_tile = 2, light = {1.0, 0.8, 0.6},
    items = {
      {"bookshelf", x = -4.25, z = 1.5, rot = 90}, {"bookshelf", x = -4.25, z = 3.6, rot = 90},
      {"bookshelf", x = -4.25, z = 5.7, rot = 90}, {"bookshelf", x = -4.25, z = 7.8, rot = 90},
      {"bookshelf", x = 3.0, z = 8.7, rot = 180},
      {"armchair", x = 1.5, z = 4.0, rot = -60}, {"armchair", x = 1.5, z = 5.6, rot = -120},
      {"rug", x = 0.5, z = 4.8, arg = 4}, {"lamp", x = 2.6, z = 4.8}, {"tea_table", x = 2.2, z = 4.8},
      {"candles", x = -2.0, z = 8.5},
    } }
  house.define_room{ name = "greenhouse", width = 8, height = 4.5, wall_tile = 31, floor_tile = 5,
    wall_color = {0.85, 0.95, 0.85}, light = {0.9, 1.0, 0.85},
    items = {
      {"planter", x = -2.6, z = 2.0, rot = 90}, {"planter", x = -2.6, z = 4.6, rot = 90}, {"planter", x = -2.6, z = 7.2, rot = 90},
      {"planter", x = 1.0, z = 4.5}, {"plant", x = 3.2, z = 1.2}, {"plant", x = 3.2, z = 8.0}, {"plant", x = -1.6, z = 7.6},
      {"barrel", x = 2.5, z = 1.0}, {"chair", x = 2.2, z = 6.5, rot = -90},
    } }
  house.define_room{ name = "observatory", width = 8, height = 6, wall_tile = 3, floor_tile = 2, light = {0.6, 0.65, 1.0},
    items = {
      {"telescope", x = 2.5, z = 4.5, rot = 0}, {"star_chart", x = -3.95, z = 3.0, rot = 90},
      {"star_chart", x = -3.95, z = 6.0, rot = 90}, {"desk", x = -2.8, z = 4.5, rot = 90}, {"rug", x = 1.0, z = 4.5, arg = 4},
      {"candles", x = -3.2, z = 8.3},
    } }
  house.define_room{ name = "tea room", width = 7, height = 3.4, wall_tile = 31, floor_tile = 2,
    wall_color = {0.95, 0.9, 0.82}, light = {1.0, 0.82, 0.6},
    items = {
      {"rug", x = 0.5, z = 4.5, arg = 5}, {"tea_table", x = 0.6, z = 4.5}, {"window_seat", x = 3.05, z = 4.5, rot = -90, scale = 0.9},
      {"plant", x = -3.0, z = 1.0}, {"plant", x = -3.0, z = 8.0}, {"lamp", x = -3.0, z = 4.5}, {"chest", x = 2.6, z = 8.3, rot = 180},
    } }
  house.define_room{ name = "bath", width = 7, height = 3.6, wall_tile = 11, floor_tile = 5, light = {1.0, 0.9, 0.75},
    items = {
      {"bathtub", x = 1.6, z = 4.5, rot = 90}, {"plant", x = -2.9, z = 1.0}, {"candles", x = 2.8, z = 2.4},
      {"rug", x = -0.8, z = 4.5, arg = 3}, {"basket", x = -2.8, z = 7.8}, {"chair", x = -2.6, z = 4.5, rot = 90},
    } }
  house.define_room{ name = "music room", width = 8, height = 4, wall_tile = 3, floor_tile = 2, light = {1.0, 0.75, 0.55},
    items = {
      {"piano", x = -3.4, z = 4.5, rot = 90}, {"rug", x = 0.0, z = 4.5, arg = 5}, {"armchair", x = 1.6, z = 3.3, rot = -110},
      {"armchair", x = 1.6, z = 5.7, rot = -70}, {"lamp", x = -3.2, z = 7.9}, {"bookshelf", x = 2.6, z = 8.75, rot = 180},
      {"plant", x = 3.3, z = 8.2},
    } }
  house.define_room{ name = "photo gallery", width = 8, height = 3.8, wall_tile = 31, floor_tile = 2,
    wall_color = {0.95, 0.93, 0.88}, light = {1.0, 0.92, 0.8},
    items = {
      {"photo", x = -3.98, z = 1.6, rot = 90, arg = 0}, {"photo", x = -3.98, z = 3.6, rot = 90, arg = 1},
      {"photo", x = -3.98, z = 5.6, rot = 90, arg = 2}, {"photo", x = -3.98, z = 7.6, rot = 90, arg = 3},
      {"photo", x = -2.2, z = 8.98, rot = 180, arg = 4}, {"photo", x = 2.2, z = 8.98, rot = 180, arg = 5},
      {"window_seat", x = 3.55, z = 4.5, rot = -90, scale = 0.9}, {"rug", x = 0, z = 4.5, arg = 4}, {"plant", x = 3.2, z = 0.8},
    } }
end

return rooms
