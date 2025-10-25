
# ArmyMenRTS — Minimal Playable Prototype (SFML)

Features:
- 32x20 tile grid (64px tiles)
- Fog of War (revealed around the player's unit)
- Select the unit with Left Click (green ring)
- Right Click sets a move target; the unit walks there
- Zoom with Mouse Wheel, Pan camera with WASD
- Simple HUD text

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ArmyMenRTS
```

Requires SFML 2.5+ installed.
