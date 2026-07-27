---
phase: 07-providers-ci
plan: 03
completed: 2026-05-20
reconstructed: true
---

# Phase 07 Plan 03: CI Unit-Test Automation Summary

**RepentePd unit tests now run headless on every push to the Linux CI job.**

> ⚠️ Retro-documented during UNIFY on 2026-07-26. No PLAN.md exists — this work
> shipped off-roadmap after Milestone 3 closed. Merged to `develop` via PR #3.

## What Was Built

| File | Change |
|------|--------|
| `.github/workflows/pd-repente-ci.yml` | Adds `libglu1-mesa-dev` + `xvfb`; drops `libvdpau-dev`; builds only `plugdata_standalone`; new step runs the binary under `xvfb-run` with a 180s `timeout` |
| `CMakeLists.txt` | `ENABLE_TESTING` now emits `=1`/`=0` instead of `=ON`/`=OFF` |
| `Source/Standalone/PlugDataApp.h` | `initialise()` short-circuits on `PLUGDATA_CI_TESTS_ONLY=1`: runs the `RepentePd` category, exits with the failure count |
| `Tests/Tests.cpp` | Same CI gate for the editor-driven path; accumulates failures; LF line endings (was CRLF) |
| `Tests/RepentePdTests.h` | Anthropic system-shape assertions updated to the `cache_control` array |

## AC Result

| Criterion | Status | Evidence |
|-----------|--------|----------|
| RepentePd tests run on every push | Pass | `pd-repente CI` run `26187559226` — linux/macos/windows all green |
| Test failures fail the job | Pass | Process exits `1` when `failures > 0` |
| No hang in a headless container | Pass | `StandalonePluginHolder` bypassed; `timeout --signal=KILL 180` guards |
| No regression in the full build matrix | Pass | `CMake` workflow fails the identical 3 jobs before and after (`Arch-x64`, `Arch-aarch64`, `windows-32-build`) |

## Deviations

Three commits were required to get the gate working, each fixing a separate cause:

1. `21488f29e` wired the test step, but `ENABLE_TESTING=ON` expanded to the bare
   identifier `ON`, which the C preprocessor evaluates as `0` — every
   `#if ENABLE_TESTING` block silently compiled out. Fixed in `14a544cee`.
2. The gate was first placed in `Tests.cpp`, which only runs once a `PluginEditor`
   exists. Headless CI never gets there. `690ee2d7f` moved it up into
   `PlugDataApp::initialise`, before audio-device and settings-file init.
3. `libvdpau-dev` on the runner made bundled ffmpeg compile `hwcontext_vdpau.c`,
   whose `-lvdpau` the ffmpeg CMake never adds — link failure. `4f1a3ee24` drops
   the package rather than patching ffmpeg.

## Coverage Limits

Linux only. The macOS and Windows jobs build but do not execute tests. GUI
fuzz tests (`ObjectFuzzTest`, `HelpfileFuzzTest`, `HelpfileErrorTest`) are
skipped in CI mode — only the synchronous `RepentePd` category runs.

## Commits

`e0fbeb997`, `21488f29e`, `4f1a3ee24`, `690ee2d7f`, `14a544cee`, `2bbf96810`
(merged to `develop` as `977cd96e5`, PR #3)

---
*Reconstructed: 2026-07-26*
