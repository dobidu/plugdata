---
phase: 01-foundation
plan: 01
type: execute
autonomous: true
---

<objective>
Fork plugdata, set up pd-repente build infrastructure, render empty
PromptBar component, validate cross-platform build.
</objective>

<context>
@docs/00_Arquitetura_Consolidada.md
@docs/01_CommandInput_Estudo.md
@projects/pd-repente/PLANNING.md
@apps/pd-repente/.paul/PROJECT.md
</context>

<acceptance_criteria>
## AC-1: Fork builds on three OSes
Given a fresh clone of the pd-repente repo
When CMake configure + build runs on Windows, macOS, and Linux
Then plugdata.app/dll/so produces no errors on all three platforms

## AC-2: PromptBar component renders
Given plugdata is running
When user opens any canvas
Then PromptBar is visible at the bottom of the canvas window,
     accepts text input, and does not break existing UI layout

## AC-3: Test infrastructure ready
Given the build directory is configured
When `ctest` runs
Then all baseline tests pass (PromptBar instantiable smoke test)
</acceptance_criteria>

<tasks>
<task type="auto">
  <name>Fork plugdata + branch setup</name>
  <files>.git/, CMakeLists.txt</files>
  <action>
    Fork https://github.com/plugdata-team/plugdata on GitHub.
    Clone with submodules:
      git clone --recursive https://github.com/{user}/pd-repente
    Rename default branch to `pd-repente-main`.
    Verify original plugdata builds clean on local machine before touching anything.
  </action>
  <verify>git status clean, cmake + build produces plugdata binary with no errors</verify>
  <done>AC-1 partial (local build confirmed)</done>
</task>

<task type="auto">
  <name>Create Source/RepentePd structure</name>
  <files>
    Source/RepentePd/Core/.gitkeep
    Source/RepentePd/Bridge/.gitkeep
    Source/RepentePd/Commands/.gitkeep
    Source/RepentePd/UI/.gitkeep
    Source/RepentePd/Integration/.gitkeep
  </files>
  <action>
    Create folder skeleton.
    Add Source/RepentePd/ as a source group in CMakeLists.txt
    (empty for now — no source files yet, just structure).
  </action>
  <verify>Directory tree exists, CMakeLists.txt compiles without error</verify>
  <done>Structure ready for Phase 2 work</done>
</task>

<task type="auto">
  <name>PromptBar.h/cpp minimal component</name>
  <files>
    Source/RepentePd/UI/PromptBar.h
    Source/RepentePd/UI/PromptBar.cpp
  </files>
  <action>
    JUCE Component subclass with:
    - TextEditor child (single-line, full width)
    - No logic — render only
    Wire into PluginEditor::resized() so it appears at bottom of canvas,
    fixed height ~32px, does not overlap existing canvas area.
    Add to CMakeLists.txt sources.
  </action>
  <verify>Open plugdata, see PromptBar at bottom, type text, no crash, no layout breakage</verify>
  <done>AC-2</done>
</task>

<task type="auto">
  <name>Cross-platform CI matrix</name>
  <files>.github/workflows/ci.yml</files>
  <action>
    GitHub Actions matrix: ubuntu-latest, macos-latest, windows-latest.
    Steps: checkout (with submodules), CMake configure, CMake build.
    Build only — no tests yet (tests wired in next task).
    Cache: cmake build dir via actions/cache.
  </action>
  <verify>All three matrix cells green on push to pd-repente-main</verify>
  <done>AC-1 full</done>
</task>

<task type="auto">
  <name>Baseline JUCE UnitTest + ctest wiring</name>
  <files>Tests/RepentePdTests.cpp</files>
  <action>
    One JUCE UnitTest: instantiate PromptBar, assert non-null.
    Wire into CMakeLists.txt as a test target.
    Add ctest integration so `ctest --test-dir build` runs it.
    Update CI yml to run ctest after build.
  </action>
  <verify>ctest runs locally and in CI, test passes on all three OSes</verify>
  <done>AC-3</done>
</task>
</tasks>

<boundaries>
## DO NOT CHANGE
- Source/Pd/*                  (libpd integration — frozen until Phase 2)
- Source/Canvas.cpp            (until Phase 2 needs canvas mutations)
- Source/Sidebar/CommandInput* (parallel system — do not touch)
- Any audio thread code paths  (never)
</boundaries>
