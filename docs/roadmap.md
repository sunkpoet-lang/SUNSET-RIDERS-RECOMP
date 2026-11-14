# Roadmap

The current releasable source builds a local Windows runtime. Online features below are planned work, not options available in this build. There is no committed delivery date.

## Four-player rollback online

- [ ] Integrate the rollback adapter with all four input lanes and the full board, sound and video checkpoint state.
- [ ] Support one or two local players per PC, host-assigned seats and host relay.
- [ ] Allow a new player to join an available seat during a match; restore a verified common checkpoint before continuing.
- [ ] Keep the remaining players running when a guest leaves; reject packets from old sessions.
- [ ] Disable offline cheats, turbo and state-changing menu actions in protected online sessions, and test the restriction.
- [ ] Preserve confirmed replay inputs and sound through session changes.
- [ ] Define spectator capacity and behaviour before exposing spectator controls.
- [ ] Validate complete credits on separate PCs, controller input, disconnect/reconnect, sound continuity and replay playback.

GekkoNet is the rollback foundation being investigated. Separate local experiments inform this work, but they are not evidence of a finished Internet mode in this source tree.

## Distribution and compatibility

- [ ] Validate a portable Windows package on a clean PC, including runtime DLLs and licence notices.
- [ ] Extend controller and long-session coverage beyond the reported local setup.
- [ ] Keep build/source manifests, ROM identification and dependency notices reproducible as the implementation changes.

Full static recompilation is also a longer-term research goal. The current 68000 and Z80 cores remain interpreted. Local build and validation instructions are in [the build guide](build.md).
