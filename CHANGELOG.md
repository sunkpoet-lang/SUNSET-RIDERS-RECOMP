# Changelog

**Note:** All dates are in Mexico City CST (UTC-6).

## [2026-11-16] - Four-player online play (experimental)

- perf: run the main 68000 in slices up to the board's next event, and read the program ROM and work RAM without a board sync: a frame goes from about 14 to under 7 ms with the same game, picture and sound (SUNSET_LOCKSTEP=1 keeps the stepped loop); two diagnostic counters of the native reference change with it

## [2026-11-14] - Build guide and development handover

- docs: record fresh HTTPS clone build/test evidence, explicit clone/setup instructions and user-confirmed four-controller play with cheats.
- docs: explain ROM loading, runtime components, source building, references and upstream credits.
- docs: distinguish reported local gameplay, exported sources and separate multiplayer research; record the development pause and remaining integration gates.

## [2026-11-13] - Extended session evidence

- docs: present the project independently in introductory prose and the window caption while retaining third-party notices.
- build: export standalone Windows runtime sources, pinned dependency notices, CMake generation and read-only ROM launch with regression checks.
- docs: record the eight-minute local session test, memory observations and provisional runtime profile.

## [2026-11-12] - Initial documentation preview

- docs: document the interpreted laboratory runtime, verified milestones and remaining release gates.
- docs: add two original-game demonstration captures with their diagnostic provenance and upstream credits.
