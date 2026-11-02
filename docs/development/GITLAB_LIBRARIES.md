# Public GitLab libraries

The existing Library Manager accepts both GitHub and gitlab.com repositories.
No authentication, self-hosted GitLab, or additional package manager is included.

## Manual acceptance

1. Launch `build/compile-workers/EasierCpp-Gui.exe` from the repository.
2. Use a disposable project. Open Manage Libraries, Add, then the existing
   Known / GitHub / GitLab workflow and choose GitHub / GitLab Repository.
3. Enter a trusted public repository using either
   `https://gitlab.com/group/project`, `https://gitlab.com/group/subgroup/project.git`,
   or `gitlab.com/group/project`. Fresh fields must be empty.
4. Retrieve Versions. Published tags/releases appear; discovery does not execute
   downloaded code. A branch, older tag, or exact commit can also be typed in the
   version field. Tree/blob URLs and URL query parameters are not accepted.
5. Validate Version. The resolved full commit is retained. No silent upgrades occur.
   Latest Stable conservatively includes only released numeric version tags for
   GitLab; select another tag explicitly if the repository uses a different scheme.
6. Review the existing third-party execution warning before approving a compatibility
   probe. Choose the actual exported CMake target(s) or header-only include directory.
   A CMakeLists.txt alone never establishes compatibility. Compatible, Incompatible,
   Unable to Verify, and Try Adding Anyway retain their existing meanings and safeguards.
7. Review and approve the existing project-change preview before applying. Build and
   run the disposable consumer. Add the same library again: duplicate protection
   should reject it. Remove/re-add or update through the existing managed workflow.
8. Check a missing repository, bad version, and credential/query URL. Errors should
   retain the form state and leave the selected project unchanged. With no tags,
   enter a known branch or commit rather than Latest Stable.
9. Repeat with a GitHub or local library, then exercise normal Build and Auto Run.

## Implementation and boundaries

- `scripts/github-library.ps1` keeps its historical entry point. It selects provider
  API routes, resolves refs, downloads pinned ZIPs, verifies SHA256, safely extracts,
  and reuses the existing metadata/source caches.
- Existing GitHub records stay `owner/repository`. GitLab records use
  `gitlab.com/group/project`, so caches and duplicate identities distinguish providers.
- `github_dependency_cmake` retains its existing API name and generates the exact
  same provider archive URL used when hashing the download. GitLab catalogs use a
  `gitlab-` ID prefix; existing GitHub IDs and settings are unchanged.
- Compatibility probes, multi-target selection, source caching, approved transactions,
  rollback, and library removal are shared. No Auto Run, PCH, or worker changes.
- Anonymous API requests determine accessibility. HTTP 401/404 cannot reliably
  distinguish a private repository from an absent one; neither is supported.
- Listings are limited to the first 100 releases and tags. Branch/tag selection is
  separate from the repository URL. Discovery has the existing five-second budget;
  service errors/rate limits remain Unable to Verify, never Compatible.
- Project path segments currently accept ASCII letters, digits, `_`, `.`, and `-`.
  There is no bare `group/project` shorthand because it would be ambiguous with GitHub.

API references: [GitLab repositories](https://docs.gitlab.com/api/repositories/),
[commits](https://docs.gitlab.com/api/commits/), and
[releases](https://docs.gitlab.com/api/releases/).

Automated checks use controlled API/archive responses and disposable CMake consumers.
Live third-party library configuration requires separate repository/commit approval.
