# Tasks

Task manager: CPU / memory graphs, process list, search, end a process — an application for the Colibry.

Part of the Colibry app suite; this repository builds only this app.
Written in C++17 with Qt 6: one app, one shared library (vendored copy in
`common/`), one build.

## Build

```sh
sudo apt install cmake ninja-build qt6-base-dev libgl1-mesa-dev   # Debian / Ubuntu
cmake -S . -B build -G Ninja -DColibry_WARNINGS_AS_ERRORS=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Install: `cmake --install build --prefix /usr`
or simply run `./install.sh` (builds, tests, then installs).

## Packagers

- Nix: `nix build` (see `flake.nix`), dev shell via `nix develop`.
- Arch Linux: `makepkg -si` (see `PKGBUILD`).

## Layout

- `common/` is the shared Colibry library (settings store, theme). It is a
  vendored copy; changes here must be ported to the other app repositories.
- `src/core/` has no widgets and no direct OS calls except through an
  adapter; everything there is unit-tested.
- `src/` has windows with a public, dialog-free API so tests can drive them.

## License

MIT. See [LICENSE](LICENSE).
