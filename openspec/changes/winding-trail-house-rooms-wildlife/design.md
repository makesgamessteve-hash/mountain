# Design

## Context

Builds on the archived change `add-mountain-foundation`. The terrain grid has 6 m cells, so a narrow path cannot be represented by the heightfield alone; the interior used to be a straight chain of 9 m rooms along +z.

## Decisions

### 1. Winding trail

- Waypoints: from the trailhead the route visits up to four waypoints chosen on rings around the summit at rising altitude (about 25, 45, 65 and 80 percent of the height range), alternating left and right of the direct line, each on dry ground with a gentle slope, then the summit. This makes the trail circle the mountain through forest, meadows and alpine terrain.
- Legs are routed with A* on the full 6 m grid with 16 neighbours. Grades above 0.22 cost heavily and above 0.35 are only used when nothing else works; a smooth noise field adds meanders; water and leaving the playable disc are expensive.
- The route is smoothed (Chaikin), resampled every 2 m, and its height profile is smoothed over about 40 m. Grid points within 8 m of the path are set to the profile height (blending back to the original terrain over the next 8 m), and any remaining point that would push the terrain above the path surface is lowered, so the path is flat across and never buried.
- The path is drawn as a flat 2.4 m wide strip at the profile height with lighter dirt, edge stones, painted red-white-red trail posts every 40 m, cairns every 80 m and a signpost at the trailhead. Trees and boulders keep 5 m away, ground cover 1.5 m.

### 2. Stairs that land on the ground

At summon time the house walks outward from the porch edge and finds where a 0.7 grade stair line meets the terrain; the stairs end there, and the last step is at most 0.25 m above the ground. If no landing is found within 16 m, the site gets a new direction or is rejected. The stilts variant faces away from the water so the stairs lead to dry land. The walkable stair surface follows the same run.

### 3. Grid of rooms

- Interior space is an infinite grid of 18 m cells; cell (i, j) holds one room centred in the cell. The cabin is cell (0, 0). Room sizes come from per-type ranges (width/length 5–13 m, own height), varied per cell by hash.
- Each room has exactly one closed side; the closed sides pair neighbouring rooms like dominoes. The plane is tiled with 2x2 blocks (offset so that the cabin and the room south of it share a block), each holding two horizontal or two vertical dominoes chosen by hash, and the cabin's block is forced vertical so that the cabin's closed side is south, where the front door is. All other sides have doorways connected by straight corridors to the neighbours, so every room has doors on at least three sides.
- Room types: slot = (a*i + b*j) mod k (a, b chosen so neighbours always differ) through a per-house permutation of the script-defined types.
- Rooms are built in a local frame whose +x side is the closed side, so script layouts stay the same for any orientation; items can be placed by metres from the centre or by fractions of the room size and against walls.
- The closed side holds the big window, or for rooms marked `balcony` a doorway to a 2 m deep balcony with a railing. The balcony is enclosed by five portal quads (front, sides, top, bottom) that show the room's viewpoint from 6 m above the ground; the room's outer wall is drawn on the balcony side.
- The game keeps the current room, its four neighbours and their corridors built. "Back to the door" puts the player 1.5 m inside the front door of the cabin, facing it.

## Risks / Trade-offs

- [Up to five window portals plus a balcony view cost GPU time] → Portals are frustum and facing culled; only openings in view are rendered.
- [Flattening a path on a 6 m grid widens the visible bench] → The bench blends back to the terrain; the low resolution hides the edges.
