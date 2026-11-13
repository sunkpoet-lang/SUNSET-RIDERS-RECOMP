# Design

Export from menu-r51, compiling every object from sources. Generate CPU opcode handlers from upstream descriptions at build time; never export game disassembly or translated ROM bytes. Isolate main CPU, sound, PCM and video host include paths so the different emu.h shims cannot collide. Replace private absolute includes with vendored relative includes. Use SDL3 installed by the developer and Windows UCRT64 GCC, CMake, Ninja and Python.

Pin arcade-decomp 9e8455630091ba71fe0130aaf1312139fa2837fc, preserve BSD notices for MAME/ymfm and GPL-3.0-only notices for recomp-common. Record exported file hashes and local modifications. Include full applicable licences; approved game images remain separately credited game artwork, not GPL source assets.

ROM input is explicit, verified against the EAC manifest, assembled only in RAM and piped to the native process. No downloading, writing ROMs or implicit personal directory defaults. Build output and temporary test files are excluded from publication. Source-level tests run without ROMs; actual game regression requires a separately supplied set. Validate icon resources and compare the existing native frame/state/audio checks. Keep long-session/full-game/physical P2-P4 and real online gates open.
