# magic-house Specification

## Purpose
Provides the magic house the player can summon anywhere: it blends into its surroundings as if it had always stood there, is entered through a door that shows the inside and outside through it, and contains a cozy main cabin followed by an endless chain of rooms whose big windows show scenic views of the mountain.

## Requirements

### Requirement: Summoning the house
When no house is summoned, pressing the house input (H, gamepad Y or the touch house button) SHALL make the house appear about 8 m in front of the player with its door facing the player, accompanied by a sparkle effect and a summon sound. The exact site MUST be chosen by analysing the surrounding terrain, water and trees.

#### Scenario: Summon on a meadow
- **GIVEN** the player stands on a wide gentle meadow with no house summoned
- **WHEN** the player presses H
- **THEN** the house appears with a sparkle effect and a summon sound
- **AND** its door is between 5 m and 12 m in front of the player and faces the player

#### Scenario: Summon with a gamepad
- **GIVEN** a connected gamepad and no house summoned
- **WHEN** the player presses Y
- **THEN** the house is summoned exactly as with H

### Requirement: House variants fit the site
The house SHALL take the variant that best fits its site: built into a steep hillside (its back part inside the slope, framed by a rock outcrop), a treehouse around or on a large nearby tree with a platform and stairs, a house on stilts beside a lake, or a cottage on gentle ground. For the same world, position and facing, the same site and variant MUST be chosen.

#### Scenario: Hillside
- **GIVEN** the ground at the summoning spot rises steeply away from the player
- **WHEN** the house is summoned
- **THEN** the house is built into the slope with its back part hidden inside the hill and a rock outcrop around it

#### Scenario: Treehouse
- **GIVEN** a large tree stands near the summoning spot
- **WHEN** the house is summoned
- **THEN** the house is built around or on that tree, with a platform and stairs leading down to the ground near the player

#### Scenario: Lakeside stilts
- **GIVEN** the summoning spot is on a lakeshore
- **WHEN** the house is summoned
- **THEN** the house stands on stilts on the shore beside the lake, with no part of it standing in the water

#### Scenario: Cottage
- **GIVEN** gentle open ground without a large tree, steep slope or lake nearby
- **WHEN** the house is summoned
- **THEN** a cottage appears whose stone foundation reaches the ground all around

#### Scenario: Same site every time
- **WHEN** the house is summoned, unsummoned and summoned again from the same position and facing
- **THEN** it appears at the same site with the same variant

### Requirement: Integrated as if always there
Whatever the variant, the house SHALL look as if it had always stood at its site: it MUST never float above the ground or leave visible gaps beneath it, its foundations, stilts or stairs reach the terrain, terrain never cuts through its door or entrance, and the way to the door is walkable.

#### Scenario: Uneven ground
- **GIVEN** a summoning site on uneven ground
- **WHEN** the house is summoned
- **THEN** no gap between the house and the terrain is visible from any side
- **AND** the player can walk from the summoning spot to the door

### Requirement: Weathered by its surroundings
The house's materials SHALL be weathered to match its biome and season: moss on stones and roof in forest and moist places, lichen in alpine and rocky places, and snow on the roof in winter or whenever the surrounding ground is snow covered.

#### Scenario: Snow on the roof
- **GIVEN** the surrounding ground is covered by snow
- **WHEN** the house is summoned
- **THEN** its roof is covered by snow

#### Scenario: Mossy forest house
- **WHEN** the house is summoned in a moist forest in summer
- **THEN** moss is visible on its stones and roof

### Requirement: Night glow and chimney smoke
From dusk to dawn the house's windows SHALL glow warmly and a lantern beside the door MUST be lit; chimney smoke rises at all times and drifts with the wind.

#### Scenario: House at night
- **GIVEN** a summoned house
- **WHEN** the clock reaches 22:00
- **THEN** its windows glow warmly and the lantern beside the door is lit
- **AND** smoke rises from the chimney and drifts in the wind direction

#### Scenario: House by day
- **WHEN** the clock shows 12:00
- **THEN** the lantern is not lit

### Requirement: Valid sites only
The house SHALL never be placed in water or partly outside the playable area. If no valid site exists near the player, no house MUST appear and a gentle message is shown instead.

#### Scenario: Facing a lake
- **GIVEN** the player stands on a shore facing open water
- **WHEN** the house is summoned
- **THEN** the house stands on dry land and no part of it is in the lake

#### Scenario: No suitable site
- **GIVEN** the player stands where no valid site exists nearby, such as a narrow rocky ridge at the edge of the playable area
- **WHEN** the player presses H
- **THEN** no house appears and a gentle message explains that the house could not find a cozy spot here

### Requirement: Door must be opened
The house door SHALL be closed after summoning. Pressing interact (E, gamepad X, the touch interact button or the VR trigger) while standing within about 3 m of the door MUST open it with a door sound, and pressing interact again closes it. A prompt is shown while the door is in reach. The door works the same from inside.

#### Scenario: Opening the door
- **GIVEN** the player stands 2 m in front of the closed door, looking at it
- **WHEN** the player presses E
- **THEN** the door swings open with a door sound

#### Scenario: Interaction prompt
- **WHEN** the player stands within 3 m of the door
- **THEN** a prompt names the interact input and the action (open or close)

