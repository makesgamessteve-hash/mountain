# Sounds

All sounds in this directory are synthesised procedurally by
`tools/gen_assets.py` from filtered noise, envelopes, FM sweeps, resonators and
small convolution reverbs. Generation is deterministic: re-running the script
produces byte-identical files. They are licensed **CC BY 4.0**, © Mountain
contributors.

Format for every file:

- WAV, PCM 16-bit signed little-endian
- Mono, 22050 Hz
- Peak level at or below -4 dBFS

One-shots start and end at zero with short fades, so they don't click.

## Loops

Loops are built circularly, so the last sample flows into the first and the
file can be played on repeat with no seam or crossfade at runtime:

- noise is shaped in the frequency domain over exactly the loop length
- all modulators are periodic
- every event's tail (chirps, drops, crackles) wraps around to the start

| File | Length | Description |
|------|--------|-------------|
| `wind_loop.wav`     | 8.0 s | Gentle gusting wind: low body, gust-dependent whistle bands, faint leaf rustle |
| `rain_loop.wav`     | 6.0 s | Steady soft rain: hiss, dense droplet ticks, occasional puddle plinks, distant rumble |
| `birds_loop.wav`    | 8.0 s | Sparse songbirds over very soft air: warble, chaffinch-like trill, "tea-cher" calls, thin "tsee" notes, chips. Light forest reverb |
| `crickets_loop.wav` | 6.0 s | Night crickets: four chirping individuals plus a soft distant chorus |
| `stream_loop.wav`   | 6.0 s | Babbling brook: gurgling bed and hundreds of small rising-pitch bubbles |
| `fire_loop.wav`     | 6.0 s | Crackling fireplace: low roar, soft hiss, clustered crackles and occasional pops |

## One-shots

| File | Length | Description |
|------|--------|-------------|
| `step_grass_1.wav`, `step_grass_2.wav` | 0.26 s | Soft rustle (heel, toe) |
| `step_rock_1.wav`, `step_rock_2.wav`   | 0.22 s | Hard click, short body, scattering grit |
| `step_snow_1.wav`, `step_snow_2.wav`   | 0.30 s | Compressing crunch |
| `step_wood_1.wav`, `step_wood_2.wav`   | 0.25 s | Hollow plank thud |
| `door_open.wav`  | 0.85 s | Latch click, then wooden creak |
| `door_close.wav` | 0.50 s | Soft thud with latch |
| `shutter.wav`    | 0.25 s | Camera shutter, two mechanical clicks |
| `summon.wav`     | 1.50 s | Magical rising shimmer with an ascending pentatonic chime arpeggio |
| `unsummon.wav`   | 1.20 s | Descending shimmer and arpeggio |
| `thunder.wav`    | 3.20 s | Distant rolling thunder |
| `owl.wav`        | 1.90 s | Two-note hoot with night reverb |
| `marmot.wav`     | 1.00 s | Short alpine marmot alarm whistle (0.3 s) with mountain echo tail |
| `ui_click.wav`   | 0.06 s | Soft UI tick |
| `chime.wav`      | 1.60 s | Pleasant UI bell (C6 with a G6 fifth) |

The `_1` and `_2` footstep variants use different seeds and a slight pitch
offset, so they can alternate.
