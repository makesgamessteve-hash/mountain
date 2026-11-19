-- Mountain: the Earth planet, its plants and animals.
-- Everything here is plain data: copy this file to make an alien or magic planet.
-- Colours are {r, g, b} in 0..1 (or 0..255). MIT License.

local earth = {}

function earth.planet()
  planet.set{
    name = "Earth",
    day_length = 1440,        -- real seconds per game day (1 game hour = 1 minute)
    days_per_season = 7,
    latitude = 46,
    axial_tilt = 23.44,
    moons = {
      { period = 8, phase = 0.45, size = 0.035, inclination = 5.1, color = {0.95, 0.93, 0.88} },
    },
  }
end

-- biome thresholds as fractions of the terrain's height range
function earth.biomes()
  world.params{ tree_line = 0.48, snow_line = 0.80 }
end

function earth.flora()
  flora.clear()
  -- trees -------------------------------------------------------------
  flora.species{ name = "spruce", kind = "tree", crown = "cone", height_min = 9, height_max = 22,
    trunk_radius = 0.32, crown_width = 0.22, tiles = {4, 5, 6, 7}, bark_tile = 6, bark_color = {0.42, 0.33, 0.26},
    colors = { spring = {0.30, 0.46, 0.30}, summer = {0.24, 0.40, 0.25}, autumn = {0.24, 0.38, 0.24}, winter = {0.22, 0.34, 0.25} },
    alt_min = 0.0, alt_max = 0.56, slope_max = 0.55, biomes = {"forest", "meadow", "alpine"}, density = 0.75, patchiness = 0.45 }
  flora.species{ name = "larch", kind = "tree", crown = "cone", deciduous = true, height_min = 10, height_max = 20,
    trunk_radius = 0.3, crown_width = 0.2, tiles = {4, 6}, bark_tile = 6, bark_color = {0.48, 0.36, 0.28},
    colors = { spring = {0.55, 0.70, 0.35}, summer = {0.40, 0.58, 0.30}, autumn = {0.88, 0.65, 0.22}, winter = {0.5, 0.45, 0.35} },
    alt_min = 0.25, alt_max = 0.6, slope_max = 0.55, biomes = {"forest", "alpine", "meadow"}, density = 0.35, patchiness = 0.5 }
  flora.species{ name = "beech", kind = "tree", crown = "round", deciduous = true, height_min = 8, height_max = 16,
    trunk_radius = 0.35, crown_width = 0.34, tiles = {0, 1, 2, 3}, bark_tile = 6, bark_color = {0.55, 0.53, 0.50},
    colors = { spring = {0.50, 0.72, 0.30}, summer = {0.32, 0.55, 0.22}, autumn = {0.85, 0.45, 0.15}, winter = {0.45, 0.35, 0.25} },
    alt_min = 0.0, alt_max = 0.36, slope_max = 0.5, moist_min = 0.2, biomes = {"forest", "meadow"}, density = 0.45, patchiness = 0.6 }
  flora.species{ name = "birch", kind = "tree", crown = "oval", deciduous = true, height_min = 7, height_max = 14,
    trunk_radius = 0.2, crown_width = 0.26, tiles = {30, 2}, bark_tile = 7, bark_color = {1, 1, 1},
    colors = { spring = {0.60, 0.78, 0.35}, summer = {0.42, 0.62, 0.28}, autumn = {0.95, 0.80, 0.25}, winter = {0.5, 0.42, 0.3} },
    alt_min = 0.05, alt_max = 0.45, slope_max = 0.45, moist_min = 0.35, biomes = {"forest", "meadow"}, density = 0.3, patchiness = 0.7 }
  flora.species{ name = "wild cherry", kind = "tree", crown = "round", deciduous = true, height_min = 5, height_max = 9,
    trunk_radius = 0.25, crown_width = 0.38, tiles = {1, 3}, bark_tile = 6, bark_color = {0.40, 0.28, 0.24}, blossom_tile = 28,
    colors = { spring = {0.52, 0.70, 0.32}, summer = {0.30, 0.50, 0.22}, autumn = {0.80, 0.30, 0.15}, winter = {0.4, 0.3, 0.25} },
    alt_min = 0.0, alt_max = 0.3, slope_max = 0.35, biomes = {"meadow"}, density = 0.12, patchiness = 0.8 }
  -- bushes and boulders ------------------------------------------------
  flora.species{ name = "juniper", kind = "bush", height_min = 0.8, height_max = 1.6, tiles = {8, 9},
    colors = { spring = {0.30, 0.45, 0.30}, summer = {0.28, 0.42, 0.28}, autumn = {0.32, 0.40, 0.25}, winter = {0.26, 0.36, 0.28} },
    alt_min = 0.3, alt_max = 0.78, slope_max = 0.6, biomes = {"alpine", "scree", "meadow"}, density = 0.35, patchiness = 0.5 }
  flora.species{ name = "hazel", kind = "bush", height_min = 1.2, height_max = 2.5, tiles = {8, 9},
    colors = { spring = {0.48, 0.66, 0.30}, summer = {0.35, 0.55, 0.25}, autumn = {0.80, 0.62, 0.25}, winter = {0.4, 0.33, 0.25} },
    alt_min = 0.0, alt_max = 0.4, slope_max = 0.5, biomes = {"forest", "meadow"}, density = 0.25, patchiness = 0.6 }
  flora.species{ name = "boulder", kind = "boulder", height_min = 0.7, height_max = 2.6, bark_color = {0.52, 0.50, 0.47},
    alt_min = 0.0, alt_max = 1.0, slope_max = 0.75, biomes = {"forest", "meadow", "alpine", "scree", "rock", "snow"},
    density = 0.35, patchiness = 0.5 }
  -- ground cover (grass is tinted by season, flowers bloom in their seasons)
  local grass = { spring = {0.55, 0.75, 0.32}, summer = {0.45, 0.65, 0.28}, autumn = {0.70, 0.62, 0.32}, winter = {0.6, 0.58, 0.45} }
  flora.species{ name = "grass", kind = "ground", height_min = 0.35, height_max = 0.7, tiles = {10, 11, 12, 13}, colors = grass,
    alt_min = 0.0, alt_max = 0.82, slope_max = 0.6, biomes = {"forest", "meadow", "alpine"}, density = 1.6, patchiness = 0.2 }
  flora.species{ name = "fern", kind = "ground", height_min = 0.5, height_max = 0.9, tiles = {14},
    colors = { spring = {0.45, 0.65, 0.30}, summer = {0.35, 0.58, 0.25}, autumn = {0.70, 0.50, 0.22}, winter = {0.45, 0.38, 0.28} },
    alt_min = 0.0, alt_max = 0.45, moist_min = 0.3, biomes = {"forest"}, density = 0.5, patchiness = 0.6 }
  flora.species{ name = "reeds", kind = "ground", height_min = 0.8, height_max = 1.3, tiles = {15},
    colors = grass, moist_min = 0.85, biomes = {"meadow", "forest", "alpine"}, density = 0.8, patchiness = 0.3 }
  local function flower(name, tile, seasons, alt_min, alt_max, biomes, density)
    flora.species{ name = name, kind = "ground", height_min = 0.25, height_max = 0.45, tiles = {tile},
      colors = { spring = {1, 1, 1}, summer = {1, 1, 1}, autumn = {1, 1, 1}, winter = {1, 1, 1} },
      bloom = seasons, alt_min = alt_min, alt_max = alt_max, biomes = biomes, density = density, patchiness = 0.75 }
  end
  flower("poppy", 16, {"summer"}, 0.0, 0.4, {"meadow"}, 0.25)
  flower("buttercup", 17, {"spring", "summer"}, 0.0, 0.5, {"meadow", "forest"}, 0.35)
  flower("gentian", 18, {"spring", "summer"}, 0.4, 0.85, {"alpine", "meadow"}, 0.35)
  flower("edelweiss", 19, {"summer"}, 0.55, 0.9, {"alpine", "scree"}, 0.25)
  flower("lavender", 20, {"summer"}, 0.0, 0.35, {"meadow"}, 0.2)
  flower("daisy", 21, {"spring", "summer", "autumn"}, 0.0, 0.5, {"meadow"}, 0.35)
  flower("alpine rose", 22, {"summer"}, 0.45, 0.75, {"alpine"}, 0.3)
  flower("arnica", 23, {"summer"}, 0.35, 0.7, {"alpine", "meadow"}, 0.25)
  flower("fly agaric", 24, {"autumn"}, 0.0, 0.5, {"forest"}, 0.12)
  flower("porcini", 25, {"autumn", "summer"}, 0.0, 0.5, {"forest"}, 0.12)
  flora.species{ name = "scree stones", kind = "ground", height_min = 0.3, height_max = 0.6, tiles = {26},
    colors = { spring = {1, 1, 1}, summer = {1, 1, 1}, autumn = {1, 1, 1}, winter = {1, 1, 1} },
    alt_min = 0.3, alt_max = 1.0, slope_max = 0.8, biomes = {"scree", "rock", "alpine"}, density = 0.5, patchiness = 0.3 }
