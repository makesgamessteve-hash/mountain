# sky-and-time Specification Delta

## Purpose

Keeps the in-game clock and calendar and renders the sky that follows it (sun, moons, stars, milky way, sky colours, clouds and scene lighting), driven by planet data so that other planets can be described without engine changes.

## ADDED Requirements

### Requirement: In-game clock
The game SHALL keep an in-game clock with the day of the year and the time of day. By default one in-game hour MUST pass per real minute (a 24-hour day lasts 24 real minutes). A newly generated world starts at 08:00 on the first day of the year.

#### Scenario: Default clock speed
- **GIVEN** the time speed is 1x
- **WHEN** 60 real seconds pass
- **THEN** the in-game clock has advanced by one in-game hour, within one in-game minute

#### Scenario: Day rollover
- **GIVEN** the clock shows 23:59 on day 3
- **WHEN** two in-game minutes pass
- **THEN** the clock shows 00:01 on day 4

### Requirement: Time controls
The menu SHALL let the player set the time speed to paused, 1x, 10x or 60x, and set the hour of the day. Setting the hour MUST keep the current day and update the sky, lighting and ambience immediately.

#### Scenario: Pausing time
- **GIVEN** the time speed is set to paused
- **WHEN** 5 real minutes pass
- **THEN** the in-game clock has not advanced and the sun has not moved

#### Scenario: Fast time
- **GIVEN** the time speed is set to 60x
- **WHEN** 60 real seconds pass
- **THEN** the in-game clock has advanced by 60 in-game hours

#### Scenario: Setting the hour
- **GIVEN** it is 14:00
- **WHEN** the player sets the hour to 21 in the menu
- **THEN** the clock shows 21:00 on the same day and the sky shows evening or night colours for that time

### Requirement: Sun path by latitude and season
The sun's position SHALL be computed from the world latitude (default 46 degrees north), the planet's axial tilt and the day of the year, so that the sun rises in the east, culminates due south (north of the equator) at 12:00 and sets in the west, and day length varies with the season. The summer solstice falls on the middle day of summer and the winter solstice on the middle day of winter.

#### Scenario: Long summer days and short winter days
- **GIVEN** the default Earth-like planet and latitude
- **WHEN** the sunrise and sunset times are measured on the middle day of summer and the middle day of winter
- **THEN** the summer day lasts between 15 and 16 in-game hours
- **AND** the winter day lasts between 8 and 9 in-game hours

#### Scenario: Noon sun
- **WHEN** the clock shows 12:00 on any day at the default latitude
- **THEN** the sun is at its highest point of that day and stands due south

### Requirement: Moons with orbits and phases
The sky SHALL show every moon listed in the planet data, each moving on its own orbit with its own period, size and colour. Each moon MUST be lit from the sun's actual direction so its phase (new, crescent, half, gibbous, full) matches the angle between sun and moon. The default Earth-like planet has one moon with a phase cycle of 8 in-game days.

#### Scenario: Full moon opposite the sun
- **GIVEN** the default planet on a day when the moon is full
- **WHEN** the sun sets
- **THEN** the moon is near the opposite horizon, rising, and its whole visible disc is lit

#### Scenario: Phase follows the period
- **GIVEN** a moon with a phase cycle of 8 in-game days that is new at the start of day 1
- **WHEN** 4 in-game days pass
- **THEN** that moon is full
- **AND** after 2 in-game days a half-lit moon is shown with its lit side facing the sun

#### Scenario: Several moons
- **GIVEN** planet data that defines two moons with different periods
- **WHEN** the night sky is observed over several in-game days
- **THEN** two moons are visible, each moving and changing phase independently

### Requirement: Stars and milky way
On clear nights the sky SHALL show a field of stars and a milky-way band that fade in during dusk and fade out at dawn, rotate around the celestial pole once per sidereal day, and are hidden behind clouds, overcast, fog and precipitation.

#### Scenario: Night sky
- **GIVEN** clear weather
- **WHEN** the clock shows 00:00
- **THEN** stars and the milky-way band are visible

#### Scenario: Day sky
- **GIVEN** clear weather
- **WHEN** the clock shows 12:00
- **THEN** no stars are visible

#### Scenario: Sky rotation
- **GIVEN** clear weather at night
- **WHEN** 6 in-game hours pass
- **THEN** the star field has rotated by about 90 degrees around the celestial pole, and the star at the pole has stayed in place

### Requirement: Sky colours
The sky SHALL be drawn as a gradient from horizon to zenith whose colours follow the sun's altitude: blue by day and lighter near the horizon, warm orange, red and pink glows on the sun's side of the horizon at sunrise and sunset, deep blue twilight and a dark night sky. The day, sunset and night sky colours MUST come from the planet data.

#### Scenario: Daytime gradient
- **GIVEN** clear weather and the default planet
- **WHEN** the sun is 30 degrees above the horizon
- **THEN** the zenith is a saturated blue and the horizon a lighter, paler blue

#### Scenario: Sunset glow
- **GIVEN** clear weather
- **WHEN** the sun is at the horizon in the evening
- **THEN** the horizon around the sun shows warm orange to red colours that are warmer than the horizon opposite the sun

#### Scenario: Planet sky colours
- **GIVEN** planet data with violet day sky colours
- **WHEN** the daytime sky is shown
- **THEN** the sky gradient is violet instead of the default blue

### Requirement: Cloud layer
The sky SHALL contain a cloud layer whose coverage follows the current weather, that drifts with the wind and is coloured by the light of the sun and moon, including warm sunset colours.

#### Scenario: Coverage follows weather
- **WHEN** the weather is clear
- **THEN** clouds cover less than a quarter of the sky
- **AND** when the weather is overcast, clouds cover the whole sky

#### Scenario: Clouds drift
- **GIVEN** a wind blowing toward the east
- **WHEN** the clouds are observed for 30 real seconds
- **THEN** they have moved toward the east

### Requirement: Scene lighting follows the sky
The terrain, plants, animals and the house exterior SHALL be lit by the sun's direction and colour by day and by the moons by night, with brightness that depends on sun altitude, moon phase and cloud cover. At night a minimum ambient light MUST keep the terrain silhouette readable so the player can still walk.

#### Scenario: Night remains navigable
- **GIVEN** a new-moon night with overcast weather
- **WHEN** the player looks at the terrain ahead
- **THEN** the terrain shape and the trail remain distinguishable from the sky

#### Scenario: Moonlight
- **GIVEN** a clear night
- **WHEN** the moon is full compared with when it is new
- **THEN** the terrain is visibly brighter under the full moon

### Requirement: Planet data
The planet description SHALL come from script data and define at least the number of moons (up to four) with their periods, sizes and colours, the sky colours, the real length of a day, the season length, the latitude and the axial tilt. The game MUST use these values without engine changes; the default data describes an Earth-like planet with one moon and a 24-hour clock.

#### Scenario: Custom planet
- **GIVEN** planet data with two moons and a day length of 600 real seconds
- **WHEN** the clock runs
- **THEN** both moons cross the sky with their own phases
- **AND** each in-game day passes in 10 real minutes

#### Scenario: Default planet
- **WHEN** the game starts with the unmodified assets
- **THEN** the sky shows one moon, a blue sky and a 24-hour day
