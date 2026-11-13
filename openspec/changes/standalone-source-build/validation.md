# Standalone source export validation

13 November 2026: clean Windows UCRT64 GCC 16.2.0 / SDL3 3.4.18 / Python 3.10.11 / CMake-Ninja Release build passed in a build directory containing a space. All CPU handlers and objects came from exported source; no laboratory .o/.a were used. The pinned arcade-decomp reference subset was compared byte-for-byte to the pristine pinned dependency. Locally adapted host shims and the five relative video includes are fingerprinted separately.

Nine CTest targets passed, including 13 synthetic ROM loader cases and seven embedded icon sizes. The paced SDL regression passed at 1,500 native frames: exact own-session state/future audio restore, identical native frame SHA-256 and canonical CPU/chip/audio counters versus r51, zero audio queue resets, mouse bar/resize checks. User ROM ZIP unchanged. Evidence: docs/evidence/source-export.json, docs/source-manifest.json. Private capture payloads remain only in the ignored build directory.

Initial export missed the Z80 daisy header; added before the passing build. GCC reports optimisation warnings in unchanged ymfm; these remain documented, not suppressed. No online/full-game/twelve-hour validation claimed. No commit, push, deployment or beta replacement performed. SSH authentication was previously rejected; resolve before publishing. Commit and push remain separate explicit approvals.
