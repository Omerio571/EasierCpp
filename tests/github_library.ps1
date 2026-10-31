
param(
    [Parameter(Mandatory = $true)]
    [string]$Script
)

$ErrorActionPreference = "Stop"


# =============================================================================
# Fixture configuration
# =============================================================================

# Every test run receives its own disposable directory.
# Keep this directory separate from user projects and downloaded libraries.

$fixtureDirectory = Join-Path `
    $env:TEMP `
    ("CppLiveLab GitHub fixtures " + [Guid]::NewGuid().ToString("N"))

[IO.Directory]::CreateDirectory($fixtureDirectory) | Out-Null

$repositoryUrl = "https://github.com/example/library"

$global:CppLiveLabApiFixture = @{
    Status          = 0
    NetworkFailure  = $false
    LastCommit      = ""
    Downloads       = 0
    DownloadFailure = $false
    Archive         = ""
    Requests        = 0
    Timeout         = 0
    Slow            = $false
    TimedOut        = $false
}


# =============================================================================
# Controlled API exception
# =============================================================================

# Simulates an HTTP failure with a StatusCode property.
# The production workflow uses that property to distinguish errors such as
# missing releases, rate limiting, and unavailable repositories.

class FixtureApiException : Exception {
    [object]$Response

    FixtureApiException([int]$status) : base("Controlled API response") {
        $this.Response = [pscustomobject]@{
            StatusCode = $status
        }
    }
}


# =============================================================================
# Mock GitHub API requests
# =============================================================================

# Shadows Invoke-RestMethod only inside this test's PowerShell execution
# context. No actual GitHub API requests should be performed.

function Invoke-RestMethod {
    param(
        $Uri,
        $Headers,
        $TimeoutSec
    )

    $fixtureState = $global:CppLiveLabApiFixture

    $fixtureState.Requests++
    $fixtureState.Timeout = $TimeoutSec

    if ($fixtureState.Slow) {
        Start-Sleep -Milliseconds 100
    }

    if ($fixtureState.TimedOut) {
        throw "Controlled request timeout"
    }

    if ($fixtureState.Status) {
        throw [FixtureApiException]::new($fixtureState.Status)
    }

    if ($fixtureState.NetworkFailure) {
        throw "Controlled disconnected network"
    }

    # Return published releases, including draft and prerelease cases.
    if ($Uri -match "/releases\?") {
        # Preserve Invoke-RestMethod's Windows PowerShell array behavior.
        Write-Output -NoEnumerate @(
            [pscustomobject]@{
                tag_name   = "v4.0-draft"
                draft      = $true
                prerelease = $false
            }

            [pscustomobject]@{
                tag_name   = "v3.0-beta"
                draft      = $false
                prerelease = $true
            }

            [pscustomobject]@{
                tag_name   = "v2.0"
                draft      = $false
                prerelease = $false
            }

            [pscustomobject]@{
                tag_name   = "v1.0"
                draft      = $false
                prerelease = $false
            }
        )

        return
    }

    # Provide a tag older than the available published releases.
    if ($Uri -match "/tags\?") {
        Write-Output -NoEnumerate @(
            [pscustomobject]@{
                name = "v0.5"
            }
        )

        return
    }

    # Resolve a requested version into a deterministic commit identifier.
    if ($Uri -match "/commits/(.*)$") {
        $fixtureState.LastCommit = $matches[1]

        return [pscustomobject]@{
            sha = ("a" * 40)
        }
    }

    throw "Unexpected fixture request: $Uri"
}


# =============================================================================
# Mock archive downloads
# =============================================================================

# Copies a local fixture archive instead of contacting GitHub.

function Invoke-WebRequest {
    param(
        [switch]$UseBasicParsing,
        $Uri,
        $OutFile,
        $TimeoutSec
    )

    $fixtureState = $global:CppLiveLabApiFixture
    $fixtureState.Downloads++

    if ($fixtureState.DownloadFailure) {
        throw "Controlled download failure"
    }

    [IO.File]::Copy(
        $fixtureState.Archive,
        $OutFile,
        $true
    )
}


