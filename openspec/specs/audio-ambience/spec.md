# audio-ambience Specification

## Purpose
Creates the calm soundscape of the mountain: ambient loops mixed by place, time, season and weather, positional one-shot sounds for actions and events, a master volume, and audio start rules for browsers.

## Requirements

### Requirement: Context-mixed ambient loops
The game SHALL play ambient loops whose volumes follow the context: wind louder with altitude and wind speed, birdsong by day in spring and summer, crickets on summer nights, rain while it rains, a stream near water, and fireplace crackle inside the cabin. Outdoor ambience MUST be quieter and muffled while the player is inside the house.

#### Scenario: Summer day in the forest
- **GIVEN** a clear summer day
- **WHEN** the player stands in a forest at low altitude
- **THEN** birdsong is audible and the wind is soft

#### Scenario: Windy summit
- **WHEN** the player walks from the trailhead up to the summit on the same day and weather
- **THEN** the wind sound becomes progressively louder

#### Scenario: Summer night
- **WHEN** the player stands in a meadow on a summer night
- **THEN** crickets are audible and no birdsong plays

#### Scenario: Near water
- **WHEN** the player walks to within 20 m of a lake shore
- **THEN** a water sound fades in

#### Scenario: Inside the cabin
- **GIVEN** it is raining outside
- **WHEN** the player enters the main room
- **THEN** the fireplace crackle becomes audible and the rain sound becomes quieter and muffled

### Requirement: Smooth volume transitions
Changes of ambient volumes SHALL ramp smoothly over at least 1 second, and no sound MUST start or stop with an audible click.

#### Scenario: Rain stops
- **WHEN** the weather changes from rain to clear
- **THEN** the rain loop fades out gradually rather than stopping abruptly

### Requirement: One-shot sounds
The game SHALL play one-shot sounds for footsteps, the house door, the camera shutter, summoning and unsummoning the house, thunder, an occasional owl call at night and the marmot alarm whistle. Sounds of things in the world MUST be panned and attenuated by their direction and distance from the player.

#### Scenario: Owl at night
- **WHEN** the player spends 5 in-game hours at night in a forest
- **THEN** an owl call is heard at least once

#### Scenario: Positional sound
- **GIVEN** a marmot whistles to the player's left
- **WHEN** the sound plays
- **THEN** it is louder in the left channel than in the right channel

### Requirement: Master volume
The menu SHALL offer a master volume from 0 to 100 percent that scales all sounds; at 0 percent the game is silent.

#### Scenario: Muting
- **WHEN** the player sets the master volume to 0 percent
- **THEN** no sound is audible

#### Scenario: Half volume
- **WHEN** the player lowers the master volume from 100 to 50 percent
- **THEN** all sounds become quieter by the same amount

### Requirement: Audio start in browsers
In the web build, audio SHALL start after the player's first interaction with the page (a click, tap or key press), as required by browsers, and the game MUST run normally and silently until then.

#### Scenario: First tap starts audio
- **GIVEN** the web build has loaded and no interaction has happened yet
- **WHEN** the player taps the screen
- **THEN** the ambient sounds start playing at their current mix
