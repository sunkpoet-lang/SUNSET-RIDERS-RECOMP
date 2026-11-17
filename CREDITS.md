# Credits and research references

Sunset Riders was created by **Konami**. Its game program, graphics, music and names remain the property of their respective rights holders. This independent PC research project does not include the ROMs or claim ownership of the original game.

The port is maintained by **sunkpoet-lang**, with community gameplay feedback. Its board integration, loader, PC controls, local menu features and verification work depend on substantial upstream contributions. Please retain those authors and licence notices when changing or distributing the code.

## Code used by the local build

**MAME and its contributors.** The CPU and Konami device support, and the board behaviour used as a reference, come from MAME-derived sources. The bundled 68000 core credits Olivier Galibert; the Z80 core credits Juergen Buchmueller. Nicola Salmoria is credited for the board-driver reference and adapted video/protection behaviour. The K052109 source credits Fabio Priuli, Acho A. Tang and R. Belmont; the K053260 source credits Ernesto Corvi and Alex W. Jackson. Other authors, including David Haywood, are named in the relevant retained files. This list is a guide, not a replacement for the full per-file notices.

- [MAME project](https://github.com/mamedev/mame)
- [Konami board driver reference, `tmnt2.cpp`](https://github.com/mamedev/mame/blob/master/src/mame/konami/tmnt2.cpp)
- [Exact EAC member definitions and recorded MAME revision](tools/set-eac.json)

**Sebastian Toyos and arcade-decomp contributors.** The exported source includes a required support subset from [arcade-decomp](https://github.com/cheviman/arcade-decomp), recorded at revision `9e8455630091ba71fe0130aaf1312139fa2837fc`. Its host/device integration provided a starting point for the local adaptations. The root notice is BSD-3-Clause; the bundled components retain their individual authorship and terms.

**Aaron Giles and ymfm contributors.** [ymfm](https://github.com/aaronsgiles/ymfm) supplies YM2151 FM synthesis. Its BSD-3-Clause notice is retained. It is one part of sound emulation; the Z80 sound program, sample device, timing and output mixing are separate responsibilities.

**SDL contributors.** [SDL3](https://github.com/libsdl-org/SDL) provides the window, audio device and controller APIs. The developer installs its development/runtime package; its zlib licence is included among the third-party notices.

**Tommy McLeroy and recomp-common contributors.** The [shared menu](https://github.com/tmcleroy/recomp-common) provides the top bar and overlay model used with local adaptations. The inspected exported sources use GPL-3.0-only. This reuse does not imply that Tommy or the upstream project endorses this port.

## Research references and separate experiments

[tmntarcade-decomp](https://github.com/cheviman/tmntarcade-decomp), by Sebastian Toyos and contributors, informed the investigation of related Konami hardware and tool organisation. A working TMNT project does not automatically provide Sunset Riders' board map, timing or protection logic. The actual exported subset and adaptations are identified in the local source manifest.

[The Sunset Riders cheat discussion](https://www.mamecheat.co.uk/forums/viewtopic.php?t=12846) was a research reference for offline assists. No forum script is bundled, and reading a public post is not treated as a licence to redistribute its code.

**Jamie Meyer and GekkoNet contributors.** [GekkoNet](https://github.com/HeatXD/GekkoNet) provides the rollback sessions of online play. CMake fetches its library at a pinned release; its BSD-2-Clause notice is in `third_party/GEKKONET-LICENSE`. The seat room, joining and leaving during a match, and the board's rollback state are this project's, on top of it.

## Licences, pins and demonstration artwork

The combined host project uses GPL-3.0-only, while retained third-party files keep their notices and applicable licences. See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the bundled licence texts. Do not replace all upstream notices with the repository's licence or describe imported CPU/device code as newly authored here.

[docs/source-manifest.json](docs/source-manifest.json) records the exact exported files and hashes. Moving upstream links above are for reading; they do not override those pins or authorise an automatic update.

The title screenshot, gameplay image and title-derived executable icon are presentation material from the original game. They remain credited to Konami and are not relicensed as source code. [Screenshot provenance](docs/evidence/screenshots.json) distinguishes captures from scripted diagnostics and real controller acceptance. No ROM or state payload is included with these credits.
