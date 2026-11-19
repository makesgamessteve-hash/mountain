# Design

## Steps and climbing

The slope limit used to refuse any move whose rise exceeded 1.25 times the horizontal move of the frame, so every ledge higher than about 5 cm was a wall. Now a rise up to the step height (0.6 m) onto walkable ground (normal within the slope limit) is always taken. A rise up to the climb height (1.2 m) is taken after the walker has pushed into it for 0.25 s. Higher ledges and steep slopes block as before; jumping still works. The eye lags behind a sudden step and catches up within a fraction of a second, so steps feel like lifting off rather than teleporting.

## The path as a surface

- The path surface at a point is the height of the nearby centre-line segments, weighted by how close they run (fourth power of the distance to the walkable edge), using only the stretch of path nearest the walker's feet and its neighbours along the path. This makes the surface continuous at bends, and a crossing or passing leg does not count.
- The walkable width is 1.4 m to each side, 0.2 m wider than the drawn strip.
- Where the walker stands on the path and the ground beside it lies more than a step lower, the edge keeps the walker on the path (sliding along it). Raised stretches still block from below unless they are low enough to climb.
- On the path, ground that pokes up to 0.3 m above the path surface counts as the flat path.

## Route

- Landings: stretches of the path that come within 3.1 m of each other within 24 m along the path (the tip of a switchback) are level. The grade limit treats the path inside a landing as having no length, which forces it level, and the rest of the profile still keeps to 30 percent.
- Later legs of the route avoid grid points next to earlier legs (cost x6), so the trail rarely crosses itself.

## Testing

A hiker bot in the unit tests follows the whole trail of a world at a run, steering at the point 4 m ahead, and must never be stuck for two seconds; from beside the path it must get onto it wherever the edge is lower than a climbable ledge.
