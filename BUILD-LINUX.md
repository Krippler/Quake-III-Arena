# Building Quake III Arena for modern 64 bit Linux

The engine builds natively for x86_64 (and aarch64) Linux with CMake, SDL2
and OpenGL. GitHub Actions builds a ready to run package on every push. To get it,
download the `quake3-linux-x86_64` artifact from the *Actions* tab, or the
release asset when a `v*` tag is pushed.

## Dependencies

CachyOS / Arch:

    sudo pacman -S --needed base-devel cmake sdl2 mesa

Debian / Ubuntu:

    sudo apt install build-essential cmake libsdl2-dev libgl-dev

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    cmake --install build --prefix ~/games/quake3

This produces `quake3.x86_64`, `q3ded.x86_64` and the native game modules in
`baseq3/`. Copy `pak0.pk3` ... `pak8.pk3` from your copy of the game into
`~/games/quake3/baseq3` (or `~/.q3a/baseq3`) and run `~/games/quake3/quake3.x86_64`.

See [dist/README-linux.txt](dist/README-linux.txt) for resolution settings.

## What changed from the 2005 source release

* SDL2 video, input and audio (`code/sdl/`) replace the X11 / GLX / OSS code,
  so it runs on X11 and Wayland with PulseAudio / PipeWire.
* 64 bit clean: the VM system call interface uses `intptr_t`, the bytecode
  interpreter is used for QVMs (the x86 JIT is 32 bit only), and pointer
  size, alignment and `long` assumptions were fixed throughout.
* New video modes: `r_mode -2` (desktop resolution, the new default) and
  modes 12 to 27 covering 720p up to 5K, including 16:10 and ultrawide, also
  selectable in the System Setup menu.
* Larger default hunk (`com_hunkMegs 128`) for high resolution screenshots.
* Game data is found next to the executable regardless of the current directory.
