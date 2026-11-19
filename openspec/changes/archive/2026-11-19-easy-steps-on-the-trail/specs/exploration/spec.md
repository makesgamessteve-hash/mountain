# exploration Specification Delta

## MODIFIED Requirements

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
