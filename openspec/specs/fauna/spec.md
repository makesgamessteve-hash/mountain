# fauna Specification

## Purpose
Brings the mountain to life with script-defined, never-hostile animals that live in suitable habitats at suitable hours and seasons, wander and graze, calmly keep their distance from the player and appear and disappear around the player.

## Requirements

### Requirement: Script-defined animal species
Animal species SHALL be defined as data in scripts, each with its appearance, habitat (biomes and altitude range), active hours, active seasons, group size, movement speeds, comfort distance and sounds. The default data MUST include deer, ibex, chamois, wild boar, red squirrels, badgers, marmots, foxes, hares, mountain hares in their white winter coat, songbird flocks, ravens, a soaring eagle, butterflies (by day in spring and summer) and fireflies (on summer nights).

#### Scenario: Default species exist
- **WHEN** the game starts with the unmodified assets
- **THEN** the species data contains deer, ibex, chamois, wild boar, red squirrels, badgers, marmots, foxes, hares, mountain hares, songbirds, ravens, an eagle, butterflies and fireflies

#### Scenario: Adding a species by script
- **WHEN** a new animal species is added to the species data
- **THEN** it appears in its habitat during its active hours and seasons without rebuilding the executable

### Requirement: Habitat and activity schedule
An animal SHALL only appear in its species' habitat, during its active hours of the in-game day and during its active seasons. Ground animals MUST stay on walkable dry terrain; birds and insects fly.

#### Scenario: Butterflies by day
- **GIVEN** the default species data and a summer meadow
- **WHEN** the player stands in the meadow at 13:00 and again at 01:00
- **THEN** butterflies are present at 13:00 and absent at 01:00

#### Scenario: Fireflies at night
- **GIVEN** the default species data and a summer meadow
- **WHEN** the player stands in the meadow at 23:00
- **THEN** glowing fireflies are visible

#### Scenario: Habitat respected
- **WHEN** marmots and chamois are observed
- **THEN** they are only found in alpine and rocky terrain, never inside a lake or the forest

### Requirement: Calm wandering behaviour
Animals SHALL behave calmly and naturally: ground animals wander, graze and rest and follow the terrain without walking into water or climbing slopes steeper than they can; songbirds fly in flocks; the eagle circles high above the terrain by day.

#### Scenario: Grazing deer
- **GIVEN** a group of deer in a meadow and the player 100 m away
- **WHEN** the deer are observed for 60 real seconds
- **THEN** they alternate between walking slowly and standing still to graze, and stay on the terrain surface

#### Scenario: Soaring eagle
- **WHEN** the eagle is present
- **THEN** it circles in the air well above the terrain below it

### Requirement: Keeping a comfort distance
Each animal SHALL calmly move away when the player comes closer than its species' comfort distance, and settle again once it is beyond that distance. Animals MUST never move toward the player to confront, chase or block the player.

#### Scenario: Approaching a deer
- **GIVEN** a deer with a comfort distance of 25 m grazing in a meadow
- **WHEN** the player walks toward it until 20 m away
- **THEN** the deer moves away from the player until it is at least 25 m away and then resumes grazing

#### Scenario: Marmot alarm
- **GIVEN** a marmot in an alpine meadow
- **WHEN** the player comes closer than its comfort distance
- **THEN** the marmot whistles and runs away from the player

### Requirement: Never hostile
No animal SHALL attack, harm or hinder the player, and the player has no way to harm animals: there is no combat in the game.

#### Scenario: Standing next to wildlife
- **WHEN** the player approaches any animal as closely as possible
- **THEN** the animal never attacks and the player is never harmed or pushed

### Requirement: Spawning around the player
Animals SHALL be created and removed around the player so that wildlife is encountered everywhere without simulating the whole mountain. Ground animals and birds MUST appear at least 40 m from the player; butterflies and fireflies may appear closer but fade in gently. Animals are removed when farther than about 300 m, keeping the number of animals bounded.

#### Scenario: No popping in front of the player
- **WHEN** a ground animal or bird is created
- **THEN** it is placed at least 40 m from the player

#### Scenario: Bounded population
- **WHEN** the player walks across the mountain for 10 real minutes
- **THEN** the number of animals present at any time never exceeds the configured maximum
- **AND** animals farther than about 300 m from the player are removed
