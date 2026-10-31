# Review and create your own Git checkpoint

No files were staged, committed or pushed by this maintenance pass. Your existing
`.vscode/tasks.json` edits were preserved. Several approved Stage 2/3 files are still
untracked; include them so the committed project will build on a fresh checkout.

1. Complete [the manual checks](MAINTENANCE_MANUAL_TESTS.md). From the manager folder,
   inspect the current changes:

   ```powershell
   git status
   git diff --stat
   git diff -- .vscode/tasks.json
   ```

   `git diff` shows tracked-file changes; open the untracked files listed by status too.
   [MAINTENANCE_REVIEW.md](MAINTENANCE_REVIEW.md) separates formatting/readability work
   from behavior fixes. The diff against Git also includes approved but uncommitted
   earlier stages, not just this maintenance pass.

2. Once you are satisfied, stage the maintained project files explicitly:

   ```powershell
   git add -- .gitignore .gitattributes .clang-format AGENTS.md CODING_STYLE.md CMakeLists.txt CMakePresets.json README.md TEST_RESULTS.md STAGE2_MANUAL_TESTS.md STAGE3_MANUAL_TESTS.md MAINTENANCE_REVIEW.md MAINTENANCE_MANUAL_TESTS.md GIT_CHECKPOINT.md include src tests scripts
   ```

   Ignored `.vs` data, build outputs and `.pdb` files are excluded, even though `src`
   is selected. This does not stage your unrelated editor task change. If you reviewed
   that change and also want it in the commit, run:

   ```powershell
   git add -- .vscode/tasks.json
   ```

3. Inspect exactly what the commit will contain:

   ```powershell
   git diff --cached --stat
   git diff --cached --check
   git diff --cached
   git diff --cached --name-only -- .vs build out
   ```

   Expected: source/docs/configuration only, no whitespace errors, and no paths from
   the final generated-directory query. Review the diff for anything private before
   committing. Captured environment blocks and machine-local test snapshots belong
   outside the index; the maintenance snapshots are under ignored `build/maintenance`.

4. Create the local commit when you approve the staged changes:

   ```powershell
   git commit -m "Improve CppLiveLab readability, documentation, and Git configuration"
   git status
   ```

   A commit saves a local Git snapshot; it has not uploaded anything yet. If you left
   the editor task change unstaged, status will still list it, which is expected.

5. Upload only when you decide the local commit is ready:

   ```powershell
   git remote
   git branch --show-current
   git push -u origin HEAD
   ```

   This repository currently has an `origin` remote and the checked-out branch is `main`.
   The final command uploads the current branch to that configured remote. If Git asks
   you to sign in, use your normal GitHub authentication flow; never paste credentials
   into source, build logs or this project's documentation. If the remote has newer
   commits, stop and resolve that normally rather than forcing a push.

There is no need to delete `.vs`, close Visual Studio, reset source changes, run
`git clean`, or force-push to complete this checkpoint.
