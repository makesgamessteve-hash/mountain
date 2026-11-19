# weather Specification Delta

## Purpose

Provides changing weather (clear, cloudy, overcast, fog, rain, storm and snow) with season-weighted transitions, smooth blending, temperature-dependent precipitation, wind, lightning and thunder, and a way for the player to lock the weather for a chosen mood.

## ADDED Requirements

### Requirement: Weather states
The game SHALL support the weather states clear, cloudy, overcast, fog, rain, storm and snow. Each state MUST define its cloud cover, visibility, precipitation intensity, wind strength and light level, so that the states are clearly distinguishable.

#### Scenario: Fog limits visibility
- **WHEN** the weather is fully fog
- **THEN** distant terrain is hidden and objects farther than about 150 m are barely visible

#### Scenario: Storm look
- **WHEN** the weather is fully storm
- **THEN** the sky is overcast and dark, precipitation is heavy and the wind is strong

### Requirement: Automatic season-weighted transitions
Unless the weather is locked, the game SHALL change the weather automatically: each state lasts a random time between 2 and 8 in-game hours, then a different next state is chosen from a transition table in script data weighted by the current season. With the default data, snow MUST be most likely in winter and storms most likely in summer.

#### Scenario: Weather changes over time
- **GIVEN** automatic weather
- **WHEN** 2 in-game days pass
- **THEN** the weather has changed to a different state at least once

#### Scenario: Season weighting
- **GIVEN** the default transition table
- **WHEN** the weather sequence is sampled over many in-game days in winter and in summer
- **THEN** the share of time with snow is higher in winter than in summer
- **AND** the share of time with storms is higher in summer than in winter

### Requirement: Smooth weather blending
A change of weather SHALL blend cloud cover, fog, light, precipitation intensity and wind from the old to the new state over 30 to 60 real seconds, without sudden jumps. A new transition MUST NOT start before the current blend has finished.

#### Scenario: Rain sets in gradually
- **GIVEN** clear weather
- **WHEN** the weather changes to rain
- **THEN** the rain intensity rises progressively from none to full over 30 to 60 real seconds
- **AND** the cloud cover increases progressively over the same period

#### Scenario: Fast time does not skip blends
- **GIVEN** the time speed is 60x
- **WHEN** a weather transition starts
- **THEN** the next transition does not start until at least 30 real seconds later

### Requirement: Temperature and precipitation type
Each location SHALL have a temperature that depends on the season, the time of day and the altitude (colder higher up: about 16 degrees Celsius colder at the top of the terrain's height range than at its bottom, a compressed lapse rate that matches the stylised mountain). Precipitation from the rain, storm and snow states MUST fall as snowflakes where the local temperature is below 1 degree Celsius and as raindrops otherwise.

#### Scenario: Snow on the summit, rain in the valley
- **GIVEN** rain weather on a day when the summit is below freezing and the trailhead is above 1 degree Celsius
- **WHEN** the player stands at the summit and later at the trailhead
- **THEN** snowflakes fall at the summit and raindrops fall at the trailhead

#### Scenario: Winter snow
- **GIVEN** a mid-winter day with snow weather and a trailhead temperature below 1 degree Celsius
- **WHEN** the player stands at the trailhead
- **THEN** snowflakes fall there

### Requirement: Wind
The weather SHALL define a wind speed and direction that change smoothly. Wind MUST drive the sway of vegetation, the drift of clouds, the slant of falling precipitation and the volume of wind sounds; wind is calm in clear and fog weather and strongest in storms.

#### Scenario: Storm wind versus calm
- **WHEN** the weather changes from clear to storm
- **THEN** trees and grass sway more strongly, falling rain slants in the wind direction and the wind sound becomes louder

### Requirement: Lightning and thunder
During storms the game SHALL produce lightning flashes at random intervals of roughly 10 to 40 real seconds that briefly brighten the sky and scene, each followed by a thunder sound 1 to 4 seconds later, as from a strike a few hundred metres to a little over a kilometre away, coming from one side.

#### Scenario: Thunder follows lightning
- **GIVEN** storm weather
- **WHEN** a lightning strike happens
- **THEN** the sky flashes immediately and thunder is heard 1 to 4 seconds later

#### Scenario: No lightning outside storms
- **WHEN** the weather is clear, cloudy, overcast, fog, rain or snow
- **THEN** no lightning flashes occur

### Requirement: Weather lock
The menu SHALL let the player choose "automatic" or lock one of the weather states. Locking MUST blend to the chosen state and keep it indefinitely until the player selects "automatic" again.

#### Scenario: Locking fog
- **WHEN** the player locks the weather to fog
- **THEN** the weather blends to fog within 60 real seconds
- **AND** it stays fog for the next 3 in-game days at 60x time speed

#### Scenario: Back to automatic
- **GIVEN** the weather is locked to fog
- **WHEN** the player selects "automatic"
- **THEN** automatic transitions resume
