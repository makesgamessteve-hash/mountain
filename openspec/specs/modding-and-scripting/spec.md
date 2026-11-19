# modding-and-scripting Specification

## Purpose
Keeps the game extensible: all content and game logic live as files beside the executable, written as scripts against a documented API, can be overridden by mods, reloaded while playing, and fail gracefully with visible error messages.

## Requirements

### Requirement: Assets beside the executable
All game content (scripts, shaders, textures, sounds and fonts) SHALL be read at run time from an `assets/` folder beside the executable and MUST NOT be compiled into the executable. In the web build the assets are served as a separate data package beside the program. If the asset folder is missing, the game shows an error naming the expected location instead of crashing.

#### Scenario: Changing a texture
- **WHEN** a texture file in `assets/textures/` is replaced and the game is restarted
- **THEN** the game shows the new texture without the executable being rebuilt

#### Scenario: Missing asset folder
- **GIVEN** a desktop build whose `assets/` folder has been removed
- **WHEN** the executable is started
- **THEN** an error message names the expected asset folder location and the program exits cleanly

### Requirement: Game logic in Lua scripts
The game logic and content SHALL be written as Lua 5.4 scripts, starting from the entry script `scripts/main.lua` in the asset folder. Scripts MUST define the plant and animal species, biomes, planet data, weather transition table, room types, menus and HUD, and the mapping of inputs to game actions.

#### Scenario: Content defined in scripts
- **WHEN** the default asset folder is inspected
- **THEN** species, biomes, planet, weather table, room types, menus, HUD and input mappings are defined in Lua script files under `scripts/`

#### Scenario: Remapping an action
- **WHEN** a script changes the key bound to the camera action from C to P
- **THEN** pressing P enters camera mode and C no longer does

### Requirement: Mod folder overrides
On desktop builds an optional `mods/` folder beside the executable SHALL take precedence over `assets/`: a file in `mods/` replaces the asset file with the same relative path, and a file that exists only in `mods/` is added. Without a `mods/` folder the game uses `assets/` unchanged.

#### Scenario: Overriding the planet
- **GIVEN** a file `mods/scripts/planet.lua` that defines two moons
- **WHEN** the game starts
- **THEN** it uses the modded planet with two moons instead of `assets/scripts/planet.lua`

#### Scenario: No mods folder
- **WHEN** the game starts without a `mods/` folder
- **THEN** all content comes from `assets/`

### Requirement: Script errors are shown, not fatal
A missing script, a syntax error or a run-time error in a script SHALL NOT crash the game. The game MUST show an on-screen error message naming the script file, line and error text, and keep running so that the error can be fixed and the scripts reloaded.

#### Scenario: Syntax error
- **GIVEN** a script with a syntax error on line 12
- **WHEN** the game loads it
- **THEN** an on-screen message shows the script's file name, line 12 and the error text
- **AND** the game process keeps running

#### Scenario: Run-time error in an update
- **WHEN** a script raises an error while the game is running
- **THEN** the error is shown on screen and the game keeps running without crashing

#### Scenario: Missing script
- **WHEN** a script required by another script does not exist
- **THEN** an on-screen message names the missing script

### Requirement: Script hot reload
In development builds and in desktop builds, pressing F5 SHALL reload all scripts while keeping the world seed, clock, weather, player position and house state, and clear any previously shown script error that has been fixed.

#### Scenario: Fixing an error live
- **GIVEN** an on-screen script error caused by a typo
- **WHEN** the typo is fixed in the file and F5 is pressed
- **THEN** the scripts reload, the error message disappears and the player is still at the same position

### Requirement: Restricted script environment
Scripts SHALL only reach files through the game's asset and user-data access and MUST NOT be able to start programs, load native libraries or read and write arbitrary files on the player's computer.

#### Scenario: Blocked system access
- **WHEN** a script tries to run an operating-system command or open a file outside the asset, mod and user-data folders
- **THEN** the attempt fails with a script error and nothing is executed or opened

### Requirement: Documented script API
The repository SHALL contain a script API reference describing every engine function, value and callback available to scripts, with parameters and an example. Every function exposed to scripts MUST appear in the reference, which is checked automatically.

#### Scenario: Reference is complete
- **WHEN** the automated documentation check runs
- **THEN** it passes only if every function exposed to scripts is described in the reference
