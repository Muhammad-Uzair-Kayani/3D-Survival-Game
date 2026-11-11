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

Requires CMake 3.20 or newer and a C++20 compiler. To generate a Visual Studio
2022 solution and build the console client:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The embedded Python policy is optional and requires Python development files
plus a pybind11 CMake package:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_PYTHON_PLUGIN=ON
cmake --build build --config Debug
.\build\Debug\GameRuntime.exe
```

Run `GameRuntime` from the generated build folder; enter `P` to request an
action from `python/policy.py`.
