# world-generation Specification Delta

## Purpose

Generates the playable mountain landscape (terrain, valleys, lakes, biomes, hiking trail, scenic viewpoints and the playable-area boundary) deterministically from a seed, so every world can be revisited and shared by its seed.

## ADDED Requirements

### Requirement: Seeded deterministic generation
The game SHALL generate each world from a 32-bit unsigned seed (0 to 4294967295). For the same seed and the same world settings (planet and biome data), the generated terrain heights, water, biomes, trail, cairns, viewpoints and plant placements MUST be identical on every platform (Windows, Linux, Web), independent of frame rate, window size, rendering resolution or the order in which areas are visited.

#### Scenario: Same seed twice on one machine
- **GIVEN** a world generated from seed 12345 with the default settings
- **WHEN** the world is generated again from seed 12345 with the default settings
- **THEN** a checksum over the terrain heights, water, biomes, trail and viewpoints is identical for both worlds

#### Scenario: Same seed across platforms
- **WHEN** seed 12345 is generated with the default settings on Linux, Windows and the web build
- **THEN** all three builds report the same world checksum

#### Scenario: Different seeds differ
- **WHEN** worlds are generated from seeds 1 and 2
- **THEN** their world checksums differ

### Requirement: Main mountain and surrounding ranges
The generated terrain SHALL cover a playable square of about 3 km by 3 km containing one central main mountain whose summit is the highest point of the playable area, surrounded by lower mountain ranges, ridges and valleys, so that the main summit stands out from every direction.

#### Scenario: Summit is central and highest
- **WHEN** any world is generated
- **THEN** the playable area measures between 2.5 km and 3.5 km along each side
- **AND** the highest terrain point of the playable area lies within the central third of the area along both axes
- **AND** at least one other peak or ridge lower than the main summit exists in the surrounding terrain

### Requirement: Eroded valleys
The terrain SHALL show valleys and gullies shaped as if carved by flowing water (branching drainage channels running downhill from ridges), rather than smooth noise hills.

#### Scenario: Drainage reaches a lake or the edge
- **WHEN** any world is generated
- **THEN** following the steepest descent from any dry land point ends either in a lake or at the edge of the generated terrain
- **AND** the main mountain's flanks contain branching channels that run downhill toward the valleys

### Requirement: Lakes fill closed basins
Every closed depression in the terrain that is deeper than 0.5 m SHALL be filled with a lake whose water surface is flat and lies at the height where the basin would spill over.

#### Scenario: Basin becomes a lake
- **GIVEN** a generated world containing a closed basin deeper than 0.5 m
- **WHEN** the world is displayed
- **THEN** the basin holds a lake with a flat surface at the basin's spill height
- **AND** no terrain point inside the lake outline rises above the water surface except islands that are themselves above it

### Requirement: Altitude and slope biomes
Every terrain point SHALL be assigned a biome (forest, meadow, alpine, rock, scree or snow) from its altitude, slope and moisture according to biome data defined in scripts. Biomes MUST determine the ground appearance and which plants and animals can occur there.

#### Scenario: Default biome rules
- **GIVEN** the default biome data
- **WHEN** a world is generated
- **THEN** gentle ground below the tree line is forest or meadow
- **AND** gentle ground between the tree line and the snowline is alpine meadow
- **AND** ground steeper than about 40 degrees is rock, with scree on the moderate slopes below rock faces
- **AND** gentle ground above the current snowline is snow

#### Scenario: Biome data from scripts
- **WHEN** a script changes the tree-line altitude in the biome data and the world is regenerated with the same seed
- **THEN** the forest boundary follows the new tree-line altitude without rebuilding the executable

### Requirement: Marked hiking trail
Each world SHALL contain one hiking trail, a visible dirt path that leads from a trailhead at low altitude to the main summit, marked by stone cairns about every 80 m. The trail MUST avoid water and steep slopes.

#### Scenario: Trail connects trailhead and summit
- **WHEN** any world is generated
- **THEN** the trail starts at a trailhead whose altitude lies in the lowest third of the terrain's height range and at least 100 m inside the playable area
- **AND** the trail ends within 10 m of the main summit
- **AND** the trail is a continuous dirt path visible on the terrain

#### Scenario: Trail is walkable and dry
- **WHEN** any world is generated
- **THEN** no point of the trail lies in a lake
- **AND** no point of the trail lies on terrain steeper than the player's 50 degree climbing limit
- **AND** the average climb along the trail is at most 25 degrees

#### Scenario: Cairns mark the trail
- **WHEN** any world is generated
- **THEN** a cairn stands at the trailhead, at the summit and along the trail with consecutive cairns between 60 m and 100 m apart measured along the path

### Requirement: Player starts at the trailhead
When a world is entered for the first time, the player SHALL be placed at the trailhead, standing on the ground and facing along the trail toward the mountain.

#### Scenario: New world start position
- **WHEN** a new world is generated and entered
- **THEN** the player stands within 5 m of the trailhead cairn
- **AND** the view direction points along the first trail segment

### Requirement: Scenic viewpoint detection
The generator SHALL detect scenic viewpoints: walkable dry locations with a high open view (little nearby terrain blocking the horizon) and strong visible relief, each with a preferred view direction. Viewpoints MUST be ranked by scenic score and spaced apart so that they show different views.

#### Scenario: Enough distinct viewpoints
- **WHEN** any world is generated
- **THEN** at least 8 viewpoints are detected
- **AND** every viewpoint lies on dry, walkable ground inside the playable area
- **AND** any two viewpoints are at least 150 m apart

#### Scenario: Viewpoints are deterministic
- **WHEN** the same seed is generated twice with the same settings
- **THEN** the same viewpoints with the same view directions are detected in the same ranking order

### Requirement: Soft playable-area boundary
The player SHALL NOT be able to leave the playable area. Near its edge, movement away from the centre MUST be gently slowed and turned back without a visible wall, while the terrain beyond the edge remains visible as scenery.

#### Scenario: Walking toward the edge
- **WHEN** the player keeps running straight toward the edge of the playable area for 2 minutes
- **THEN** the player's position never leaves the playable area
- **AND** outward speed decreases progressively within the last 50 m before the edge instead of stopping abruptly

#### Scenario: Scenery beyond the edge
- **WHEN** the player stands near the edge and looks outward
- **THEN** terrain continues to be visible beyond the edge up to the fog distance

### Requirement: Generating a new world
The menu SHALL show the current world seed and offer to generate a new random mountain (with a randomly chosen seed) or to generate the world for a typed decimal seed. Generating a new world MUST dismiss the magic house and place the player at the new trailhead.

#### Scenario: New random mountain
- **WHEN** the player chooses "New random mountain" in the menu
- **THEN** a world with a different seed is generated and entered
- **AND** the menu shows the new seed

#### Scenario: Typed seed
- **GIVEN** the player previously explored the world of seed 42 and noted its trailhead view
- **WHEN** the player types the seed 42 in the menu and confirms
- **THEN** the same world is generated and the player starts at the same trailhead

#### Scenario: Invalid seed input
- **WHEN** the player types a value that is not a whole number between 0 and 4294967295
- **THEN** no world is generated and the menu shows a short hint about valid seeds
