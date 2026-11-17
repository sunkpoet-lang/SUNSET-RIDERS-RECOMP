# Build and run Sunset Riders on Windows

This guide builds the **local EAC runtime from source**. It does not build a finished four-player online release. ROMs are supplied separately at launch; they are not compiled into the executable.

## What you need

| Requirement | Why it is needed |
| --- | --- |
| Windows and MSYS2 UCRT64 | The platform/toolchain currently accepted by this source export |
| UCRT64 GCC with C++20 support | Compiles the host, CPU cores and device support |
| CMake 3.24+ and Ninja | Configures and builds the project |
| UCRT64 SDL3 development files | Headers, import library and runtime for window/audio/controllers |
| Native Windows Python 3.10+ | Generates CPU handler sources, verifies ROMs and runs the launcher/test tools |
| Your own matching ROM archive | Supplies the original program, graphics, sound and initial EEPROM data |

Use the **UCRT64 terminal** for the following commands. Mixing UCRT64 and another MSYS2 toolchain can select incompatible libraries. The recorded export was tested with GCC 16.2.0, SDL3 3.4.18 and Python 3.10.11; those are observed versions, not a claim that every other version has been tested. Linux, macOS and MSVC builds are not supported by the current CMake configuration.

The required reference source subset is already present under `third_party/`. You do not need to download the entire MAME application, install FBNeo or run a network server for local play. GekkoNet is a separate research dependency, not a dependency of this exported CMake build.

## Get the dependencies and clone

Install [MSYS2](https://www.msys2.org/) and open its **UCRT64** terminal. After the normal MSYS2 update procedure, the development packages are:

```sh
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-zlib
```

The first configure downloads GekkoNet (online play's rollback library) at a pinned release, checked against its SHA-256; it needs an Internet connection once.

Also install native Windows Python 3.10+ and make it available in this terminal, or use its explicit path in the configure and runner commands. These installation commands are a setup recipe; the reproduction check below used the already installed versions recorded in its receipt, not a freshly installed MSYS2 machine. MSYS2 packages advance over time. See the official [environment guide](https://www.msys2.org/docs/environments/) and [SDL3 package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-sdl3).

```sh
git clone https://github.com/sunkpoet-lang/SUNSET-RIDERS-RECOMP.git
cd SUNSET-RIDERS-RECOMP
git rev-parse HEAD
```

The clone contains the CPU/chip support sources and generators needed for this build. There are no required local laboratory folders, prebuilt object archives or private submodules. Keep your own ROM archive outside the source checkout.

## 1. Configure

From the repository root:

```sh
cmake -S . -B build-clean -G Ninja -DCMAKE_BUILD_TYPE=Release
```

If CMake chooses the wrong Python, pass your native Windows interpreter explicitly:

```sh
cmake -S . -B build-clean -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE="C:/path/to/python.exe"
```

If SDL3 is not found, check that its development package belongs to UCRT64 and that you are using the UCRT64 CMake/compiler. Changing an existing build folder between toolchains is unreliable; configure a new folder instead.

## 2. Compile and check

```sh
cmake --build build-clean --parallel 2
ctest --test-dir build-clean --output-on-failure
```

CMake runs the bundled 68000 and Z80 source generators, compiles the CPU/device libraries and links `build-clean/Sunset-Riders-Lab.exe`, the online window `build-clean/Sunset-Riders-Online.exe` and the headless `sunset-seat-peer.exe` the online checks use. It also copies the title BMP and SDL3 runtime beside the executables. Assertions remain enabled in Release builds.

The nine configured tests cover the menu, top-bar model, assists, turbo, menu input, session profile, core self-test, embedded icon and ROM loader. They use synthetic fixtures where appropriate and do not prove that a real game was completed.

Keep the UCRT64 runtime DLLs available through that terminal's PATH. Copying only the EXE to another PC does not make a portable package; a distributable runtime/DLL/licence bundle still needs its own clean-machine checks.

## 3. Supply the correct game set

The loader requires **Sunset Riders — 4 Players ver EAC**, defined in [tools/set-eac.json](../tools/set-eac.json). The ZIP must contain the required EAC program, shared graphics/sound and `ssriders_eac.nv` members with the specified checksums. A two-player set, another revision or an incomplete clone archive is not interchangeable.

Do not rename chips to make a mismatched archive pass. Missing or duplicate members are a set-identification problem. The loader reads the archive, verifies it, assembles regions in memory and checks that the original ZIP stayed unchanged. No separate BIOS file is requested by this loader; all required members are listed in its manifest.

## 4. Open the game

```sh
python tools/run.py --rom "C:/my-roms/ssriders.zip" \
  --exe "build-clean/Sunset-Riders-Lab.exe"
```

The runner defaults to windowed play. Use the runner rather than double-clicking this research executable: it validates and supplies the ROM regions through standard input. The executable does not yet provide a standalone ROM-folder browser.

For controls and menu shortcuts, see the [README](../README.md#playing-locally). F5/F8 use one temporary in-memory slot, lost on exit. Offline turbo and lives/powerup assists are present; persistent external state loading and finished online play are not supplied here.

## Optional game-derived regression

With your own matching set:

```sh
python tools/check_game.py --rom "C:/my-roms/ssriders.zip" \
  --exe "build-clean/Sunset-Riders-Lab.exe" --output "build-clean/game-check"
```

Choose an output directory that does not already exist. This opens a diagnostic SDL window for about 27 seconds and checks the accepted four-player frame, CPU/chip/audio counters, temporary save/load and resized menu interaction. Its captures are private, game-derived test output: keep them out of Git and release archives.

Historical export results are in [source-export.json](evidence/source-export.json) and [session-r51.json](evidence/session-r51.json). They identify the tested revision; updating documentation does not rerun or expand those tests. See [development status](development-status.md) before assuming later laboratory work is included in this source tree.

## Reproduction check, 14 November 2026

The published commit `5c1d600487bbcbc9e2680678ffb247c7505a6531` was cloned over HTTPS into a new directory. A fresh CMake/Ninja build, including a build path containing a space, compiled all runtime objects and generated CPU handlers from those sources. It reused no laboratory build objects.

The resulting executable passed all nine CTest tests, then the 1500-frame user-ROM regression, including the accepted reference frame and ROM-unchanged check. The source worktree remained clean. The [metadata-only receipt](evidence/reproducibility-20261114.json) records the exact revision and toolchain; private logs/captures and game data are not bundled.

The source manifest retains its original export hashes and adds `git_sha256` for the actual Git blob bytes. Git normalises text to LF; 48 original Windows export hashes therefore differ after cloning only by line endings. Those differences were checked byte-for-byte. Use `git_sha256` to verify a clone and preserve the older fields as provenance.

This is a functional source-build reproduction on one Windows development machine. It does not demonstrate a second person's PC, a fresh dependency installation, a portable binary package or bit-for-bit identical executables. The receipt identifies the tested code revision; documentation-only changes do not constitute a new runtime test.
