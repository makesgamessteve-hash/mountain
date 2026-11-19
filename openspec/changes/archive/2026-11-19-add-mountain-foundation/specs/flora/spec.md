# flora Specification Delta

## Purpose

Covers the mountain with script-defined vegetation and boulders, placed deterministically by altitude, slope and moisture rules, rendered efficiently near and far, swaying in the wind and making room for the magic house.

## ADDED Requirements

### Requirement: Script-defined plant species
Plant species SHALL be defined as data in scripts, each with its kind, appearance, size range, seasonal appearance and placement rules (biomes, altitude, slope and moisture ranges, density and clustering). The default data MUST include conifer, deciduous and birch trees, bushes, grass, flowers, ferns, mushrooms and boulders.

#### Scenario: Default species present
- **WHEN** a world is generated with the default data
- **THEN** conifer, deciduous and birch trees, bushes, grass, flowers, ferns, mushrooms and boulders all occur somewhere in the playable area

#### Scenario: Adding a species by script
- **WHEN** a new flower species is added to the species data and the world is regenerated
- **THEN** the new flower appears in matching places without rebuilding the executable

### Requirement: Rule-based placement
Plants and boulders SHALL only be placed where their species rules allow: within their altitude, slope and moisture ranges and biomes. Moisture MUST be higher near lakes and in valleys than on ridges. No plant or boulder SHALL stand in a lake, and no tree, bush or boulder SHALL stand on the trail.

#### Scenario: No trees above the tree line
- **GIVEN** the default data
- **WHEN** a world is generated
- **THEN** no tree stands above the tree line or on rock steeper than its species' maximum slope

#### Scenario: Water and trail stay clear
- **WHEN** a world is generated
- **THEN** no plant or boulder stands inside a lake
- **AND** no tree, bush or boulder stands on the trail surface

#### Scenario: Moisture-loving plants
- **GIVEN** the default data
- **WHEN** a world is generated
- **THEN** ferns and mushrooms are more frequent in moist forest valleys than on dry ridges

### Requirement: Deterministic placement
For the same seed and species data, the position, species, size and orientation of every tree, bush and boulder SHALL be identical in every session on the same platform, regardless of the player's route. Dense ground cover (grass, flowers) MUST look the same at the same place each time the player returns.

#### Scenario: Same trees after regenerating
- **WHEN** the same seed is generated twice
- **THEN** every tree, bush and boulder has the same position, species, size and orientation

#### Scenario: Ground cover is stable
- **GIVEN** the player looked at a patch of flowers
- **WHEN** the player walks 500 m away and returns
- **THEN** the same flowers are at the same places

### Requirement: Tree rendering and detail levels
Trees SHALL be drawn with three-dimensional trunks and branches carrying camera-facing leaf-cluster sprites. With growing distance, trees MUST switch to cheaper representations, down to one or two upright sprites far away. Detail changes happen at distances (about 160 m and 520 m) where they are hardly noticeable at the low resolution.

#### Scenario: Near tree
- **WHEN** the player stands 10 m from a deciduous tree in summer
- **THEN** the tree shows a solid trunk with branches and many leaf-cluster sprites

#### Scenario: Far tree
- **WHEN** the same tree is 500 m away
- **THEN** it is drawn as one or two upright sprites without a trunk mesh
- **AND** at 300 m it is drawn with a simple trunk and a few large leaf clusters

### Requirement: Dense ground cover near the player
Grass and flowers SHALL be drawn densely around the player, within at least 30 m, with their density thinning out with distance so that no hard edge of the covered area is visible.

#### Scenario: Meadow around the player
- **WHEN** the player stands in a summer meadow
- **THEN** grass tufts and flowers cover the ground densely within 30 m
- **AND** their density decreases gradually beyond that distance

### Requirement: Wind sway
Vegetation SHALL sway with the current wind: grass, flowers and leaves noticeably while trunks stay still, with stronger and faster motion in stronger wind and almost no motion in calm weather.

#### Scenario: Calm versus storm
- **WHEN** the same meadow is observed in calm clear weather and during a storm
- **THEN** the grass is almost still in calm weather and sways strongly during the storm

### Requirement: Clearing for the magic house
While the magic house is summoned, plants and boulders that would intersect the house or its entrance SHALL be hidden, except a tree the house is built around or on. When the house is unsummoned, the hidden plants and boulders MUST reappear unchanged.

#### Scenario: Cottage on a meadow
- **GIVEN** a meadow with bushes and flowers at the chosen house site
- **WHEN** the house is summoned there
- **THEN** no plant or boulder pokes through the house or blocks its door

#### Scenario: Plants return
- **GIVEN** the house was summoned on a meadow
- **WHEN** the house is unsummoned
- **THEN** the plants and boulders that were hidden are visible again at their original places

#### Scenario: Treehouse keeps its tree
- **WHEN** the house is summoned as a treehouse around a large tree
- **THEN** that tree remains visible as part of the house
