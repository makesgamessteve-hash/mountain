# Design

## Context

Builds on the archived change `add-mountain-foundation`. The terrain grid has 6 m cells, so a narrow path cannot be represented by the heightfield alone; the interior used to be a straight chain of 9 m rooms along +z.

## Decisions

### 1. Winding trail

- Waypoints: from the trailhead the route visits up to two waypoints on rings around the summit at rising altitude (about 30 and 62 percent of the height range), turning around the mountain from the trailhead's side, each on dry ground with a gentle slope; then the summit. Grade limits make the route zigzag up steep flanks anyway.
- Legs are routed with A* on the full 6 m grid with 16 neighbours. Grades above 0.25 cost heavily and above 0.38 are only used when nothing else works; a smooth noise field adds meanders; water, leaving the playable disc and bare rock are expensive.
- The grid route is straightened where walking straight is fine (string pulling: grade, no deep cut or fill, no water), smoothed (Chaikin, three passes), resampled every 2 m, cleared of small loops, and ends with a straight run onto the summit.
- Height profile: the terrain heights along the path, smoothed over about 40 m, then limited to a 30 percent grade: the largest grade-limited profile below and the smallest above are averaged, and both ends are held at the terrain height when the length allows it, so the summit keeps its height.
- Grid points within 8 m of the path take the profile height (blending back to the terrain by 16 m); grid points that would still put the terrain above the 2.4 m strip are lowered. The 6 m grid cannot represent a narrow bench on a steep slope, so the path is its own walkable surface: the player stands on the path where it is above the terrain, and the renderer draws its sides as dry-stone walls down to the ground (solid from below).
- The path is drawn as a level 2.4 m strip of light gravel (not covered by snow: it is trodden), with edge stones, painted red-white-red posts every 40 m, cairns every 80 m and a signpost at the trailhead. Trees keep 5 m away, bushes 3 m, ground cover stays off the strip.

### 2. Stairs that land on the ground

When a site is evaluated the stairs walk outward from the porch or platform edge until a 0.7 grade stair line meets the terrain; they end there, the last step at most 0.25 m above the ground, which must be dry. Houses with a platform (tree, stilts) try all four sides and prefer the front; the stilts house still looks over the water. A site where no stairs reach dry ground within 16 m is rejected. The walkable stair surface, the stair mesh with stringers and handrail, and the gap in the platform railing follow the chosen side and run.

### 3. Grid of rooms

- Interior space is an infinite grid of 18 m cells; cell (i, j) holds one room centred in the cell. The cabin is cell (0, 0), its front wall at z = 0 where the front door is. Room sizes come from per-type ranges (4 to 13 m, so corridors are at least 5 m long) and per-type ceiling heights, varied per cell by hash.
- Each room has exactly one closed side; the closed sides pair neighbouring rooms like dominoes. The plane is tiled with 2x2 blocks (rows offset so that the cabin and the room south of it share a block), each holding two horizontal or two vertical dominoes chosen by hash; the cabin's block is vertical so that the cabin's closed side is south, where the front door is. All other sides have doorways in the middle of the wall, connected by straight corridors (1.6 m wide, with end caps around the doorways, a runner rug and a lamp) to the neighbours, so every room has doors on three sides. A room builds the corridors to its east and north neighbours.
- Room types: each row of the grid uses a per-house order of the non-cabin types; consecutive rows are shifted by 1..k-1 (hash), so neighbours in both directions always differ.
- Rooms are built in a local frame whose +x side is the closed side, so script layouts work for any orientation; items are placed by metres or fractions of the room size from the centre, or against a wall (along, at, inset), facing into the room. Items in front of a doorway are skipped.
- The closed side holds the big window (a portal to a viewpoint; neighbours use different viewpoints), or for some rooms a 2 m wide doorway to a 2 m deep railed balcony. Around the balcony a box of five portal quads (front, sides, top, bottom) shows the viewpoint lifted 8 m; the world is rendered once through all of them, clipped at the wall plane while the viewer is inside the room and unclipped out on the balcony. The outer face of the wall, with eaves, is drawn around the balcony and a daylight source sits on the balcony.
- The cabin's closed side holds the front door and a window beside it; that window maps through the door's transform, so it shows the real view from the house front, where the exterior has a matching window.
- The game keeps the 13 rooms within two steps of the player's cell built (reusing rooms already built when the player moves), collides with all of them and leaves the house only when crossing the front door plane. Interior lighting uses the nearest eight lamps plus daylight from the current room's window; up to three window or balcony views are rendered per frame, nearest first.
- "Back to the door" puts the player 1.5 m inside the front door of the cabin, facing it: `house.entrance()`, the house key or button inside the house, and the menu.

## Risks / Trade-offs

- [Window and balcony views cost a full world render each] → Portals are frustum and facing culled, at most three are rendered per frame, nearest first; a balcony's five quads share one render.
- [Built-up stretches of path on steep switchbacks] → They read as dry-stone retaining walls; the player can walk on them and they block from below.
- [Flattening a path on a 6 m grid widens the visible bench] → The bench blends back to the terrain; the low resolution hides the edges.
