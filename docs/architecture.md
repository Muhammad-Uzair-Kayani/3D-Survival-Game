# Initial Architecture

## Project boundaries

```text
GameRuntime (console client) ─────> survival_core (C++ static library)
       │
       └── optional PythonPolicyPlugin (pybind11 embedded Python)
                     │
                     └── python/policy.py
```

`survival_core` owns simulation state and exposes a small public C++ API. It
does not read input, print to the console, render graphics, or depend on Python
or PlagC. A client submits an action and receives a plain `GameSnapshot`.

`GameRuntime` links the static library. It handles keyboard input and displays
the returned snapshot. With `BUILD_PYTHON_PLUGIN=ON`, the client also loads the
Python policy in-process through pybind11's embedding API. Python returns an
action name; C++ validates the name and remains responsible for applying the
action to the authoritative simulation.

The game contract and policy observation use plain values. No PlagC entity,
renderer, physics object, or Python type appears in `survival_core`. A later
PlagC adapter can replace the grid world while preserving the public client
boundary.

## Build

Visual Studio 2022 console application:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\Debug\GameRuntime.exe
```

To include the embedded Python policy, install Python development files and a
pybind11 CMake package, then configure with the option enabled:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_PYTHON_PLUGIN=ON
cmake --build build --config Debug
.\build\Debug\GameRuntime.exe
```

Enter `P` at the runtime prompt to ask `python/policy.py` for the next action.
The included policy is a deterministic shortest-path baseline, not a trained
neural policy. It can be replaced with model inference while keeping the
observation/action boundary stable.

## Current slice

The first slice provides a deterministic, turn-stepped grid, collision checks,
a destination, a C++ console client, and an optional in-process Python policy
adapter. Seeded scenarios, multiple agents, survival systems, perception,
social memory, and neuroevolution training are subsequent milestones.
