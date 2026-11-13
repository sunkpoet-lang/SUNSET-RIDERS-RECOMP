# Third-party sources and artwork

- CPU/chip support and host framework: [cheviman/arcade-decomp](https://github.com/cheviman/arcade-decomp), pinned revision `9e8455630091ba71fe0130aaf1312139fa2837fc`, Sebastian Toyos, BSD-3-Clause. Only the required reference support subset is vendored. Host shims are locally adapted for Sunset Riders checkpoint ownership; they are not unchanged upstream files.
- MAME CPU, video and sound device code: per-file BSD-3-Clause notices retained, including Nicola Salmoria, David Haywood, Ernesto Corvi and Alex W. Jackson. See `third_party/MAME-LICENSE` and individual files; the inherited notice also describes upstream components not used here.
- YM2151: ymfm, Aaron Giles, BSD-3-Clause, `third_party/YMFM-LICENSE`.
- Shared menu: Tommy McLeroy's recomp-common, with ForkAde modifications, GPL-3.0-only. Exact exported files are fingerprinted in `docs/source-manifest.json`; all SPDX and author notices are retained.
- SDL3: zlib licence, `third_party/SDL3-LICENSE`; supplied by the developer's installation.

The combined host project is distributed under GPL-3.0-only (`LICENSE`); retained third-party files keep their own notices and terms. No licence is granted here to Konami game assets. Approved demonstration PNGs, title-derived ICO and BMP are presentation artwork derived from an identified local game run, credited to Konami; they are not relicensed as GPL code. See `docs/evidence/screenshots.json`. No game ROM or state payload is included.