# =============================================================================
# Test assertions
# =============================================================================

function Require {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}


# Verify that an operation is rejected with the expected error message.

function Refuses {
    param(
        [scriptblock]$Action,
        [string]$Pattern
    )

    $errorMessage = ""

    try {
        & $Action
    }
    catch {
        $errorMessage = $_.Exception.Message
    }

    Require `
        ($errorMessage -match $Pattern) `
        "Expected '$Pattern', got '$errorMessage'"
}


# =============================================================================
# CMake executable discovery
# =============================================================================

# Prefer an existing PATH entry, but support Visual Studio's bundled CMake.
# This avoids requiring users to permanently modify their environment.

function Find-CMakeExecutable {
    $cmakeCommand = Get-Command `
        cmake.exe `
        -ErrorAction SilentlyContinue

    if ($null -ne $cmakeCommand) {
        return $cmakeCommand.Source
    }

    $programFilesDirectory = ${env:ProgramFiles}

    if (-not $programFilesDirectory) {
        throw "Program Files directory could not be located."
    }

    $visualStudioRoot = Join-Path `
        $programFilesDirectory `
        "Microsoft Visual Studio"

    foreach ($visualStudioVersion in @("2022", "18")) {
        foreach ($edition in @(
            "Community",
            "Professional",
            "Enterprise",
            "BuildTools"
        )) {
            $cmakePath = Join-Path `
                $visualStudioRoot `
                "$visualStudioVersion\$edition\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

            if (Test-Path -LiteralPath $cmakePath -PathType Leaf) {
                return $cmakePath
            }
        }
    }

    throw "CMake was not found on PATH or in a supported Visual Studio installation."
}


# =============================================================================
# Execute offline integration tests
# =============================================================================

