# Customized theme accessibility fix

## Diagnosis

`stage4_console_view` compared Rich Edit's error text against the old literal
`RGB(185,28,28)`. Reported COLORREF `12543` is `0x0030FF`, or RGB(255,48,0): the
customized error color was correctly applied. `dwMask == 4294967295` includes
CFM_COLOR. This failure was an obsolete test expectation, not a rendering failure.

`stage4_appearance` used the correct sRGB relative-luminance formula and 4.5:1
requirement, but its generic error did not identify the failing theme or semantic
color. Four customized colors genuinely failed that requirement. Normal console
text is 11-point Consolas, so no large-text relaxation is appropriate.

## Measured palettes

Contrast uses linearized sRGB luminance and `(lighter + 0.05)/(darker + 0.05)`.
Values shown are rounded to six decimals; tests compare the unrounded ratio to 4.5.

| Theme | Semantic color | Background RGB | Original foreground RGB | Original contrast | Final foreground RGB | Final contrast |
|---|---|---|---|---:|---|---:|
| Dark | Neutral/stdout | (32,36,43) | (235,240,245) | 13.580241 | unchanged | 13.580241 |
| Dark | Information | (32,36,43) | (96,165,250) | 6.124526 | unchanged | 6.124526 |
| Dark | Success | (32,36,43) | (74,222,128) | 8.935890 | unchanged | 8.935890 |
| Dark | Warning | (32,36,43) | (251,191,36) | 9.327585 | unchanged | 9.327585 |
| Dark | Error | (32,36,43) | (255,48,0) | **4.207751** | (255,68,25) | **4.517769** |
| Light | Neutral/stdout | (255,255,255) | (25,35,50) | 15.815573 | unchanged | 15.815573 |
| Light | Information | (255,255,255) | (0,78,255) | 5.942686 | unchanged | 5.942686 |
| Light | Success | (255,255,255) | (81,255,0) | **1.341522** | (43,136,0) | **4.541147** |
| Light | Warning | (255,255,255) | (255,174,0) | **1.857352** | (158,109,0) | **4.524618** |
| Light | Error | (255,255,255) | (255,48,0) | **3.700583** | (228,43,0) | **4.521626** |

Light colors were scaled darker along their customized hues until they met 4.5:1.
Warning's red channel was adjusted one additional step to retain the existing
30-degree separation from error red. Dark error received a small white tint, retaining
its red hue and saturation as far as practical. No background, neutral, information,
or already-readable Dark success/warning color was reverted. No runtime color
correction system was added.

## Test changes

- `tests/theme_test_support.hpp`: shared test-only contrast calculation, readable
  theme/semantic/RGB descriptions, and Rich Edit palette verification.
- `tests/stage4_tests.cpp`: report every palette contrast; collect precise contrast
  failures; verify black/white 21:1 and identical-color 1:1 reference calculations.
  Retain the 4.5:1 minimum, semantic channel-dominance checks and 30-degree pairwise
  hue separation. Identity/separation failures now identify colors and measured values.
- `tests/console_view_tests.cpp`: replace the obsolete literal with the selected
  palette's expected error color. Verify CFM_COLOR, absence of automatic text color,
  and exact palette application for both fresh and recolored retained text, including
  stderr and neutral stdout in both themes.
- Production changes are limited to four color constants/comments in
  `src/appearance.hpp` and `src/appearance.cpp`. No renderer, scheduler, process,
  build, Run, Stop, project or console-input behavior was changed in this fix.

## Verification

The targeted pair passed 2/2 in 1.21 seconds after an actual MSVC/CMake/Ninja build.
The exact requested Quick command passed 23/23 in 3.51 seconds (CTest real time):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
```

The requested Full command passed **30/30 in 71.79 seconds**, and doctor passed.
All original Stage 1–4 regressions remained selected. Both suite times exclude
manager SDK/configure/build setup. Complete logs are retained under ignored
`build/theme-fix-quick-details.log` and `build/theme-fix-full-details.log`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
```

An additional direct appearance invocation printed all contrast measurements but
could not save its isolated test registry key inside the restricted sandbox. Repeating
it with the same permissions as the build/test script passed all 36 appearance checks.
This did not require a production code change or changing the user's saved preferences.

Formatting was applied and validated for 39 maintained C++ files with clang-format
19.1.5; `git diff --check` passed. Numerical checks and
native Rich Edit tests do not replace visual inspection on the user's display or
guarantee distinguishability for every form of color-vision deficiency; severity
labels remain alongside colors.

## Manual check

1. Launch `.\build\debug\cpplivelab-gui.exe` and open your calculator project.
2. Choose Light, then Dark. Build successfully: expect green success, blue system
   messages and neutral program prompts/results.
3. In a disposable project, introduce a missing semicolon and Build: expect readable
   red compiler errors. Restore the semicolon and rebuild. Stop a waiting program
   to inspect orange warning text.
4. Switch themes while those messages remain visible. Expect retained text to recolor
   correctly without losing output or changing the running program.

No Stage 5 work, automatic commit or push is part of this fix.
