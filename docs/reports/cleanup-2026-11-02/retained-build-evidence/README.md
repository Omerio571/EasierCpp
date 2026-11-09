# Historical build evidence

Architectural reports and distinct screenshots remain tracked. Identical screenshots
are represented once. SCREENSHOT_ALIASES.csv records each locally retained duplicate,
its tracked representative, and the SHA-256 proving equivalence.

Raw CTest logs, the inherited-environment dump and duplicate screenshots are no
longer tracked. Existing local copies are preserved and ignored. They are optional
historical debugging evidence, not active test fixtures. The migration changed-file
lists record historical names and are not instructions to recreate those artifacts.

Current regression logs belong under build/. Follow tests/README.md for the active
suite and docs/release/RELEASE_POLICY.md for public-package acceptance.
