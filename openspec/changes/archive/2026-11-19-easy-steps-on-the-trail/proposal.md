# Proposal

## Why

Playtesting showed that it was easy to get stuck on the way up: the player could not step onto the path even where its edge was only a few centimetres high, and on the path walking stopped at bends and at the tips of switchbacks. The cause: any small ledge counted as a slope too steep to climb, the height of the path jumped at bends and where switchback legs lie side by side, and the coarse terrain sometimes poked a little above the path with the steep normal of the cut beside it.

## What Changes

- The walker steps up ledges up to 0.6 m at once and climbs ledges up to 1.2 m after pushing into them for a quarter of a second; the view follows such steps smoothly.
- The walkable path is a little wider than it looks (1.4 m to each side), has no steps at bends, and keeps the walker on it where it is built up (a drop of more than a step); on the path the coarse terrain never counts as a slope.
- The tips of switchbacks are level landings; the route keeps away from its own earlier legs, so it rarely crosses itself.
- Cairns stand clear of the walkable edge; trees and boulders keep 5 m away from the path.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `exploration`: the slope limit (automatic steps and climbing low ledges).
- `world-generation`: the marked hiking trail (landings, no crossings, never stuck).

## Impact

- Engine core: `player.c` (step and climb), `terrain.c` (route, landings, surface lookup), `game.c` (ground and path edges), `flora.c` (clearance).
- Tests: player steps and climbing, the trail surface, and a hiker bot that follows the whole trail without getting stuck.
