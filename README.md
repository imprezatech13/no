# ROBLOX 2013 for New 3DS (fan tribute)

Software-rendered, single-player 2013-style ROBLOX sandbox. Top screen = game, bottom screen = Game Menu.

| Input | Action |
|---|---|
| Circle Pad | Move |
| C-Stick | Camera |
| A | Jump |
| B (twice) | Reset character (first press shows the confirm prompt on the bottom screen) |
| L / R | Zoom camera in / out |
| START | Pause (tap "Resume Game") |
| Touch | Game Menu: Resume, Reset, Settings, Help, Screenshot, Leave Game |

Screenshots go to `sdmc:/roblox2013/shot_NNN.bmp`.

## Getting the .cia (no toolchain needed on your PC)
1. Make a free GitHub repo and upload everything in this folder (keep `.github/workflows/build.yml`).
2. Open the **Actions** tab -> "Build ROBLOX 2013 for 3DS" -> wait ~3-5 min.
3. Download the `roblox2013-3ds` artifact: `roblox2013.cia` (install with FBI) and `roblox2013.3dsx` (Homebrew Launcher, `sd:/3ds/`).
   If the CIA step fails, the .3dsx is still produced.

## Building locally (Docker)
    docker run --rm -v "$PWD":/work -w /work devkitpro/devkitarm make        # .3dsx
For `make cia` you additionally need `makerom` and `bannertool` on PATH.

Requires libctru 2.x (current devkitpro/devkitarm image): uses `hidCstickRead`.