try {

    # =========================================================================
    # 1. Published releases and tags
    # =========================================================================

    & $Script `
        -Action Discover `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory

    $versionsFile = Join-Path $fixtureDirectory "versions.txt"
    $versions = [IO.File]::ReadAllLines($versionsFile)

    Require (
        $versions[0] -eq "Latest Stable" -and
        $versions -contains "v0.5" -and
        $versions -contains "v3.0-beta"
    ) "Actual release/tag alternatives missing"

    Require (
        $versions -notcontains "v4.0-draft"
    ) "Draft release was offered as a published version"


    # =========================================================================
    # 2. Cached metadata reuse
    # =========================================================================

    # Cached discovery must not make new API requests.

    $requestCount = $global:CppLiveLabApiFixture.Requests
    $global:CppLiveLabApiFixture.Status = 403

    $cachedClock = [Diagnostics.Stopwatch]::StartNew()

    & $Script `
        -Action Discover `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory

    $cachedClock.Stop()

    Write-Host (
        "Controlled cached metadata check: " +
        "$($cachedClock.ElapsedMilliseconds) ms"
    )

    Require (
        $global:CppLiveLabApiFixture.Requests -eq $requestCount
    ) "Repeated discovery unnecessarily requested remote metadata"

    $global:CppLiveLabApiFixture.Status = 0


    # =========================================================================
    # 3. Bounded network timeout
    # =========================================================================

    $slowOutputDirectory = Join-Path $fixtureDirectory "slow"

    $global:CppLiveLabApiFixture.Slow = $true

    & $Script `
        -Action Discover `
        -Repository $repositoryUrl `
        -OutputDirectory $slowOutputDirectory

    Require (
        $global:CppLiveLabApiFixture.Timeout -ge 1 -and
        $global:CppLiveLabApiFixture.Timeout -le 5
    ) "Quick request did not use a bounded timeout"

    $global:CppLiveLabApiFixture.Slow = $false


    # =========================================================================
    # 4. Failed discovery must not publish metadata
    # =========================================================================

    $timeoutDirectory = Join-Path $fixtureDirectory "timeout"

    $global:CppLiveLabApiFixture.TimedOut = $true

    Refuses {
        & $Script `
            -Action Discover `
            -Repository $repositoryUrl `
            -OutputDirectory $timeoutDirectory
    } "timeout"

    Require (
        -not (Test-Path (Join-Path $timeoutDirectory "versions.txt"))
    ) "Failed quick check published usable metadata"

    $global:CppLiveLabApiFixture.TimedOut = $false


    # =========================================================================
    # 5. API rate limiting
    # =========================================================================

    $global:CppLiveLabApiFixture.Status = 429

    Refuses {
        & $Script `
            -Action Discover `
            -Repository $repositoryUrl `
            -OutputDirectory (Join-Path $fixtureDirectory "rate-limit")
    } "existence could not be checked"

    $global:CppLiveLabApiFixture.Status = 0


    # =========================================================================
    # 6. Resolve the latest stable release
    # =========================================================================

    & $Script `
        -Action Resolve `
        -Repository $repositoryUrl `
        -Version "Latest Stable" `
        -OutputDirectory $fixtureDirectory

    Require (
        $global:CppLiveLabApiFixture.LastCommit -eq "v2.0"
    ) "Latest Stable incorrectly selected prerelease"

    $resolvedFile = Join-Path $fixtureDirectory "resolved.txt"
    $resolvedVersion = [IO.File]::ReadAllLines($resolvedFile)

    Require (
        $resolvedVersion[1] -eq ("a" * 40) -and
        $resolvedVersion[2] -eq "v2.0"
    ) "Exact reproducible version was not saved"


    # =========================================================================
    # 7. Resolve an older explicit tag
    # =========================================================================

    & $Script `
        -Action Resolve `
        -Repository $repositoryUrl `
        -Version "v0.5" `
        -OutputDirectory $fixtureDirectory

    Require (
        $global:CppLiveLabApiFixture.LastCommit -eq "v0.5"
    ) "Older explicit tag was replaced"


    # =========================================================================
    # 8. Reject credential-bearing repository URLs
    # =========================================================================

    Refuses {
        & $Script `
            -Action Discover `
            -Repository "https://github.com/user/repo?token=private" `
            -OutputDirectory $fixtureDirectory
    } "without credentials"


    # =========================================================================
    # 9. Rate-limited and missing-version resolution
    # =========================================================================

    foreach ($statusCode in @(403, 429)) {
        $global:CppLiveLabApiFixture.Status = $statusCode

        Refuses {
            & $Script `
                -Action Resolve `
                -Repository $repositoryUrl `
                -Version "v9" `
                -OutputDirectory $fixtureDirectory
        } "existence could not be checked"
    }

    $global:CppLiveLabApiFixture.Status = 404

    Refuses {
        & $Script `
            -Action Resolve `
            -Repository $repositoryUrl `
            -Version "v9" `
            -OutputDirectory $fixtureDirectory
    } "Version 'v9'"

    $global:CppLiveLabApiFixture.Status = 0


    # =========================================================================
    # 10. Disconnected network
    # =========================================================================

    $global:CppLiveLabApiFixture.NetworkFailure = $true

    Refuses {
        & $Script `
            -Action Resolve `
            -Repository $repositoryUrl `
            -Version "v9" `
            -OutputDirectory $fixtureDirectory
    } "network/API failure"

    $global:CppLiveLabApiFixture.NetworkFailure = $false


    # =========================================================================
    # 11. Prepare a pinned source archive
    # =========================================================================

    Add-Type -AssemblyName System.IO.Compression.FileSystem

    $archiveSourceDirectory = Join-Path `
        $fixtureDirectory `
        "archive-content"

    $revisionDirectory = Join-Path `
        $archiveSourceDirectory `
        "fixture-revision"

    [IO.Directory]::CreateDirectory($revisionDirectory) | Out-Null

    [IO.File]::WriteAllText(
        (Join-Path $revisionDirectory "header.hpp"),
        "// pinned fixture"
    )

    $fixtureArchive = Join-Path $fixtureDirectory "fixture.zip"

    [IO.Compression.ZipFile]::CreateFromDirectory(
        $archiveSourceDirectory,
        $fixtureArchive
    )

    $global:CppLiveLabApiFixture.Archive = $fixtureArchive


    # =========================================================================
    # 12. Download exact pinned source
    # =========================================================================

    & $Script `
        -Action Resolve `
        -Repository $repositoryUrl `
        -Version "v1.0" `
        -OutputDirectory $fixtureDirectory

    & $Script `
        -Action Download `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory

    $downloadManifest = Join-Path $fixtureDirectory "downloaded.txt"
    $usableSource = [IO.File]::ReadAllLines($downloadManifest)

    $sourceDirectoryName = Split-Path -Leaf $usableSource[0]
    $sourceParentDirectory = Split-Path -Parent $usableSource[0]

    Require (
        $sourceDirectoryName -match "^s[0-9a-f]{8}$" -and
        $sourceParentDirectory -eq $fixtureDirectory
    ) "Extracted source root was not compact and contained in its cache"


    # =========================================================================
    # 13. Reuse an unchanged downloaded source
    # =========================================================================

    & $Script `
        -Action Download `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory

    Require (
        $global:CppLiveLabApiFixture.Downloads -eq 1
    ) "Unchanged pin was downloaded unnecessarily"


    # =========================================================================
    # 14. Compact a legacy cache
    # =========================================================================

    $legacySourceDirectory = Join-Path `
        $fixtureDirectory `
        "legacy-github-full-commit-source"

    [IO.Directory]::CreateDirectory($legacySourceDirectory) | Out-Null

    $legacyHeader = Join-Path $legacySourceDirectory "header.hpp"

    [IO.File]::WriteAllText(
        $legacyHeader,
        "// pinned fixture"
    )

    [IO.File]::WriteAllLines(
        $downloadManifest,
        [string[]]@(
            $legacySourceDirectory,
            $usableSource[1]
        )
    )

    & $Script `
        -Action Download `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory `
        -ExpectedArchiveHash $usableSource[1]

    $usableSource = [IO.File]::ReadAllLines($downloadManifest)
    $sourceDirectoryName = Split-Path -Leaf $usableSource[0]

    Require (
        $global:CppLiveLabApiFixture.Downloads -eq 1 -and
        $sourceDirectoryName -match "^s[0-9a-f]{8}$" -and
        [IO.File]::ReadAllText($legacyHeader) -eq "// pinned fixture"
    ) "Legacy cache compaction downloaded again or altered the previous source tree"


    # =========================================================================
    # 15. Repair corrupted extracted files
    # =========================================================================

    $extractedHeader = Join-Path $usableSource[0] "header.hpp"

    [IO.File]::WriteAllText(
        $extractedHeader,
        "// corrupted source"
    )

    & $Script `
        -Action Download `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory `
        -Repair `
        -ExpectedArchiveHash $usableSource[1]

    $usableSource = [IO.File]::ReadAllLines($downloadManifest)
    $extractedHeader = Join-Path $usableSource[0] "header.hpp"

    Require (
        $global:CppLiveLabApiFixture.Downloads -eq 1 -and
        [IO.File]::ReadAllText($extractedHeader) -eq "// pinned fixture"
    ) "Repair did not re-extract the verified archive without redownloading"


    # =========================================================================
    # 16. Reject an unexpected archive hash
    # =========================================================================

    $sourceArchive = Join-Path $fixtureDirectory "source.zip"

    $originalArchiveHash = (
        Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256
    ).Hash

    Refuses {
        & $Script `
            -Action Download `
            -Repository $repositoryUrl `
            -OutputDirectory $fixtureDirectory `
            -ExpectedArchiveHash ("0" * 64)
    } "Pinned archive hash changed"

    $currentArchiveHash = (
        Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256
    ).Hash

    Require (
        $currentArchiveHash -eq $originalArchiveHash
    ) "Hash mismatch replaced the previous usable archive"


    # =========================================================================
    # 17. Failed downloads must preserve valid cached source
    # =========================================================================

    $global:CppLiveLabApiFixture.DownloadFailure = $true

    $downloadIdentityFile = Join-Path `
        $fixtureDirectory `
        "download-identity.txt"

    [IO.File]::WriteAllText(
        $downloadIdentityFile,
        "different identity"
    )

    Refuses {
        & $Script `
            -Action Download `
            -Repository $repositoryUrl `
            -OutputDirectory $fixtureDirectory
    } "Controlled download failure"

    Require (
        [IO.File]::ReadAllText($extractedHeader) -eq "// pinned fixture"
    ) "Failed download deleted the previous usable source"

    $currentManifest = [IO.File]::ReadAllLines($downloadManifest)

    Require (
        $currentManifest[0] -eq $usableSource[0]
    ) "Failed download published a replacement manifest"

    $global:CppLiveLabApiFixture.DownloadFailure = $false


    # =========================================================================
    # 18. Prepare an archive containing a long Windows path
    # =========================================================================

    $longPathArchive = Join-Path $fixtureDirectory "long-path.zip"

    $longDirectoryA = "a" * 100
    $longDirectoryB = "b" * 100

    $longArchiveEntry = (
        "fixture-revision/include/" +
        $longDirectoryA + "/" +
        $longDirectoryB + "/value.hpp"
    )

    $zipArchive = [IO.Compression.ZipFile]::Open(
        $longPathArchive,
        [IO.Compression.ZipArchiveMode]::Create
    )

    try {
        $zipEntry = $zipArchive.CreateEntry($longArchiveEntry)

        $writer = New-Object IO.StreamWriter($zipEntry.Open())

        try {
            $writer.Write("// long path fixture")
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $zipArchive.Dispose()
    }

    $global:CppLiveLabApiFixture.Archive = $longPathArchive


    # =========================================================================
    # 19. Extract and verify the long archive path with CMake
    # =========================================================================

    # This replaces the old PATH-only lookup that caused the test failure.
    # Discover the bundled Visual Studio CMake when necessary.

    $cmakeExecutable = Find-CMakeExecutable

    & $Script `
        -Action Download `
        -Repository $repositoryUrl `
        -OutputDirectory $fixtureDirectory `
        -CMakeExecutable $cmakeExecutable

    $longSourceDirectory = (
        [IO.File]::ReadAllLines($downloadManifest)
    )[0]

    $expectedHeader = Join-Path $fixtureDirectory "expected.hpp"

    [IO.File]::WriteAllText(
        $expectedHeader,
        "// long path fixture"
    )

    $actualHeader = Join-Path `
        $longSourceDirectory `
        ("include\" + $longDirectoryA + "\" + $longDirectoryB + "\value.hpp")

    & $cmakeExecutable `
        -E compare_files `
        $expectedHeader `
        $actualHeader

    Require (
        $LASTEXITCODE -eq 0
    ) "Discovered CMake did not extract the real long archive path correctly"


    # =========================================================================
    # 20. Report success
    # =========================================================================

    Write-Host (
        "GitHub offline fixtures passed: " +
        "releases, tags, prerelease exclusion, exact pin, older release, " +
        "URL validation, missing version, rate limit, disconnected network."
    )
}
finally {

    # =========================================================================
    # Safe fixture cleanup
    # =========================================================================

    # The test is only allowed to remove the uniquely named directory it
    # created directly beneath the Windows temporary directory.

    $resolvedFixtureDirectory = [IO.Path]::GetFullPath(
        $fixtureDirectory
    )

    $temporaryDirectory = [IO.Path]::GetFullPath(
        $env:TEMP
    ).TrimEnd("\")

    $fixtureParent = [IO.Path]::GetDirectoryName(
        $resolvedFixtureDirectory
    )

    $fixtureName = [IO.Path]::GetFileName(
        $resolvedFixtureDirectory
    )

    if (
        $fixtureParent -ne $temporaryDirectory -or
        $fixtureName -notlike "CppLiveLab GitHub fixtures *"
    ) {
        throw "Fixture cleanup escaped its exclusively created temporary directory."
    }

    # Use the extended Windows path prefix for safe long-path cleanup.
    Remove-Item `
        -LiteralPath ("\\?\" + $resolvedFixtureDirectory) `
        -Recurse `
        -Force

    Remove-Variable `
        -Name CppLiveLabApiFixture `
        -Scope Global
}
