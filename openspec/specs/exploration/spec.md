# exploration Specification

## Purpose
Defines how the player moves through the world in first person (walking, running, jumping, slope limits, collisions and surface footsteps), the keyboard, mouse and gamepad controls, and the promise that the game has no failure states.

## Requirements

### Requirement: First-person walking
The player SHALL explore in first person with the eye about 1.7 m above the ground, walking at about 2.6 m/s and running at about 5.2 m/s while the run input is held. Movement MUST follow the terrain surface smoothly, uphill and downhill, at a speed independent of the frame rate.

#### Scenario: Walking speed
- **GIVEN** the player stands on flat ground
- **WHEN** the forward input is held for 4 real seconds
- **THEN** the player has moved about 10 m forward and the view stays about 1.7 m above the ground

#### Scenario: Running speed
- **GIVEN** the player stands on flat ground
- **WHEN** forward and run are held for 4 real seconds
- **THEN** the player has moved about 20 m forward

#### Scenario: Frame-rate independence
- **WHEN** the same 4-second forward input is replayed at 30 and at 60 frames per second
- **THEN** the distance travelled differs by less than 2 percent

### Requirement: Slope limit
The player SHALL NOT be able to walk up terrain steeper than about 50 degrees; moving into such a slope MUST stop the uphill motion and let the player slide sideways along it instead. Walking on gentler slopes and walking downhill is always possible. Ledges onto walkable ground MUST NOT stop the player: up to 0.6 m they are stepped up at once, up to 1.2 m they are climbed after walking into them for a moment, and the view follows such steps smoothly.

#### Scenario: Blocked by a cliff
- **GIVEN** the player stands at the foot of a 60-degree rock face
- **WHEN** the player walks straight into it for 5 real seconds
- **THEN** the player has not climbed more than the height of a jump up the face

#### Scenario: Steep but walkable
- **GIVEN** a 40-degree slope
- **WHEN** the player walks straight up it
- **THEN** the player climbs the slope

#### Scenario: Stepping onto a ledge
- **GIVEN** a flat ledge 0.5 m high in front of the player
- **WHEN** the player walks into it
- **THEN** the player is on top of the ledge without stopping

#### Scenario: Climbing a low wall
- **GIVEN** a flat-topped wall 1 m high in front of the player
- **WHEN** the player keeps walking into it
- **THEN** within half a second the player climbs onto it

### Requirement: Jumping
The player SHALL be able to jump about 1 m high while standing on the ground, and MUST NOT be able to jump again while in the air.

#### Scenario: Single jump
- **GIVEN** the player stands on flat ground
- **WHEN** jump is pressed twice within half a second
- **THEN** the player rises about 1 m once and lands back on the ground

### Requirement: Collisions
The player SHALL collide with tree trunks, boulders, cairns, the magic house's walls and furniture, and closed doors, sliding along them instead of passing through. Grass, flowers, ferns, mushrooms, bushes and animals MUST NOT block movement.

#### Scenario: Tree trunk blocks
- **WHEN** the player walks straight into a tree trunk
- **THEN** the player stops at the trunk surface and does not pass through it
- **AND** pressing sideways movement slides the player around the trunk

#### Scenario: Grass does not block
- **WHEN** the player walks through dense grass and flowers
- **THEN** the walking speed is unchanged

### Requirement: Surface footsteps
While the player walks or runs on the ground, footstep sounds SHALL play at a rate matching the movement speed, chosen by the surface underfoot: grass (forest, meadow and alpine ground), rock (rock, scree and trail), snow (snow biome or snow cover) and wood (house floors, platforms and stairs).

#### Scenario: Surface changes the sound
- **WHEN** the player walks from a meadow onto a rock slab and then onto snow
- **THEN** the footsteps change from grass to rock to snow sounds

#### Scenario: Wooden floor
- **WHEN** the player walks inside the magic house
- **THEN** wooden footsteps are heard

#### Scenario: Faster steps when running
- **WHEN** the player starts running
- **THEN** footsteps play more frequently than when walking

### Requirement: Keyboard and mouse controls
On desktop and web the default keyboard and mouse controls SHALL be: W, A, S, D or the arrow keys to move, mouse to look (pointer captured during play), Shift held to run, Space to jump, E to interact, H for the house, C for the camera, G for the gallery and Esc for the menu. Mouse capture MUST be released while a menu or the gallery is open.

#### Scenario: Moving and looking
- **WHEN** the player holds W and moves the mouse to the right
- **THEN** the player walks forward and the view turns to the right

#### Scenario: Opening the menu
- **WHEN** the player presses Esc during play
- **THEN** the menu opens and the mouse pointer is released
- **AND** pressing Esc again closes the menu and recaptures the pointer

### Requirement: Gamepad controls
When a gamepad is connected, the game SHALL accept: left stick to move, right stick to look, left stick click to toggle running, A to jump, X to interact, Y for the house, RB for the camera, Back/View for the gallery and Start for the menu. In menus the D-pad or left stick MUST move the selection, A confirm and B go back. Gamepads can be connected or disconnected at any time.

#### Scenario: Gamepad play
- **GIVEN** a connected gamepad
- **WHEN** the left stick is pushed forward and A is pressed
- **THEN** the player walks forward and jumps

#### Scenario: Hot-plugging
- **WHEN** a gamepad is connected while the game is running
- **THEN** it controls the player without restarting the game

#### Scenario: Gamepad menu navigation
- **GIVEN** the menu is open
- **WHEN** the player presses D-pad down and then A
- **THEN** the second menu entry is activated

### Requirement: No failure states
The game SHALL have no health, hunger, stamina, fall damage, death, failure or game-over state. Falls from any height, entering water, weather and nights MUST never harm or reset the player. The menu offers returning to the trailhead at any time.

#### Scenario: Falling from a cliff
- **WHEN** the player walks off a 30 m cliff
- **THEN** the player lands and can keep walking immediately without any penalty

#### Scenario: Return to the trailhead
- **WHEN** the player chooses "To trailhead" in the menu
- **THEN** the player stands at the trailhead of the current world, with time, weather and season unchanged
