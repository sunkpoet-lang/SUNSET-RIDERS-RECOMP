# Development handover — 14 November 2026

Implementation is paused for maintainer handover. This note separates what can be built here, what was observed in the laboratory and what still needs integration. No new online release is being announced.

## The source tree in this repository

The standalone export is the r51 Windows interpreted-CPU runtime. Its [source manifest](source-manifest.json), [build validation](evidence/source-export.json) and [extended-session receipt](evidence/session-r51.json) identify that export. Later multiplayer work has not been copied into it simply because the documentation now describes that work.

Local features include four input slots, SDL window/audio/controllers, top bar and overlay, volume, temporary own-session save/load, offline turbo and lives/powerup assists. Since 16 November 2026 the source also builds an experimental online window (next section). The native executables still interpret the 68000 and Z80.

## Human gameplay report

The tester reported reaching the final-boss encounter, stable sound and gameplay, normal deaths and continues, a physical controller on P1 and keyboard use on P3/P4 between scenes. The tester also confirmed the menu, turbo and cheats in gameplay. On 14 November 2026 a further manual report confirmed that all four physical controllers work with cheats enabled. The executable fingerprint and controller models were not supplied for that report; it is recorded as human acceptance, not an automated device test.

This report belongs to the locally tested laboratory build. The later report supersedes the earlier pending local four-controller acceptance. It does not demonstrate final victory, Internet play, cycle accuracy or acceptance of every rebuilt export. The earlier r51 automated eight-minute soak remains a different ## Online in the source (16 November 2026)

`Sunset-Riders-Online.exe` plays with rollback for up to four players on up to four PCs (GekkoNet; the seat layer in `src/net`): a host-star room, one or two players per PC, players joining and leaving during the match (a joining PC rebuilds the state from its own start and the confirmed inputs; every PC checks it), a host left alone waiting, every player's name under that player's score, and the menu's online mode (no states, cheats, turbo or pause). Usage is in the [README](../README.md#playing-online-experimental).

To make rollback fit in a frame, the main CPU now runs in slices up to the board's next event instead of one step at a time, and reads of the program ROM and work RAM no longer sync the board. On the four-player regression this gives the same game, picture and sound as stepping; `SUNSET_LOCKSTEP=1` keeps the stepped loop, which still matches `tests/native-reference.json` exactly. Two diagnostic counters of that reference changed with the new loop: the main CPU's greatest lookahead (435 to 0 ticks) and the sound CPU's greatest lead (27 to 24). Rollback's own state (about 65 KB: CPU, board, sound and clock) saves and loads in well under a millisecond.

Checked on one PC with `tools/online_check.py` (3600 frames with and without rollbacks equal to stepping; four headless PCs joining and leaving with every epoch's checkpoints equal; hidden online windows with four seats) and by the tester with four windows and real controllers on one PC (16 November 2026). Not yet checked: a match across the Internet between separate PCs, long online sessions, replays of online matches and spectators.

 have been tested.

## Keep the evidence honest

- Local loopback, four processes and four real PCs are different tests.
- A source export, a laboratory executable and a distributed package are different artefacts.
- Reported gameplay, repeatable automated checks and independently measured hardware accuracy are different claims.
- Do not copy game-state payloads, ROM data, private coordination records or credentials into this repository.
- Preserve upstream notices and the independent presentation of this project. [Credits](../CREDITS.md) and [third-party notices](../THIRD_PARTY_NOTICES.md) explain the code and research lineage.

## Fresh-clone reproducibility

On 14 November 2026 the published source revision was cloned over HTTPS and rebuilt without laboratory objects. All nine ROM-free tests and the 1500-frame matching-EAC regression passed; the original ZIP and tracked source stayed unchanged. See [the receipt](evidence/reproducibility-20261114.json). This build check does not resume online implementation or turn separate laboratory research into an exported feature.
