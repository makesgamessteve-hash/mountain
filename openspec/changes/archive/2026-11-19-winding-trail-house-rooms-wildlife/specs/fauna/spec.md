# fauna Specification Delta

## MODIFIED Requirements

### Requirement: Script-defined animal species
Animal species SHALL be defined as data in scripts, each with its appearance, habitat (biomes and altitude range), active hours, active seasons, group size, movement speeds, comfort distance and sounds. The default data MUST include deer, ibex, chamois, wild boar, red squirrels, badgers, marmots, foxes, hares, mountain hares in their white winter coat, songbird flocks, ravens, a soaring eagle, butterflies (by day in spring and summer) and fireflies (on summer nights).

#### Scenario: Default species exist
- **WHEN** the game starts with the unmodified assets
- **THEN** the species data contains deer, ibex, chamois, wild boar, red squirrels, badgers, marmots, foxes, hares, mountain hares, songbirds, ravens, an eagle, butterflies and fireflies

#### Scenario: Adding a species by script
- **WHEN** a new animal species is added to the species data
- **THEN** it appears in its habitat during its active hours and seasons without rebuilding the executable
