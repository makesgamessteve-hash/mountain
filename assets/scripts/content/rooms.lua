-- Mountain: the rooms of the magic house. MIT License.
--
-- The rooms form an endless grid. Every room has doorways in the middle of three
-- walls and, on the fourth, a big window or a balcony with a view of the mountain.
-- The cabin is always the first room: its fourth wall holds the front door.
--
-- Room-local coordinates (metres from the centre of the floor): +x points to the
-- window wall, z runs along it. "width" is the size across (towards the window),
-- "length" the size along the window wall; give a number or {min, max}: every room
-- of the type gets its own size in that range. "balcony" is the chance (0..1) that
-- a room of the type has a balcony instead of the window.
--
-- Items: {"prefab", x =, z =} in metres, or fx =, fz = as fractions of the half
-- width / half length, or against a wall: wall = "window" | "back" | "right" |
-- "left", along = -1..1 of the wall's half length (plus at = metres), inset = the
-- distance of the item's origin from the wall; wall items face into the room and
-- rot is added. Nothing is ever placed in front of a doorway.
-- Prefabs: bed kitchen desk bookshelf fireplace armchair rug plant lamp table chair
-- tea_table bathtub piano telescope planter chest candles photo star_chart
-- window_seat barrel basket sauna_stove sauna_bench bucket long_table sofa wardrobe
-- dresser workbench tool_rack globe chandelier.

local rooms = {}

