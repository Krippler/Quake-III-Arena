Native 64-bit Linux build of the Quake III Arena 1.32b GPL source, ported to SDL2.

## Install
1. Install SDL2 and an OpenGL driver. On CachyOS / Arch: `sudo pacman -S --needed sdl2 mesa`
2. Download and extract `quake3-linux-x86_64.tar.gz`.
3. Copy `pak0.pk3` ... `pak8.pk3` from your copy of Quake III Arena into its `baseq3/` folder (or `~/.q3a/baseq3`).
4. Run `./quake3.x86_64`.

The game data is not included.

## Highlights
- Runs natively on X11 and Wayland, with PulseAudio / PipeWire sound.
- Starts fullscreen at your desktop resolution.
- Wide screens show more at the sides instead of a zoomed-in view (`r_widescreenFov 0` for the original behavior).
- The System Setup menu adds 16:9, 16:10 and ultrawide modes up to 5120x2880, plus "Desktop" and "Custom".
- Console: `r_mode -2` sets the desktop resolution, `modelist` lists all modes, `vid_restart` applies changes, and Alt+Enter toggles fullscreen.
- Includes the `q3ded.x86_64` dedicated server and native 64-bit game modules.

See `README.txt` in the archive for details.
