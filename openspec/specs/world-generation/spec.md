# world-generation Specification

## Purpose
Generates the playable mountain landscape (terrain, valleys, lakes, biomes, hiking trail, scenic viewpoints and the playable-area boundary) deterministically from a seed, so every world can be revisited and shared by its seed.

## Requirements

### Requirement: Seeded deterministic generation
The game SHALL generate each world from a 32-bit unsigned seed (0 to 4294967295). For the same seed and the same world settings, the terrain heights, water, biomes, trail, cairns, viewpoints and plant placements MUST be identical on the same platform, independent of frame rate, window size, rendering resolution or the order in which areas are visited. A world checksum MUST be available to scripts and tests.

#### Scenario: Same seed twice on one machine
- **GIVEN** a world generated from a seed with the default settings
- **WHEN** the world is generated again from the same seed with the same settings
- **THEN** the world checksum over terrain heights, water, biomes, trail, cairns and viewpoints is identical for both worlds

#### Scenario: Same seed on different platforms
- **WHEN** the same seed is generated on Linux, Windows and the web build
- **THEN** the main summit, lakes, trail and viewpoints are in the same places
- **AND** small numeric differences from the platforms' maths libraries never change the overall layout

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

#### Scenario: Valleys drain the landscape
- **WHEN** any world is generated
- **THEN** the main mountain's flanks contain branching channels that run downhill toward the valleys
- **AND** at least one outflow valley cuts through the surrounding ranges, so that no ring-shaped lake encircles the main mountain

### Requirement: Lakes fill closed basins
Every closed depression in the terrain that is deeper than 1.5 m and covers more than about 800 square metres SHALL be filled with a lake whose water surface is flat and lies at the height where the basin would spill over. Smaller hollows stay dry.

#### Scenario: Basin becomes a lake
- **GIVEN** a generated world containing a closed basin deeper than 1.5 m and larger than about 800 square metres
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
- **AND** ground steeper than about 60 degrees is rock, with scree on slopes steeper than about 50 degrees and just below the snowline
- **AND** gentle ground above the current snowline is snow

#### Scenario: Biome data from scripts
- **WHEN** a script changes the tree-line altitude (`world.params`) and the world is regenerated with the same seed
- **THEN** the forest boundary follows the new tree-line altitude without rebuilding the executable

### Requirement: Marked hiking trail
Each world SHALL contain one hiking trail, an easy to see path that winds from a low trailhead through forest, meadows and alpine terrain to the main summit. The trail MUST avoid water, keep a gentle grade (zigzagging up steep flanks, level at the tips of switchbacks), rarely cross itself, be level across and walkable as a surface of its own (built up with dry-stone walls where the slope falls away), and be well marked.

#### Scenario: Trail connects trailhead and summit
- **WHEN** any world is generated
- **THEN** the trail starts at a low, gentle and dry trailhead at least 100 m inside the edge of the playable area
- **AND** the trail ends on the main summit, which keeps its height
- **AND** the trail is a continuous, light dirt path about 2.4 m wide that stands out from the surrounding ground

#### Scenario: Trail winds around the mountain
- **WHEN** any world is generated
- **THEN** the trail is at least 1.5 times as long as the straight distance from the trailhead to the summit
- **AND** it passes through at least two different biomes on its way up

#### Scenario: Trail is walkable and dry
- **WHEN** any world is generated
- **THEN** no point of the trail lies in a lake
- **AND** the grade along the trail, measured over any 20 m of it, stays at or below 30 percent
- **AND** across the trail the path is level, the terrain never rises above the path surface, and the player walks on the path surface

#### Scenario: Never stuck on the trail
- **WHEN** a walker follows the trail from the trailhead to the summit
- **THEN** it is never stopped: bends, switchback tips and the coarse ground beside the path do not block it
- **AND** where the path is built up, its edge keeps the walker on it
- **AND** from beside the path the walker steps or climbs onto it wherever the edge is not higher than a climbable ledge

#### Scenario: Cairns mark the trail
- **WHEN** any world is generated
- **THEN** a signpost and a cairn stand at the trailhead and a cairn on the summit
- **AND** painted trail posts stand beside the path about every 40 m and cairns about every 80 m

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
