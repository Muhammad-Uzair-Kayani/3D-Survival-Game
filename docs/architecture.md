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
the returned snapshot. When generated with the Python options, the client also loads the
Python policy in-process through pybind11's embedding API. Python returns an
action name; C++ validates the name and remains responsible for applying the
action to the authoritative simulation.

The game contract and policy observation use plain values. No PlagC entity,
renderer, physics object, or Python type appears in `survival_core`. A later
PlagC adapter can replace the grid world while preserving the public client
boundary.

## Build

Use `GenerateProjects.bat` to generate the normal console build. The optional
Python build needs a full 64-bit Windows CPython installation and pybind11.
Use the same Python installation for pip and the build:

```cmd
py -m pip install pybind11
py -c "import sys, pybind11; print(sys.base_prefix); print(pybind11.get_include())"
```

Pass the two printed paths, in order, to the generator:

```cmd
GenerateProjects.bat vs2026 --python-root="C:\path\to\Python" --pybind-include="C:\path\to\pybind11\include"
```

Open `build\premake\vs2026-python\SurvivalGame.sln` and press **Ctrl+F5**.
Keep the selected Python installation on PATH so Windows can load its DLL.
Enter `P` to ask `python/policy.py` for a move. The included policy uses
deterministic shortest-path search.

## Current slice

The first slice provides a deterministic, turn-stepped grid, collision checks,
a destination, a C++ console client, and an optional in-process Python policy
adapter. Seeded scenarios, multiple agents, survival systems, perception,
social memory, and neuroevolution training are subsequent milestones.
