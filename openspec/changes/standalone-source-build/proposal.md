# Export standalone Sunset Riders sources

## Why
The accepted laboratory executable depends on private absolute paths and old object files. A public repository must contain auditable sources and a build that works independently of that laboratory.

## What Changes
- Export the r51 host, board, menu and test sources using an explicit allowlist.
- Vendor only the required pinned CPU/chip sources, adapted host shims and licence notices.
- Add a Windows CMake build and a Python read-only EAC ROM launcher.
- Preserve the approved title/gameplay evidence, report limitations and validate a clean source build.

## Impact
New standalone repository only; no installed beta, ROM, credentials or laboratory runtime changes. Both guest CPUs remain interpreted; this is not a completed static recompilation. Commit and push require separate fresh approvals.
