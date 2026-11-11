# 3D Survival Game

A survival simulation project with a backend-neutral C++ core, a console
runtime, and an optional embedded Python policy. The current repository starts
with a small playable slice; PlagC rendering and engine integration are later
milestones.

## Architecture

- `survival_core` is a C++20 static library. It owns simulation state and
  exposes a small client-facing API without console, rendering, PlagC, or
  Python dependencies.
- `GameRuntime` is a console executable that links the static library and
  handles keyboard input and display.
- `PythonPolicyPlugin` is an optional client-side adapter using pybind11's
  embedded Python support. `GameRuntime` can ask a Python policy for an action;
  C++ remains authoritative for validating and applying it.
- The gameplay contract is designed to remain stable when the current world
  implementation is later connected to PlagC.

See [docs/architecture.md](docs/architecture.md) for build commands and the
current integration boundary.

## Current slice

The console client controls a deterministic grid simulation and receives a
snapshot containing the actor, destination, tick, seed, and action result. When
the Python plugin is enabled, pressing `P` asks a Python shortest-path baseline
for the next action. The core has no rendering, input handling, or Python
dependency.

## Build

Put `premake5.exe` from a [Premake release](https://github.com/premake/premake-core/releases)
that supports `vs2026` in the project root or on PATH. Run:

```cmd
GenerateProjects.bat
```

Open `build\premake\vs2026\SurvivalGame.sln` and press **Ctrl+F5**.
The executable is `build\premake\vs2026\bin\Debug\GameRuntime.exe`.
For Visual Studio 2022, run `GenerateProjects.bat vs2022` and open the
solution under `build\premake\vs2022`.

For the optional Python policy setup, see [the architecture guide](docs/architecture.md#build).
