# Roadmap

The source builds the local Windows runtime and an experimental online window. Ticked items are in the source and checked as described; the rest is planned work with no committed delivery date.

## Four-player rollback online

- [x] Integrate the rollback adapter with all four input lanes and the board, sound and machine state (rollback's own in-memory state; the video is redrawn, as the game never reads it).
- [x] Make a frame and a save/load fast enough for rollback at 60 frames a second: the main CPU in event-bounded slices (same game, picture and sound as stepping), state saved and loaded in well under a millisecond.
- [x] Support one or two local players per PC, host-assigned seats and host relay.
- [x] Allow a new player to join an available seat during a match: the joining PC rebuilds the state from its own start and the confirmed inputs, and every PC checks it before continuing.
- [x] Keep the remaining players running when a guest leaves; a host left alone waits for players; packets from old sessions are rejected.
- [x] Disable offline cheats, turbo, states and pause in online sessions (the menu's online mode).
- [ ] Preserve confirmed replay inputs and sound through session changes.
- [ ] Define spectator capacity and behaviour before exposing spectator controls.
- [ ] Validate complete credits on separate PCs across the Internet, controller input, disconnect/reconnect, sound continuity and replay playback.

Rollback runs on GekkoNet. The online checks so far ran on one PC (`tools/online_check.py`, and a tester's four windows with real controllers); they are not yet evidence of play across the Internet.

## Distribution and compatibility

- [ ] Validate a portable Windows package on a clean PC, including runtime DLLs and licence notices.
- [ ] Extend controller and long-session coverage beyond the reported local setup.
- [ ] Keep build/source manifests, ROM identification and dependency notices reproducible as the implementation changes.

Full static recompilation is also a longer-term research goal. The current 68000 and Z80 cores remain interpreted. Local build and validation instructions are in [the build guide](build.md).
