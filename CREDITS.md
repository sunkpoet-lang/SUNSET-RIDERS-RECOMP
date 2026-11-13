# Credits and licence review

Preserve original authorship, copyright notices and licence texts when exporting source or distributing binaries. A repository-wide licence must not be used to relicense third-party code or the original game.

| Project | Contribution/reference | Licence handling |
| --- | --- | --- |
| [MAME](https://github.com/mamedev/mame) | Guest CPU/chip implementations and board/set reference | Preserve per-file notices and applicable licence texts; exported source hashes and dependency pins are in docs/source-manifest.json |
| [ymfm](https://github.com/aaronsgiles/ymfm) | Aaron Giles' FM synthesis implementation | Preserve bundled BSD-3-Clause notices |
| [SDL](https://github.com/libsdl-org/SDL) | SDL3 window, audio and controller facilities | Preserve zlib licence and distribution notices |
| [recomp-common](https://github.com/tmcleroy/recomp-common) | Tommy McLeroy's shared menu; local adaptations | Inspected menu sources use GPL-3.0-only; preserve notices and corresponding-source obligations |
| [tmntarcade-decomp](https://github.com/cheviman/tmntarcade-decomp) | Toolchain/reference research | Root BSD-3-Clause notice credits Sebastian Toyos; vendored components retain their own terms |
| [arcade-decomp](https://github.com/cheviman/arcade-decomp) | Shared analysis/tool infrastructure | Root BSD-3-Clause notice credits Sebastian Toyos; inspect component terms separately |

The cheat forum is a research reference, not licensed imported code. No forum script is included. Game names and demonstration artwork remain the property of their rights holders. Screenshot inclusion does not license redistribution of ROMs or extraction of assets.

The standalone source export includes the host runtime, required pinned reference sources and licence texts. See THIRD_PARTY_NOTICES.md, docs/source-manifest.json and docs/build.md. Developer build validation does not establish a portable binary release or full-game compatibility.
