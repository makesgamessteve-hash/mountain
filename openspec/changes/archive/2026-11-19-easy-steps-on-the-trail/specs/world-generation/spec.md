# world-generation Specification Delta

## MODIFIED Requirements

### Requirement: Marked hiking trail
Each world SHALL contain one hiking trail, an easy to see path that winds from a low trailhead through forest, meadows and alpine terrain to the main summit. The trail MUST avoid water, keep a gentle grade (zigzagging up steep flanks, level at the tips of switchbacks), rarely cross itself, be level across and walkable as a surface of its own (built up with dry-stone walls where the slope falls away), and be well marked.

#### Scenario: Trail connects trailhead and summit
- **WHEN** any world is generated
- **THEN** the trail starts at a low, gentle and dry trailhead at least 100 m inside the edge of the playable area
- **AND** the trail ends on the main summit, which keeps its height
- **AND** the trail is a continuous, light dirt path about 2.4 m wide that stands out from the surrounding ground

#### Scenario: Trail winds around the mountain
- **WHEN** any world is generated
- **THEN** the trail is at least 1.5 times as long as the straight distance from the trailhead to the summit
- **AND** it passes through at least two different biomes on its way up

#### Scenario: Trail is walkable and dry
- **WHEN** any world is generated
- **THEN** no point of the trail lies in a lake
- **AND** the grade along the trail, measured over any 20 m of it, stays at or below 30 percent
- **AND** across the trail the path is level, the terrain never rises above the path surface, and the player walks on the path surface

#### Scenario: Never stuck on the trail
- **WHEN** a walker follows the trail from the trailhead to the summit
- **THEN** it is never stopped: bends, switchback tips and the coarse ground beside the path do not block it
- **AND** where the path is built up, its edge keeps the walker on it
- **AND** from beside the path the walker steps or climbs onto it wherever the edge is not higher than a climbable ledge

#### Scenario: Cairns mark the trail
- **WHEN** any world is generated
- **THEN** a signpost and a cairn stand at the trailhead and a cairn on the summit
- **AND** painted trail posts stand beside the path about every 40 m and cairns about every 80 m
