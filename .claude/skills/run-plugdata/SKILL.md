---
name: run-plugdata
description: Build and launch the plugdata standalone app to manually test changes. Use when asked to run, start, build, relaunch, or screenshot plugdata / pd-repente, or to confirm a change works in the real app rather than in tests.
---

# Build & run plugdata standalone

Builds the standalone app and launches it so a human can drive the UI.

## Build

```bash
cmake --build build --config Release --target plugdata_standalone -- -m > /tmp/pdbuild.log 2>&1
echo "EXIT=$?"
echo "ERRORS=$(grep -cE 'error C[0-9]+|error LNK|fatal error|MSB[0-9]{4}' /tmp/pdbuild.log)"
tail -3 /tmp/pdbuild.log
```

Run it with `run_in_background: true` — a cold build takes ~15–25 min, incremental
~1–3 min. Expect the last line to be:

```
plugdata_standalone.vcxproj -> C:\Git\plugdata\Plugins\Standalone\plugdata.exe
```

**Never pipe `cmake --build` straight into `tail`/`head`.** The pipe swallows
MSBuild's exit code, so a total failure reports `EXIT=0`. Redirect to a log,
check `$?`, then grep — as above.

## Launch

```bash
cd "/c/Git/plugdata/Plugins/Standalone" && ./plugdata.exe
```

Also `run_in_background: true` — it runs until the user closes the window.
Then confirm the window actually came up (a process with an empty
`MainWindowTitle` means it launched but failed to open a window):

```powershell
Start-Sleep -Seconds 5
Get-Process plugdata -ErrorAction SilentlyContinue |
  Select-Object Id, MainWindowTitle, @{n='RAM_MB';e={[math]::Round($_.WorkingSet64/1MB)}}
```

Healthy: one row, `MainWindowTitle = plugdata`, ~200–250 MB.

## Gotchas paid for already

- **Target is `plugdata_standalone`.** `plugdata_standalone_Standalone` exists
  only on iOS, where the app is built via `juce_add_plugin(... FORMATS Standalone)`
  (`CMakeLists.txt:495-515`); desktop goes through `juce_add_gui_app`
  (`CMakeLists.txt:479`) and gets the bare name. Wrong target on this Visual
  Studio tree fails with a bare `MSB1009: project file does not exist`.
  `ls build/*.vcxproj` lists the real target names.
- **The exe is `Plugins/Standalone/plugdata.exe`** — `RUNTIME_OUTPUT_DIRECTORY`
  is redirected there at `CMakeLists.txt:889-897`, so nothing lands in
  `build/plugdata_standalone_artefacts/` except the import lib. `Plugins/` is
  gitignored (`*Plugins`).
- **Exit code 139 on shutdown is normal.** It fires during teardown (Gem plugin
  unload) after the window is already gone. Not a crash, not caused by your
  change. A clean close gives 0; both are fine.
- **Adding files under `Source/RepentePd/`** needs no CMake edit — the
  `CONFIGURE_DEPENDS` glob at `CMakeLists.txt:234` (RepentePd entries on lines
  257-270) picks them up. But the
  re-configure it triggers costs ~3 min before compilation starts.
- **Stale binary after edits**: the build is incremental, so always rebuild
  before relaunching if source changed since the last build.

## Running the RepentePd unit tests

There is no `ctest` target. Tests are compiled into the standalone and fire
~200 ms after the main window opens, printing to the console the app was
launched from.

The checked-in `build/` has `ENABLE_TESTING:BOOL=OFF`, so **the default build
above does not run them**. Enabling it changes compile definitions and forces a
full rebuild — confirm with the user before doing this to their build tree:

```bash
cmake -S . -B build -DENABLE_TESTING=ON
```

To verify a single pure-logic RepentePd component without a full build, compile
it against `juce_core` standalone (seconds, not minutes):

```bash
cl /nologo /std:c++17 /EHsc /MD /permissive- \
   /DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 /DJUCE_STANDALONE_APPLICATION=1 /DNDEBUG \
   /I Libraries/JUCE/modules /I Source \
   harness.cpp Source/RepentePd/<Component>.cpp \
   Libraries/JUCE/modules/juce_core/juce_core.cpp \
   Libraries/JUCE/modules/juce_core/juce_core_CompilationTime.cpp \
   /link shell32.lib ole32.lib
```

`juce_core_CompilationTime.cpp` and the two libs are all required — omitting
them gives unresolved-symbol link errors. Run from a `vcvars64.bat` shell:
`"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"`.

## Driving it

The app is GUI-only; there is no headless or scripted mode. Hand it to the user
to test, with the relevant prompt-bar commands for whatever changed. The
pd-repente prompt bar has input focus on launch. Useful for inspecting LLM
traffic:

```
/config              show LLM settings and all on/off toggles
/config verbose on   record full request + response per turn
/canvas              print the canvas context that would be sent
```

With verbose on, each turn logs a `[#N]` tag — right-click that console line to
open the full request/response popup.
