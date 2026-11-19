# seasons Specification

## Purpose
Turns the in-game calendar into a year of four seasons that visibly change the landscape: foliage colours, blossoms and flowers, the snowline, fresh snow and which animals are around.

## Requirements

### Requirement: Four-season year
The in-game year SHALL consist of spring, summer, autumn and winter in that order, each lasting a configurable number of in-game days (default 7, giving a 28-day year). The year starts with the first day of spring and repeats after winter.

#### Scenario: Default season lengths
- **GIVEN** the default season length of 7 days
- **WHEN** the clock reaches day 8 of the year
- **THEN** the season changes from spring to summer
- **AND** after day 28 the year wraps around to day 1 of spring

#### Scenario: Configured season length
- **GIVEN** season data configured with 3 days per season
- **WHEN** the clock reaches day 4 of the year
- **THEN** the season is summer and the year lasts 12 days

### Requirement: Choosing the season
The menu SHALL let the player jump to spring, summer, autumn or winter. Choosing a season MUST move the calendar to the middle of that season, where its look is most typical, while keeping the time of day, and the landscape MUST show the new season within a few seconds without reloading the world.

#### Scenario: Jump to autumn
- **GIVEN** it is 10:00 on a summer day
- **WHEN** the player chooses autumn in the menu
- **THEN** the calendar shows 10:00 on the middle day of autumn
- **AND** within 5 real seconds the deciduous trees show autumn colours

### Requirement: Foliage colour blending
Foliage colours SHALL change continuously with the date: light green with blossoms on some trees and bushes in spring, full green in summer, yellow, orange and red in autumn, and bare deciduous trees in winter. Conifers MUST stay green all year. Colours MUST blend smoothly across days and season boundaries, never switching abruptly.

#### Scenario: Seasonal looks
- **WHEN** the date is in the middle of spring, summer, autumn and winter in turn
- **THEN** deciduous foliage is light green with blossoms, green, yellow-orange-red, and absent (bare branches) respectively
- **AND** conifers are green in all four cases

#### Scenario: No abrupt change at a boundary
- **WHEN** the clock passes midnight from the last day of summer to the first day of autumn
- **THEN** the foliage colour changes by only a small step compared with the hour before

### Requirement: Seasonal snowline
The snowline altitude SHALL vary through the year, highest in late summer and lowest in mid-winter. With the default data the summer snowline MUST lie above 80 percent of the terrain's height range and the mid-winter snowline below 40 percent. Ground above the snowline shows the snow biome.

#### Scenario: Winter snowline is lower
- **WHEN** the snowline is compared between the middle of summer and the middle of winter
- **THEN** the winter snowline is lower than the summer snowline
- **AND** in mid-winter the alpine meadows are covered by snow

### Requirement: Fresh snowfall and melting
While snow falls at a location, the ground, plants and roofs there SHALL whiten gradually, reaching full cover after about 2 in-game hours of continuous snowfall, even below the snowline. After snowfall stops, fresh snow below the snowline MUST melt back gradually over about 6 in-game hours (about a day in winter).

#### Scenario: Snow accumulates
- **GIVEN** a meadow below the snowline without snow cover
- **WHEN** snow falls there continuously for 2 in-game hours
- **THEN** the meadow ground appears fully white

#### Scenario: Snow melts
- **GIVEN** a meadow below the snowline fully covered by fresh snow outside winter
- **WHEN** 6 in-game hours pass without snowfall
- **THEN** the meadow shows its normal ground again

### Requirement: Flower blooming
Flowers SHALL be absent in winter, start blooming in spring, be most abundant from late spring through summer and fade during autumn, with the amount of visible flowers changing gradually with the date.

#### Scenario: Flowers by season
- **WHEN** the same meadow is observed in the middle of summer and in the middle of winter
- **THEN** many flowers are visible in summer and none in winter

### Requirement: Seasonal wildlife
Animal species SHALL only appear during the seasons listed in their species data, so that some wildlife is seasonal (for example butterflies only in spring and summer, fireflies only in summer, marmots hibernating in winter).

#### Scenario: Marmots hibernate
- **GIVEN** the default species data
- **WHEN** the player walks through alpine meadows on winter days
- **THEN** no marmots appear

#### Scenario: Fireflies only in summer
- **GIVEN** the default species data
- **WHEN** the player stands in a meadow at night in spring, summer, autumn and winter
- **THEN** fireflies appear only in summer
