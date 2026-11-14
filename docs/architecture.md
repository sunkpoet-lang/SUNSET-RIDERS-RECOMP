# How this runtime works

The executable is a host for one identified arcade board and ROM variant. Understanding that distinction helps when adding a feature: a menu change belongs to the PC frontend, while a sound desynchronisation can involve CPU timing, chip state or the audio queue.

## From a ZIP to a running board

1. `tools/run.py` asks `tools/rom_loader.py` to verify the user-supplied ZIP against `tools/set-eac.json`.
2. The loader checks member names, sizes, CRC32 and SHA-1. It rejects missing, ambiguous or corrupt members. A merged archive with duplicate matching basenames may be rejected; renaming a different set to `ssriders.zip` will not make it compatible.
3. The loader interleaves the main program and graphics according to their board bus lanes. It produces six regions: `maincpu`, `eeprom`, `k053245`, `audiocpu`, `k053260` and `k052109`.
4. The runner passes those regions to the executable through standard input. The board starts from those separately supplied bytes. There is no ROM-to-native-code conversion in this loading step.
5. The host coordinates CPU execution, device accesses, interrupts, video edges, input and audio. SDL3 presents the resulting frame and sound on the PC.

## Components and responsibilities

| Component | What it does | Where to start reading |
| --- | --- | --- |
| Main CPU | Interprets the original Motorola 68000 program | `third_party/reference/mame/m68000/`, `src/boundary_cpu_probe.cpp` |
| Sound CPU | Interprets the Z80 sound program and communicates with the board | `third_party/z80/`, `src/z80_core.cpp`, `src/window_sound.cpp` |
| Board bus | Maps memory, inputs, EEPROM, protection and device registers | `src/board_bus.h`, `src/board_state.h` |
| Video | Tile/sprite rendering, palette and priority integration; K052109, K053245 and K053251 support | `src/window_video.cpp`, `src/sprite_k053245.h`, `src/priority_k053251.h` |
| PCM sound | Konami K053260 sample playback and its host integration | `third_party/devices/`, `src/window_pcm.cpp` |
| FM sound | YM2151 synthesis via Aaron Giles' ymfm | `third_party/reference/ymfm/`, sound integration sources |
| Window and frame loop | Runs the board, routes live inputs, coordinates sound/video and owns checkpoints | `src/window_machine.cpp` |
| Shared UI | Top bar, overlay and local menu actions | `src/shared/`, `src/sunset_menu*`, `src/sunset_assists.h`, `src/sunset_turbo.h` |
| Build generation | Generates the CPU handler sources and compiles the host/libraries | `CMakeLists.txt` |

Generated CPU handlers are part of building the interpreter. Their presence is not evidence that the original game's entire control flow has been statically recompiled.

## Checkpoints, saves and rollback

The laboratory keeps the main CPU, board, sound CPU/chips, video and host timing together in an own-process checkpoint. Restoring only the main CPU would leave sound, graphics or scheduling at a different moment. The temporary menu save/load uses an own-session state, not an arbitrary state file downloaded from another player.

A future online implementation would use complete checkpoints to predict inputs, roll back and rerun frames. The exported CMake project does not currently build an online adapter. That work must still be integrated and tested against this game's full state, audio and replay behaviour. See [the roadmap](roadmap.md).

## What belongs to this project

The project integrates the EAC board, read-only ROM loading, host scheduling, device wiring, local controls, menu actions, assists and verification tools. CPU and chip support retains upstream authorship; not every file under `src/` is an entirely independent implementation. Locally adapted files and vendored dependencies must keep their notices.

For the upstream board reference, see MAME's [Konami `tmnt2.cpp` driver](https://github.com/mamedev/mame/blob/master/src/mame/konami/tmnt2.cpp). For the dependency revision and exact exported files, use the local [source manifest](source-manifest.json), not a moving upstream branch. [Credits](../CREDITS.md) explain which projects supplied code and which served as references.
