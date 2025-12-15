# spritechop

Small CLI that slices frames out of a sprite sheet and packs them into an animated GIF. You give it an input image, the frame size, and a list of top-left coordinates to copy; it writes a GIF with a configurable frame delay (default 80 ms).

## Build & Install

- Build locally: `make`
- Optional install (defaults to `/usr/local/bin`): `make install` (use `sudo` if needed)
  - Override paths with `PREFIX=/custom/prefix` or `BINDIR=/custom/bin`
  - Package-friendly installs: `DESTDIR=/tmp/pkgroot make install`
- Remove installed binary: `make uninstall`
- Clean build artifacts: `make clean`

Requirements: a C compiler (e.g., `gcc`) and `make`. All other dependencies ship in `include/`.

## Usage

```
spritechop -i INPUT -o OUTPUT -s WIDTHxHEIGHT [-f DELAY_CS] X1,Y1 [X2,Y2 ...]
```

- `-i` input image (PNG, JPG, etc.)
- `-o` output GIF path
- `-s` frame size, e.g., `80x114`
- `-f` frame delay in centiseconds (default `8` → 80 ms)
- Coordinates are the top-left pixel of each frame inside the source image.

Example:

```
./spritechop -i assets/ninja.png -o ninja.gif -s 80x114 \
  35,24 159,24 278,24 397,24
```

The command above emits a 4-frame `ninja.gif` using 80 ms per frame by default; pass `-f` to change it.
