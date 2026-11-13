## ADDED Requirements

### Requirement: Independent source build
The repository SHALL build the Windows runtime from its own source tree and documented installed developer dependencies without private laboratory files or prebuilt laboratory objects.

#### Scenario: Fresh developer checkout
- **WHEN** a developer configures a clean build with the documented UCRT64 toolchain and SDL3
- **THEN** all CPU handlers and runtime objects are generated or compiled from the exported sources
- **AND** a path containing spaces is supported.

### Requirement: Read-only user ROM input
The runner SHALL verify a user-supplied EAC archive and pass assembled regions in memory without changing that archive.

#### Scenario: Missing or mismatched set
- **WHEN** required members are missing or their hashes do not match
- **THEN** launch fails with a useful error before executing the game.

### Requirement: Auditable publication
The export SHALL retain dependency provenance and licences, exclude private coordination and game payloads, and distinguish proven behaviour from remaining acceptance gates.

#### Scenario: Review before publication
- **WHEN** the source export is prepared for commit review
- **THEN** a file inventory, build/test evidence and known limits are available
- **AND** no commit or push occurs without its own explicit approval.
