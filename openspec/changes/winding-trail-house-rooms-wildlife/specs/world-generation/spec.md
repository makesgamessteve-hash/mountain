# world-generation Specification Delta

## MODIFIED Requirements

### Requirement: Marked hiking trail
Each world SHALL contain one hiking trail, an easy to see path that winds from a trailhead at low altitude through forest, meadows and alpine terrain to the main summit. The trail MUST avoid water, keep a gentle grade (zigzagging up steep flanks), be flat across its width, and be marked by painted trail posts about every 40 m, stone cairns about every 80 m and a signpost at the trailhead.

#### Scenario: Trail connects trailhead and summit
- **WHEN** any world is generated
- **THEN** the trail starts at a low, gentle and dry trailhead at least 100 m inside the edge of the playable area
- **AND** the trail ends within 10 m of the main summit
- **AND** the trail is a continuous, light dirt path about 2.4 m wide that stands out from the surrounding ground

#### Scenario: Trail winds around the mountain
- **WHEN** any world is generated
- **THEN** the trail is at least 1.5 times as long as the straight distance from the trailhead to the summit
- **AND** it passes through at least two different biomes on its way up

#### Scenario: Trail is walkable and dry
- **WHEN** any world is generated
- **THEN** no point of the trail lies in a lake
- **AND** the grade along the trail, averaged over 20 m, stays below 25 percent wherever the terrain allows it
- **AND** across the trail the ground is level, and the terrain never rises above the path surface

#### Scenario: Cairns mark the trail
- **WHEN** any world is generated
- **THEN** a signpost and a cairn stand at the trailhead and a cairn on the summit
- **AND** painted trail posts stand beside the path about every 40 m and cairns about every 80 m