### Requirement: Seeing through the open door
While the door is open, the main room SHALL be visible through the doorway from outside, and the outside world from inside, in correct perspective from every viewing position as if the two spaces were physically joined. While the door is closed, neither side is visible through it.

#### Scenario: Looking in from outside
- **GIVEN** the door is open and the player stands outside 5 m from it
- **WHEN** the player looks at the doorway and moves sideways
- **THEN** the warmly lit main room is visible through the doorway and its visible part shifts with the player's position like a real room behind the door

#### Scenario: Looking out from inside
- **GIVEN** the player is in the main room with the door open
- **WHEN** the player looks at the doorway
- **THEN** the real outside terrain, sky and weather are visible through it

### Requirement: Entering and leaving
The player SHALL only enter or leave the house through the open door. Walking through the open doorway MUST move the player seamlessly between outside and inside without a loading screen or fade, and a closed door blocks the way.

#### Scenario: Walking in
- **GIVEN** the door is open
- **WHEN** the player walks forward through the doorway
- **THEN** the player is inside the main room without any loading screen or fade

#### Scenario: Closed door blocks
- **GIVEN** the door is closed
- **WHEN** the player walks into the doorway
- **THEN** the player stays outside

#### Scenario: Walking out
- **GIVEN** the player is in the main room and the door is open
- **WHEN** the player walks out through the doorway
- **THEN** the player stands outside in front of the door

### Requirement: Cozy main cabin
The first room behind the front door SHALL always be a cozy wood cabin containing a huge bed, a kitchen with stove and shelves, a writer's desk with a journal, a bookshelf, a burning fireplace, an armchair, a rug, plants and lamps, and a large window showing the real view from the house's location in the current time, season and weather.

#### Scenario: Furnishing
- **WHEN** the player enters the main room
- **THEN** the bed, kitchen, desk with journal, bookshelf, burning fireplace, armchair, rug, plants and lamps are present

#### Scenario: Real window view
- **GIVEN** the house stands beside a lake at sunset in rain
- **WHEN** the player looks through the main room's large window
- **THEN** the actual surroundings of the house are visible at sunset in rain, matching what is seen outside

### Requirement: Endless chain of rooms
Every room SHALL have a door at the back leading to a next room, without end; a room is created when it is first needed. The main room is room 0 and the rooms behind it are numbered 1, 2, 3 and so on. Rooms after the main room MUST be chosen from room types defined in scripts (default: library, greenhouse, observatory, tea room, bath, music room and photo gallery room), the same for the same world and room number, with no two consecutive rooms of the same type.

#### Scenario: Next room on demand
- **GIVEN** the player is in room 3
- **WHEN** the player opens the back door
- **THEN** room 4 is visible through it and can be entered

#### Scenario: Truly endless
- **WHEN** an automated test walks through 100 consecutive rooms
- **THEN** every room has a back door to a further room
- **AND** the game's memory use stays bounded and it keeps running smoothly

#### Scenario: Rooms are stable
- **GIVEN** the player walked forward to room 5
- **WHEN** the player walks back to room 2 and then forward again to room 5
- **THEN** each room shows the same type and layout as before

#### Scenario: Script-defined room type
- **WHEN** a new room type is added to the room-type data
- **THEN** it can appear in the chain of rooms without rebuilding the executable

### Requirement: Scenic room windows
Every room after the main room SHALL have a big window showing a different scenic viewpoint of the same mountain, rendered live at the current time of day, season and weather; consecutive rooms MUST show different viewpoints.

#### Scenario: Different views
- **WHEN** the player compares the windows of rooms 1 and 2
- **THEN** they show views from two different scenic viewpoints

#### Scenario: Views follow time and weather
- **GIVEN** it is night and snowing outside
- **WHEN** the player looks through any room window
- **THEN** the view shows the mountain at night with snow falling

### Requirement: Photo gallery room
The photo gallery room type SHALL show the player's most recent photos, up to at least 6, framed on its walls, newest first, and include photos taken since the room was last visited. Frames without a photo MUST show an empty picture placeholder.

#### Scenario: Recent photos on the wall
- **GIVEN** the player has taken 3 photos
- **WHEN** the player enters a photo gallery room
- **THEN** those 3 photos hang framed on the wall and the remaining frames are empty

### Requirement: Unsummoning from outside
Pressing the house input while outside and within 25 m of the house SHALL unsummon it with a sparkle effect and a sound, after which the site looks exactly as before the house appeared.

#### Scenario: Unsummon nearby
- **GIVEN** the player stands outside 10 m from the house
- **WHEN** the player presses H
- **THEN** the house disappears with a sparkle effect and the original terrain and plants are visible again

### Requirement: Re-summoning from afar
Pressing the house input while outside and farther than 25 m from the house SHALL remove the house from its old site and summon it near the player, following the summoning rules.

#### Scenario: Calling the house along
- **GIVEN** the house stands 300 m behind the player
- **WHEN** the player presses H
- **THEN** the house vanishes from its old site and appears about 8 m in front of the player

### Requirement: No unsummoning from inside
The house SHALL NOT be unsummoned or moved while the player is inside it; pressing the house input inside MUST leave the house unchanged and show a gentle message that the house can only be dismissed from outside.

#### Scenario: Pressing H indoors
- **GIVEN** the player is in room 2 of the house
- **WHEN** the player presses H
- **THEN** the house and the player's position are unchanged and a gentle message is shown
