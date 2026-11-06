# EasierCpp release policy

## Build and eligibility

- Use the CMake project version for the binaries, executable metadata, notes, and ZIP
  name. Keep the native caption `EasierCpp` and the approved Windows icon.
- Build the public Windows x64 candidate in a dedicated Release directory with
  `EASIERCPP_PORTABLE=ON` and `BUILD_TESTING=OFF`. Use the Development configuration
  (`EASIERCPP_PORTABLE=OFF`) to build and execute the internal test suites.
- Pass applicable automated regressions and manual GUI acceptance. Resolve known
  release blockers. Preserve user project compatibility and the existing settings,
  library records, and build/cache locations. Keep dependencies to those required.

## Public UI and portability

- Select public capabilities at compile time. Hide source-only commands, including
  the test runner and its cancel action, even when running from a checkout.
- Keep supported project tools available. Verify each visible action, navigation,
  help icons, Light and Dark appearance, branding, resizing, and DPI scaling.
- Extract the ZIP outside the checkout. Test from ordinary PowerShell and launch the
  GUI without administrator rights or a developer terminal. Include all required
  first-party runtime resources and exclude developer-machine paths.
- Document prerequisites, including the Visual C++ x64 Redistributable, installed
  CMake, Ninja, MSVC or another supported compiler, and Windows SDK. Preserve existing
  user data on upgrade. Use a separate disposable profile for migration tests.

## Verification

Run the applicable Development tests and exercise the **extracted public binaries**.
Use disposable CMake projects to verify creation and opening, Build/Run, incremental
Auto Run and recovery, Stop/Restart, console input/output, library management,
project settings, preferred editor, both themes, window restoration, startup and
shutdown, and settings persistence. Check previous-version compatibility. Record
results, test scope, failures, and outstanding manual or clean-machine checks.

## Deliverables and approval

- Produce `EasierCpp-v<version>-win-x64.zip` and its SHA-256 checksum. Keep the
  portable build, staging, and published artifacts separate from Development builds.
- Include version-matched notes from `notes/v<version>.md`, a short installation
  and upgrade guide, the MIT license, and applicable third-party notices. Exclude
  source trees, test fixtures, build caches, logs, and internal reports.
- Document new features, fixes, compatibility, and known limitations in plain
  language. Keep the verification report with the candidate for review.
- Local preparation and public publication require separate approval. Present the
  candidate and remaining checks to the owner before committing, pushing, tagging,
  uploading artifacts, or creating a GitHub release.
