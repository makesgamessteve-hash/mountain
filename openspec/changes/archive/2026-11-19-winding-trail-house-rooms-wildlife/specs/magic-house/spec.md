# magic-house Specification Delta

## MODIFIED Requirements

### Requirement: Integrated as if always there
Whatever the variant, the house SHALL look as if it had always stood at its site: it MUST never float above the ground or leave visible gaps beneath it, its foundations and stilts reach the terrain, terrain never cuts through its door or entrance, and the way to the door is walkable. Its stairs MUST end on ground the player can step onto: the last step at most 0.25 m above dry, walkable terrain.

#### Scenario: Uneven ground
- **GIVEN** a summoning site on uneven ground
- **WHEN** the house is summoned
- **THEN** no gap between the house and the terrain is visible from any side
- **AND** the player can walk from the summoning spot to the door

#### Scenario: Stairs land on the ground
- **WHEN** a cottage, hillside house, tree house or stilts house is summoned anywhere
- **THEN** its stairs run down until they meet the terrain, ending at most 0.25 m above dry ground
- **AND** the stairs of a house on stilts or in a tree may leave the platform on any side and lead to dry ground, never into the water
- **AND** a place where no stairs could reach the ground is not chosen for the house

### Requirement: Cozy main cabin
The first room behind the front door SHALL always be a cozy wood cabin containing a huge bed, a kitchen with stove and shelves, a writer's desk with a journal, a bookshelf, a burning fireplace, an armchair, a rug, plants and lamps, and a large window beside the front door showing the real view from the house's location in the current time, season and weather.

#### Scenario: Furnishing
- **WHEN** the player enters the main room
- **THEN** the bed, kitchen, desk with journal, bookshelf, burning fireplace, armchair, rug, plants and lamps are present

#### Scenario: Real window view
- **GIVEN** the house stands beside a lake at sunset in rain
- **WHEN** the player looks through the main room's large window
- **THEN** the actual surroundings of the house are visible at sunset in rain, matching what is seen outside

### Requirement: Endless chain of rooms
The rooms of the house SHALL form an endless grid that extends in every direction: every room MUST have doorways on at least three of its four sides, each leading through a short corridor to a neighbouring room. Rooms are created when needed and MUST differ in size (with their own ceiling height) and function, chosen from script-defined room types; they stay the same for the same world and position, and neighbouring rooms MUST be of different types.

#### Scenario: Doors on three sides
- **WHEN** the player enters any room
- **THEN** the room has doorways on at least three of its sides
- **AND** each doorway leads to another room

#### Scenario: Next room on demand
- **GIVEN** the player stands in a room
- **WHEN** the player looks through any of its doorways
- **THEN** the corridor and the next room are visible through it and can be entered

#### Scenario: Truly endless
- **WHEN** an automated test walks 40 rooms in one direction, stepping around the closed walls of the rooms on its way
- **THEN** every room it reaches has a doorway leading further in that direction or around the closed wall
- **AND** the game's memory use stays bounded and it keeps running smoothly

#### Scenario: Rooms are stable
- **GIVEN** the player walked three rooms east and two rooms north
- **WHEN** the player walks back and forth along the same way
- **THEN** each room shows the same type, size and layout as before

#### Scenario: Different sizes and functions
- **WHEN** the player visits ten different rooms
- **THEN** they are of at least five different types and at least three clearly different sizes

#### Scenario: Default room types
- **WHEN** the game starts with the unmodified assets
- **THEN** the room types are the cabin, library, greenhouse, observatory, tea room, bath, sauna, music room, photo gallery, dining hall, bedroom, workshop and study

#### Scenario: Script-defined room type
- **WHEN** a new room type is added to the room-type data
- **THEN** it can appear in the grid of rooms without rebuilding the executable

### Requirement: Scenic room windows
Every room other than the main room SHALL have a big window or a balcony on its fourth side showing a scenic viewpoint of the same mountain, rendered live at the current time of day, season and weather; neighbouring rooms MUST show different viewpoints.

#### Scenario: Different views
- **WHEN** the player compares the windows of two neighbouring rooms
- **THEN** they show views from two different scenic viewpoints

#### Scenario: Views follow time and weather
- **GIVEN** it is night and snowing outside
- **WHEN** the player looks through any room window
- **THEN** the view shows the mountain at night with snow falling

## ADDED Requirements

### Requirement: Room balconies
Some room types SHALL have a balcony instead of the big window: a doorway on the window side leads onto a railed balcony on the outside of the house wall, and around the balcony the scenic view of the mountain opens in every direction, as if the balcony hung 8 m above the viewpoint.

#### Scenario: Stepping onto a balcony
- **GIVEN** a room with a balcony
- **WHEN** the player walks through the balcony doorway
- **THEN** the player stands on a railed balcony with the scenic view in front, to the sides, above and below
- **AND** the railing keeps the player on the balcony

### Requirement: Back to the entrance
While inside the house, the player SHALL be able to return to the main room with a single action: the house input (key, gamepad button or the house button of the touch controls) or the "To the door" button of the menu. The player MUST then stand just inside the front door, facing it.

#### Scenario: Return from far inside
- **GIVEN** the player is ten rooms away from the main room
- **WHEN** the player presses the house key or taps the house button
- **THEN** the player stands in the main room facing the front door
