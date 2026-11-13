# Building the experimental Windows runtime

This is an interpreted-CPU research runtime, not a complete static recompilation or a network-ready release. No ROMs, BIOS files or prebuilt game objects are included.

Install MSYS2 UCRT64 GCC, CMake, Ninja, SDL3 and Python 3.10 or newer. From the **UCRT64 terminal**, with Python available:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python tools/run.py --rom /path/to/ssriders.zip
```

Use a native Windows Python interpreter; pass its path with `-DPython3_EXECUTABLE=...` if discovery selects the wrong one. Quote paths containing spaces. The first export is tested with Windows UCRT64 GCC; other toolchains are unverified. Keep UCRT64 runtime DLLs on PATH when testing this developer build. This is not yet a redistributable installer.

The runner requires the exact **4 Players ver EAC** members/checksums in `tools/set-eac.json`. It validates before execution and assembles regions only in RAM. The ZIP is never modified. All original game data must be supplied separately. ROM-free tests use synthetic fixtures only.

P1: arrows, Z/X, coin 1. P2: WASD, F/G, coin 2. P3: IJKL, U/O, coin 3. P4: keypad 4/6/8/5, keypad 1/3, coin 4. Esc opens the shared menu; F10 opens the top bar; F5/F8 use one temporary own-session slot; F11 fullscreen. Cheats and turbo are offline only. The provisional twelve-hour arithmetic profile is not a twelve-hour gameplay validation. Full-game and four-player online acceptance remain pending.

Optional regression using your own matching set (opens one SDL window for about 27 seconds):

```sh
python tools/check_game.py --rom /path/to/ssriders.zip --exe build/Sunset-Riders-Lab.exe --output build/game-check
```

The output directory must not exist yet. It contains private game-derived captures: keep it out of Git and release packages. This checks the accepted four-player frame, native CPU/chip/audio counters, one save/load and resized mouse/menu handling. It does not prove full-game completion or network compatibility.