end

function earth.fauna()
  fauna.clear()
  fauna.species{ name = "red deer", kind = "quadruped", size = 1.7, leg = 0.95, neck = 0.65, head = 0.28,
    color = {0.52, 0.34, 0.20}, color2 = {0.80, 0.70, 0.55}, horns = 1, tail = 0.12, walk_speed = 1.2, run_speed = 6,
    comfort = 22, alt_min = 0.0, alt_max = 0.55, biomes = {"forest", "meadow"}, from = 5, to = 21,
    seasons = {"spring", "summer", "autumn", "winter"}, group_min = 1, group_max = 4, max_groups = 2, chance = 0.4 }
  fauna.species{ name = "ibex", kind = "quadruped", size = 1.3, leg = 0.7, neck = 0.4, head = 0.24,
    color = {0.45, 0.40, 0.34}, color2 = {0.75, 0.70, 0.62}, horns = 2, tail = 0.08, walk_speed = 1.0, run_speed = 5,
    comfort = 18, alt_min = 0.45, alt_max = 0.95, biomes = {"alpine", "scree", "rock"}, from = 5, to = 20,
    group_min = 2, group_max = 5, max_groups = 2, chance = 0.5 }
  fauna.species{ name = "marmot", kind = "quadruped", size = 0.5, leg = 0.12, neck = 0.1, head = 0.12,
    color = {0.50, 0.40, 0.28}, color2 = {0.62, 0.52, 0.40}, horns = 0, tail = 0.15, walk_speed = 0.8, run_speed = 3,
    comfort = 14, alt_min = 0.4, alt_max = 0.8, biomes = {"alpine", "meadow", "scree"}, from = 7, to = 19,
    seasons = {"spring", "summer", "autumn"}, group_min = 1, group_max = 3, max_groups = 3, chance = 0.6, sound = "marmot" }
  fauna.species{ name = "fox", kind = "quadruped", size = 0.8, leg = 0.38, neck = 0.25, head = 0.16,
    color = {0.78, 0.38, 0.14}, color2 = {0.95, 0.92, 0.88}, horns = 3, tail = 0.45, walk_speed = 1.1, run_speed = 5,
    comfort = 20, alt_min = 0.0, alt_max = 0.6, biomes = {"forest", "meadow", "alpine"}, from = 17, to = 8,
    group_min = 1, group_max = 1, max_groups = 1, chance = 0.35 }
  fauna.species{ name = "hare", kind = "quadruped", size = 0.45, leg = 0.2, neck = 0.12, head = 0.11,
    color = {0.55, 0.45, 0.35}, color2 = {0.85, 0.80, 0.72}, horns = 3, tail = 0.05, walk_speed = 1.5, run_speed = 6,
    comfort = 15, alt_min = 0.0, alt_max = 0.7, biomes = {"meadow", "alpine", "forest"}, from = 5, to = 22,
    group_min = 1, group_max = 2, max_groups = 2, chance = 0.4 }
  fauna.species{ name = "songbirds", kind = "bird", size = 0.35, color = {0.25, 0.22, 0.20}, from = 5, to = 20,
    seasons = {"spring", "summer", "autumn"}, alt_min = 0.0, alt_max = 0.7, group_min = 4, group_max = 9, max_groups = 2, chance = 0.6 }
  fauna.species{ name = "golden eagle", kind = "soaring", size = 2.1, color = {0.30, 0.22, 0.14}, from = 8, to = 18,
    alt_min = 0.3, alt_max = 1.0, group_min = 1, group_max = 1, max_groups = 1, chance = 0.3 }
  fauna.species{ name = "peacock butterfly", kind = "butterfly", size = 0.35, tile = 32, from = 9, to = 18,
    seasons = {"spring", "summer"}, alt_min = 0.0, alt_max = 0.7, biomes = {"meadow", "alpine"},
    group_min = 2, group_max = 5, max_groups = 2, chance = 0.6 }
  fauna.species{ name = "blue butterfly", kind = "butterfly", size = 0.3, tile = 34, from = 9, to = 18,
    seasons = {"summer"}, alt_min = 0.2, alt_max = 0.85, biomes = {"meadow", "alpine"},
    group_min = 2, group_max = 4, max_groups = 2, chance = 0.5 }
  fauna.species{ name = "fireflies", kind = "firefly", size = 0.25, tile = 36, from = 21, to = 2,
    seasons = {"summer"}, alt_min = 0.0, alt_max = 0.45, biomes = {"forest", "meadow"},
    group_min = 10, group_max = 20, max_groups = 2, chance = 0.8 }
end

return earth
