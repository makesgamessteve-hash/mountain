#!/usr/bin/env bash
# Mountain - the game must fail cleanly (exit code 2, no crash) when its
# shaders or textures are broken or missing; mods/ overrides are used to
# inject the broken files. Usage: failures.sh <mountain executable>
set -u
EXE="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
ASSETS="$(dirname "$EXE")/assets"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
export SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
run() { xvfb-run -a -s "-screen 0 640x360x24" "$EXE" --assets "$1/" --test /dev/null --data-dir "$TMP/data" --frames 10 > "$TMP/log" 2>&1; echo $?; }
fail=0
expect() { local name="$1" dir="$2"; local code; code=$(run "$dir"); if [ "$code" = "2" ]; then echo "ok   $name"; else echo "FAIL $name (exit $code)"; cat "$TMP/log"; fail=1; fi; }
# 1. a fragment shader that does not compile
mkdir -p "$TMP/a/mods/shaders" && ln -s "$ASSETS" "$TMP/a/assets" && echo "this is not glsl" > "$TMP/a/mods/shaders/sky.frag"
expect "broken shader" "$TMP/a"
# 2. a vertex shader that compiles but cannot be linked (it has no main())
mkdir -p "$TMP/b/mods/shaders" && ln -s "$ASSETS" "$TMP/b/assets"
printf 'uniform vec3 u_clash;\nvec4 helper(){ return vec4(u_clash, 1.0); }\n' > "$TMP/b/mods/shaders/post.vert"
expect "unlinkable program" "$TMP/b"
# 3. a missing shader file
mkdir -p "$TMP/c/assets" && ln -s "$ASSETS/textures" "$TMP/c/assets/textures"
expect "missing shaders" "$TMP/c"
# 4. a corrupt texture
mkdir -p "$TMP/d/mods/textures" && ln -s "$ASSETS" "$TMP/d/assets" && echo "not a png" > "$TMP/d/mods/textures/terrain.png"
expect "corrupt texture" "$TMP/d"
# 5. a missing texture
mkdir -p "$TMP/e/assets" && ln -s "$ASSETS/shaders" "$TMP/e/assets/shaders"
expect "missing textures" "$TMP/e"
exit $fail
