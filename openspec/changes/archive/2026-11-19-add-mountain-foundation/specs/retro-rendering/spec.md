# retro-rendering Specification Delta

## Purpose

Gives the game its realistic-yet-retro look: a low internal resolution upscaled with crisp pixels, ordered dithering with reduced colour depth, distance fog, a mix of 3D meshes, voxel models and sprites, and a pixel font, while keeping a smooth 60 frames per second.

## ADDED Requirements

### Requirement: Low internal resolution
The 3D scene SHALL be rendered at a low internal resolution of 216 lines by default, selectable in the menu from 180, 216, 270 and 360 lines. The internal width MUST follow the window's aspect ratio, and a changed setting applies immediately and is remembered for the next session.

#### Scenario: Default resolution on a 16:9 window
- **GIVEN** a 1920 by 1080 window and the default setting
- **WHEN** a frame is rendered
- **THEN** the scene is rendered at 384 by 216 pixels

#### Scenario: Changing the resolution
- **WHEN** the player selects 360 lines in the menu
- **THEN** the next frames are rendered at 360 lines without restarting
- **AND** the setting is still 360 lines after restarting the game

#### Scenario: Window aspect changes
- **GIVEN** the default setting
- **WHEN** the window is resized to 1080 by 1920 (portrait)
- **THEN** the scene is rendered 216 lines tall and about 122 pixels wide

### Requirement: Crisp nearest-neighbour upscaling
The low-resolution frame SHALL be enlarged to fill the window using nearest-neighbour sampling, so that retro pixels stay sharp-edged blocks without blur.

#### Scenario: Whole-number scale
- **GIVEN** a 1920 by 1080 window at 216 lines
- **WHEN** the frame is shown
- **THEN** every retro pixel covers exactly a 5 by 5 block of identical window pixels

### Requirement: Ordered dithering and reduced colour depth
The final image SHALL use a reduced colour depth (default 32 levels per colour channel) with an ordered dithering pattern, so that smooth gradients such as the sky appear as fine regular dither patterns instead of colour bands.

#### Scenario: Quantised colours
- **WHEN** a rendered frame is read back at its internal resolution
- **THEN** every colour channel value of every pixel is one of the 32 allowed levels

#### Scenario: Dithered sky gradient
- **WHEN** a clear daytime sky gradient is shown
- **THEN** transitions between neighbouring colour levels appear as a regular dither pattern

### Requirement: Distance fog
Objects SHALL fade with distance into a fog whose colour matches the sky near the horizon in the viewing direction, so the end of the visible world never shows a hard edge. Fog density MUST increase with fog, rain, snow and storm weather.

#### Scenario: Distant ranges fade
- **GIVEN** clear weather
- **WHEN** the player looks at mountain ranges far away
- **THEN** they are tinted toward the horizon sky colour and blend into the sky without a visible cut-off edge

#### Scenario: Denser in fog
- **WHEN** the weather changes from clear to fog
- **THEN** the distance at which objects disappear becomes much shorter

### Requirement: Mixed meshes, voxels and sprites
Scenes SHALL combine 3D meshes (terrain, tree trunks, house, boulders, animals), voxel models with small voxels (cairns, stonework, house details) and camera-facing sprites (leaf clusters, grass, flowers, particles, butterflies), all lit and fogged consistently. The overall look MUST read as realistic, with smooth terrain rather than block-built landscape.

#### Scenario: Cairn made of voxels
- **WHEN** the player looks at a cairn from 3 m away
- **THEN** it appears as a stack of stones built from small voxels, lit and fogged like the terrain around it

#### Scenario: Sprites face the camera
- **WHEN** the player walks around a bush of flowers
- **THEN** the flower sprites keep facing the camera

#### Scenario: Smooth terrain
- **WHEN** the player looks at a hillside
- **THEN** the terrain surface is continuous and smoothly shaded, without cube-shaped steps

### Requirement: Pixel font interface
The menus, HUD, messages and gallery SHALL use a pixel font drawn with whole-number scaling, so that each font pixel is a sharp square, at a size readable on phone screens.

#### Scenario: Crisp text
- **WHEN** the menu is open on a 1920 by 1080 window
- **THEN** all text uses the pixel font and every font pixel is drawn as a sharp square block

### Requirement: Frame-rate target
At default settings the game SHALL target a stable 60 frames per second on mid-range desktop hardware (for example a recent integrated laptop GPU) and on capable phones, synchronised with the display. A frame-time overlay MUST be toggleable with F3 to measure performance.

#### Scenario: Smooth frame rate
- **GIVEN** mid-range desktop hardware at the default settings
- **WHEN** the player walks through a forest for 60 seconds
- **THEN** the average frame time is at most 16.7 ms

#### Scenario: Frame-time overlay
- **WHEN** the player presses F3
- **THEN** the current frames per second and frame time are shown on screen
