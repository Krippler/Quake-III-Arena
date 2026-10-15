Quake III Arena - native 64 bit Linux build
===========================================

This is the id Software GPL source release (1.32b) ported to 64 bit Linux
with SDL2, so it runs natively on modern distributions (Arch, CachyOS,
Fedora, Ubuntu, ...) on X11 and Wayland, with PulseAudio / PipeWire sound.

Contents
--------
  quake3.x86_64           the game
  q3ded.x86_64            dedicated server
  baseq3/uix86_64.so      menus, with the modern resolutions
  baseq3/cgamex86_64.so   client game module
  baseq3/qagamex86_64.so  server game module

Game data
---------
The game data is NOT included, you need a copy of Quake III Arena.
Copy pak0.pk3 ... pak8.pk3 into the baseq3 directory next to quake3.x86_64,
or into ~/.q3a/baseq3.

On CachyOS / Arch you need SDL2 and an OpenGL driver, normally already
installed:

  sudo pacman -S --needed sdl2 mesa       (sdl2-compat provides sdl2 too)

Then run:

  ./quake3.x86_64

Resolutions
-----------
By default the game starts fullscreen at your desktop resolution
(r_mode -2).  The System Setup menu lists the original modes plus 16:9,
16:10 and ultrawide modes up to 5120x2880, "Desktop" and "Custom".

From the console (~ key):

  r_mode -2              desktop resolution
  r_mode 18              1920x1080, see "modelist" for all modes
  r_mode -1              custom size, set r_customwidth / r_customheight
  r_fullscreen 0 / 1     windowed / fullscreen
  vid_restart            apply the changes

Alt+Enter toggles fullscreen.  r_swapInterval 1 enables vsync.

The HUD and menus are designed for 4:3 and get stretched on wide screens,
the 3D view uses the full width.  Raise cg_fov to taste (e.g. cg_fov 110).

Native modules
--------------
The menus always load from baseq3/uix86_64.so, in the main menu and in
game, so the new video modes show up.  This also works on pure servers.
Set cl_nativeUI 0 to use the pak's ui.qvm instead.  The client and server
game code still comes from the pak QVMs on pure servers, running in the
bytecode interpreter.
