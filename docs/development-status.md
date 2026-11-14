# Development handover — 14 November 2026

Implementation is paused for maintainer handover. This note separates what can be built here, what was observed in the laboratory and what still needs integration. No new online release is being announced.

## The source tree in this repository

The standalone export is the r51 Windows interpreted-CPU runtime. Its [source manifest](source-manifest.json), [build validation](evidence/source-export.json) and [extended-session receipt](evidence/session-r51.json) identify that export. Later multiplayer work has not been copied into it simply because the documentation now describes that work.

Local features include four input slots, SDL window/audio/controllers, top bar and overlay, volume, temporary own-session save/load, offline turbo and lives/powerup assists. There is no finished four-player online adapter in this exported build. Its native executable still interprets the 68000 and Z80.

## Human gameplay report

The tester reported reaching the final-boss encounter, stable sound and gameplay, normal deaths and continues, a physical controller on P1 and keyboard use on P3/P4 between scenes. The tester also confirmed the menu, turbo and cheats in gameplay. On 14 November 2026 a further manual report confirmed that all four physical controllers work with cheats enabled. The executable fingerprint and controller models were not supplied for that report; it is recorded as human acceptance, not an automated device test.

This report belongs to the locally tested laboratory build. The later report supersedes the earlier pending local four-controller acceptance. It does not demonstrate final victory, Internet play, cycle accuracy or acceptance of every rebuilt export. The earlier r51 automated eight-minute soak remains a different piece of evidence.

## Online belongs to the roadmap

The current source build is a local game. Four-player online, mid-game joining/leaving and online replay integration are planned work, described in [the roadmap](roadmap.md). Separate laboratory diagnostics do not make those features available here. No online completion percentage or release date is claimed.

Before a future release, maintainers must integrate full-machine checkpoint/rollback handling, protect online settings, preserve sound and replay continuity, then validate complete games on separate PCs. Offline cheats being confirmed locally does not prove that their online restrictions have been tested.

## Keep the evidence honest

- Local loopback, four processes and four real PCs are different tests.
- A source export, a laboratory executable and a distributed package are different artefacts.
- Reported gameplay, repeatable automated checks and independently measured hardware accuracy are different claims.
- Do not copy game-state payloads, ROM data, private coordination records or credentials into this repository.
- Preserve upstream notices and the independent presentation of this project. [Credits](../CREDITS.md) and [third-party notices](../THIRD_PARTY_NOTICES.md) explain the code and research lineage.

## Fresh-clone reproducibility

On 14 November 2026 the published source revision was cloned over HTTPS and rebuilt without laboratory objects. All nine ROM-free tests and the 1500-frame matching-EAC regression passed; the original ZIP and tracked source stayed unchanged. See [the receipt](evidence/reproducibility-20261114.json). This build check does not resume online implementation or turn separate laboratory research into an exported feature.
