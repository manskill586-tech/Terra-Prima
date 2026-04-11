# Genesis Engine Alpha (Windows)

This is the alpha pipeline: **SIM Core publishes shared memory** and **Godot renders particles**.

## Prerequisites
- CMake + MSVC (Visual Studio Build Tools)
- Python 3 + `scons` (`pip install scons`)
- Godot 4 (installed at `E:\Godot 4 STABLE`)
- Git (for `godot-cpp` submodule)

## Build
From repo root:
```powershell
.\genesis_engine\build_all.ps1
```

Initialize submodules if needed:
```powershell
git submodule update --init --recursive genesis_engine/renderer/addons/godot-cpp
```

Manual build:
```powershell
cmake -S genesis_engine -B genesis_engine/build
cmake --build genesis_engine/build --config Release

cd genesis_engine\renderer\addons\genesis_bridge
scons platform=windows target=template_debug
scons platform=windows target=template_release
```

## Run (Smoke Test)
1) Start SIM Core:
```powershell
genesis_engine\build\Release\genesis_sim.exe --particles 50000 --fps 60
```

2) Open Godot:
- Launch `E:\Godot 4 STABLE\Godot_v4.x.exe`
- Open project at `genesis_engine\renderer`
- Run the scene

You should see moving particles and a UI panel with sim time + particle count.

## Snapshot / Delta
Save a snapshot after a fixed number of steps:
```powershell
genesis_engine\build\Release\genesis_sim.exe --steps 200 --save snapshot.bin
```

Load a snapshot:
```powershell
genesis_engine\build\Release\genesis_sim.exe --load snapshot.bin --steps 100
```

Create a delta from a base snapshot:
```powershell
genesis_engine\build\Release\genesis_sim.exe --delta-from base.bin --steps 100 --delta-out delta.bin
```

## Chemistry Options
```powershell
genesis_engine\build\Release\genesis_sim.exe --chem-root ..\\data\\elements --chem-cache ..\\data\\elements\\chemdb.bin
genesis_engine\build\Release\genesis_sim.exe --chem-rebuild
genesis_engine\build\Release\genesis_sim.exe --chem-seed 8 --chem-heat-scale 1e-5
```

## Near Particles
```powershell
genesis_engine\build\Release\genesis_sim.exe --near-particles 50000 --particle-from-chem
```

## SHM Name Override
Default SHM name is `GenesisSim`. To override:
```powershell
$env:GENESIS_SHM_NAME="MySim"
genesis_engine\build\Release\genesis_sim.exe --name MySim
```

## AI (Deferred)
CLAS and specialist AI modules are **spec-only** in alpha.
No runtime AI, no training, and no data logging are enabled yet.
See `genesis_engine/clas_module` for the current specification.