function rooms.define()
  house.clear_rooms()
  house.define_room{ name = "cabin", cabin = true, width = 9, length = 11, height = 3.6, wall_tile = 3, floor_tile = 2,
    light = {1.0, 0.78, 0.55},
    items = {
      {"fireplace", wall = "left", along = 0.55, inset = 0.5},
      {"armchair", x = 3.1, z = -3.0, rot = 200},
      {"armchair", x = 1.6, z = -3.1, rot = 160},
      {"rug", x = 2.3, z = -2.6, arg = 4},
      {"basket", x = 4.0, z = -4.9},
      {"kitchen", wall = "left", along = -0.6, inset = 0.4},
      {"table", x = -2.5, z = -2.2, rot = 90},
      {"chair", x = -3.4, z = -2.2, rot = 90},
      {"chair", x = -1.6, z = -2.2, rot = -90},
      {"bed", wall = "back", along = 0.55, inset = 1.45},
      {"chest", x = -1.2, z = 3.0, rot = 90},
      {"bookshelf", wall = "back", along = -0.6, inset = 0.22},
      {"desk", wall = "window", along = 0.36, inset = 0.45},
      {"plant", x = 3.9, z = -1.8},
      {"plant", x = -4.0, z = 5.0},
      {"lamp", x = 3.9, z = 4.9},
      {"rug", x = -0.2, z = 1.0, arg = 5},
      {"sofa", wall = "right", along = 0.55, inset = 0.47},
    } }
  house.define_room{ name = "library", width = {7, 11}, length = {8, 12}, height = 5, balcony = 0.3,
    wall_tile = 3, floor_tile = 2, light = {1.0, 0.8, 0.6},
    items = {
      {"bookshelf", wall = "back", along = -0.75, inset = 0.22}, {"bookshelf", wall = "back", along = -0.4, inset = 0.22},
      {"bookshelf", wall = "back", along = 0.4, inset = 0.22}, {"bookshelf", wall = "back", along = 0.75, inset = 0.22},
      {"bookshelf", wall = "right", along = -0.6, inset = 0.22}, {"bookshelf", wall = "right", along = 0.6, inset = 0.22},
      {"bookshelf", wall = "left", along = -0.6, inset = 0.22}, {"bookshelf", wall = "left", along = 0.6, inset = 0.22},
      {"rug", fx = 0.3, arg = 4}, {"tea_table", fx = 0.3},
      {"armchair", fx = 0.3, z = -1.3, rot = 0}, {"armchair", fx = 0.3, z = 1.3, rot = 180},
      {"lamp", fx = 0.75, fz = 0.75}, {"globe", fx = -0.35, fz = 0.55},
      {"desk", wall = "window", along = 0.75, inset = 0.45},
    } }
  house.define_room{ name = "greenhouse", width = {6, 9}, length = {7, 11}, height = 4.5,
    wall_tile = 31, floor_tile = 5, wall_color = {0.85, 0.95, 0.85}, light = {0.9, 1.0, 0.85},
    items = {
      {"planter", wall = "back", along = -0.6, inset = 0.42}, {"planter", wall = "back", along = 0.6, inset = 0.42},
      {"planter", wall = "right", along = -0.55, inset = 0.42}, {"planter", wall = "left", along = 0.55, inset = 0.42},
      {"planter", x = 0, z = 0, rot = 90},
      {"plant", fx = 0.85, fz = 0.85}, {"plant", fx = 0.85, fz = -0.85}, {"plant", fx = -0.85, fz = 0.85},
      {"barrel", fx = -0.85, fz = -0.85}, {"chair", fx = 0.5, z = 1.8, rot = -90}, {"bucket", fx = 0.55, z = -1.6},
    } }
  house.define_room{ name = "observatory", width = {6, 8}, length = {6, 9}, height = 6, balcony = 0.7,
    wall_tile = 3, floor_tile = 2, light = {0.6, 0.65, 1.0},
    items = {
      {"telescope", fx = 0.45, z = 1.6, rot = -90},
      {"star_chart", wall = "back", along = -0.55, inset = 0.02}, {"star_chart", wall = "back", along = 0.55, inset = 0.02},
      {"desk", wall = "right", along = -0.6, inset = 0.45}, {"candles", wall = "right", along = -0.6, inset = 0.3},
      {"rug", x = 0, z = 0, arg = 4}, {"globe", fx = -0.6, fz = -0.7}, {"chair", fx = 0.2, fz = -0.4, rot = 45},
    } }
  house.define_room{ name = "tea room", width = {5, 6}, length = {5, 7}, height = 3.4, balcony = 0.5,
    wall_tile = 31, floor_tile = 2, wall_color = {0.95, 0.9, 0.82}, light = {1.0, 0.82, 0.6},
    items = {
      {"rug", x = 0.3, z = 0, arg = 4}, {"tea_table", x = 0.3, z = 0},
      {"window_seat", wall = "window", along = 0, inset = 0.36, scale = 0.9},
      {"plant", fx = -0.8, fz = 0.8}, {"plant", fx = -0.8, fz = -0.8}, {"lamp", fx = 0.8, fz = 0.8},
      {"chest", wall = "right", along = 0.65, inset = 0.32},
    } }
  house.define_room{ name = "bath", width = {5, 7}, length = {5, 7}, height = 3.6,
    wall_tile = 11, floor_tile = 5, light = {1.0, 0.9, 0.75},
    items = {
      {"bathtub", wall = "window", along = 0, inset = 0.65}, {"candles", fx = 0.75, fz = 0.75},
      {"plant", fx = -0.8, fz = -0.8}, {"rug", x = -0.6, z = 0, arg = 3}, {"basket", fx = -0.8, fz = 0.8},
      {"chair", fx = 0.7, fz = -0.75, rot = -60}, {"dresser", wall = "right", along = 0.6, inset = 0.27, scale = 0.8},
    } }
  house.define_room{ name = "sauna", width = {4.5, 5.5}, length = {4.5, 6}, height = 3.0,
    wall_tile = 3, floor_tile = 3, wall_color = {0.95, 0.8, 0.6}, floor_color = {0.9, 0.75, 0.55}, light = {1.0, 0.65, 0.4},
    items = {
      {"sauna_bench", wall = "window", along = 0, inset = 0.96},
      {"sauna_stove", fx = -0.7, fz = -0.72},
      {"bucket", fx = -0.25, fz = -0.75},
      {"basket", fx = -0.75, fz = 0.75},
    } }
  house.define_room{ name = "music room", width = {7, 9}, length = {7, 10}, height = 4, balcony = 0.3,
    wall_tile = 3, floor_tile = 2, light = {1.0, 0.75, 0.55},
    items = {
      {"piano", wall = "back", along = 0.55, inset = 0.32}, {"rug", x = 0.5, z = 0, arg = 5},
      {"armchair", fx = 0.4, z = -1.2, rot = 30}, {"armchair", fx = 0.4, z = 1.2, rot = 150},
      {"lamp", fx = -0.8, fz = -0.8}, {"bookshelf", wall = "right", along = 0.6, inset = 0.22},
      {"plant", fx = 0.85, fz = -0.85}, {"sofa", wall = "left", along = -0.55, inset = 0.47},
    } }
  house.define_room{ name = "photo gallery", width = {6, 8}, length = {9, 13}, height = 3.8,
    wall_tile = 31, floor_tile = 2, wall_color = {0.95, 0.93, 0.88}, light = {1.0, 0.92, 0.8},
    items = {
      {"photo", wall = "back", along = -0.75, inset = 0.02, arg = 0}, {"photo", wall = "back", along = -0.4, inset = 0.02, arg = 1},
      {"photo", wall = "back", along = 0.4, inset = 0.02, arg = 2}, {"photo", wall = "back", along = 0.75, inset = 0.02, arg = 3},
      {"photo", wall = "right", along = -0.55, inset = 0.02, arg = 4}, {"photo", wall = "right", along = 0.55, inset = 0.02, arg = 5},
      {"photo", wall = "left", along = -0.55, inset = 0.02, arg = 6}, {"photo", wall = "left", along = 0.55, inset = 0.02, arg = 7},
      {"window_seat", wall = "window", along = 0, inset = 0.36, scale = 0.9}, {"rug", x = 0, z = 0, arg = 4},
      {"plant", fx = 0.85, fz = 0.85}, {"plant", fx = 0.85, fz = -0.85},
    } }
  house.define_room{ name = "dining hall", width = {9, 12}, length = {11, 13}, height = 5, balcony = 0.4,
    wall_tile = 0, floor_tile = 2, light = {1.0, 0.8, 0.55},
    items = {
      {"long_table", x = 0, z = -2.0, rot = 90}, {"long_table", x = 0, z = 2.0, rot = 90},
      {"chandelier", x = 0, z = -2.0, arg = 32}, {"chandelier", x = 0, z = 2.0, arg = 32},
      {"fireplace", wall = "right", along = 0.55, inset = 0.5}, {"fireplace", wall = "left", along = -0.55, inset = 0.5},
      {"dresser", wall = "back", along = -0.55, inset = 0.27}, {"dresser", wall = "back", along = 0.55, inset = 0.27},
      {"plant", fx = 0.85, fz = 0.85}, {"plant", fx = 0.85, fz = -0.85}, {"plant", fx = -0.85, fz = 0.85},
      {"barrel", fx = -0.85, fz = -0.85}, {"rug", x = 0, z = 0, arg = 6},
    } }
  house.define_room{ name = "bedroom", width = {6, 8}, length = {6, 8}, height = 3.4, balcony = 0.6,
    wall_tile = 3, floor_tile = 2, wall_color = {0.98, 0.92, 0.86}, light = {1.0, 0.78, 0.6},
    items = {
      {"bed", wall = "back", along = 0.55, inset = 1.45}, {"chest", x = -0.2, fz = 0.55, rot = 90},
      {"wardrobe", wall = "left", along = 0.6, inset = 0.33}, {"dresser", wall = "right", along = -0.6, inset = 0.27},
      {"rug", x = 0, z = 0, arg = 3}, {"lamp", fx = -0.8, fz = -0.8}, {"plant", fx = 0.8, fz = 0.8},
      {"armchair", fx = 0.55, fz = -0.55, rot = 60},
    } }
  house.define_room{ name = "workshop", width = {6, 9}, length = {7, 10}, height = 4,
    wall_tile = 3, floor_tile = 3, wall_color = {0.9, 0.85, 0.78}, light = {1.0, 0.85, 0.65},
    items = {
      {"workbench", wall = "back", along = 0.55, inset = 0.47}, {"tool_rack", wall = "back", along = 0.55, inset = 0.06},
      {"workbench", wall = "left", along = 0.55, inset = 0.47},
      {"barrel", fx = 0.85, fz = 0.85}, {"barrel", fx = 0.7, fz = 0.85}, {"chest", wall = "right", along = 0.6, inset = 0.32},
      {"lamp", fx = -0.8, fz = -0.8}, {"basket", fx = 0.8, fz = -0.8}, {"chair", x = -0.6, z = 1.5, rot = 180},
    } }
  house.define_room{ name = "study", width = {5, 7}, length = {6, 8}, height = 3.6, balcony = 0.4,
    wall_tile = 3, floor_tile = 2, light = {1.0, 0.82, 0.6},
    items = {
      {"desk", wall = "right", along = 0.55, inset = 0.45}, {"candles", wall = "right", along = 0.55, inset = 0.3},
      {"bookshelf", wall = "back", along = -0.6, inset = 0.22}, {"bookshelf", wall = "back", along = 0.6, inset = 0.22},
      {"armchair", fx = 0.45, fz = -0.5, rot = 45}, {"lamp", fx = 0.8, fz = -0.8}, {"globe", fx = -0.6, fz = -0.7},
      {"rug", x = 0, z = 0, arg = 3}, {"chest", wall = "left", along = 0.6, inset = 0.32},
    } }
end

return rooms
