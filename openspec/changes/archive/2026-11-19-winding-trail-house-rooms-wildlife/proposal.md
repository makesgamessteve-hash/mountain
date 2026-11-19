# Proposal

## Why

Playtesting the first version showed that the hiking trail is hard to see and climbs steep, bumpy ground in a mostly straight line; that the stairs of the magic house can end in mid-air or inside the slope; and that the endless rooms feel monotonous because they all have the same size and lie in one straight line. The owner also asked for more wildlife, a sauna, balconies and a quick way back to the entrance.

## What Changes

- The hiking trail winds through intermediate waypoints (forest, meadows, flanks, ridge) up to the summit, keeps a gentle grade with switchbacks, is graded flat across its width into the terrain, is wider and lighter, and is marked with painted trail posts and a signpost in addition to cairns.
- The stairs of the magic house are fitted to the terrain so that the last step always ends at ground level the player can step onto; the stilts house turns its stairs towards dry land.
- The interior of the magic house becomes an endless two-dimensional grid of rooms of different sizes and functions, connected by short corridors; every room has doorways on at least three sides. One side of every room holds its big window, and some rooms have a balcony there instead, open to the scenic view.
- A "back to the door" button (and the house key inside the house) takes the player straight to the entrance room.
- New room types: sauna, dining hall, bedroom, workshop, study and others, with new furniture.
- New animal species: chamois, wild boar, red squirrel, badger, snow hare (winter) and ravens.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `world-generation`: the marked hiking trail requirement (route, grade, flat cross-section, markers).
- `magic-house`: integration of the stairs, the main cabin's window, the endless rooms (grid, sizes, doors on three sides), the room windows, plus new requirements for balconies and returning to the entrance.
- `fauna`: the default species list.

## Impact

- Engine core: `terrain.c` (trail), `house.c` (stairs, entrance frame), `room.c` (grid of rooms, corridors, balconies, prefabs), `game.c` (rooms around the player, teleport), Lua API (`house.room`, `house.entrance`).
- Renderer: wider flat trail with markers; several room windows and balcony views as portals.
- Scripts: `content/rooms.lua` (room sizes and layouts), `content/earth.lua` (species), `hud.lua` (button).
- Tests: unit tests for trail, stairs and rooms; the house game test walks through the grid of rooms.
