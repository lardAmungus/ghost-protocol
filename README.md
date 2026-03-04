# Ghost Protocol

A cyberpunk side-scrolling action platformer for the Game Boy Advance.

Play as a mercenary hacker navigating the net — a digital underworld of rogue AI, corporate firewalls, and encrypted vaults. Choose your class, take contracts from a terminal hub, and battle through procedurally generated levels to bring down five tech-parody megacorp bosses.

Inspired by Megaman, The Matrix, Borderlands, and Skyland GBA.

---

## Features

- **3 playable classes** — Assault (tank/buster), Infiltrator (stealth/rapid fire), Technomancer (multi-aim)
- **Procedural levels** — 128×32 tile maps seeded per contract, 8 section types with sub-variants
- **5 acts / 20 story missions** — voiced dialogue between ZERO, AXIOM, and PROXY
- **5 bosses** — Microslop, Gogol, Amazomb, Crapple, Faceplant; phase-based AI with enrage
- **6 enemy types** — Sentry, Patrol, Flyer, Shield, Spike, Hunter
- **Loot system** — 5 weapon types, 5 rarity tiers, procedural names, DPS comparison
- **Bug Bounty mode** — 5 unlockable tiers of increasingly brutal survival runs
- **9 MOD music tracks + 18 WAV SFX** via Maxmod
- **3 SRAM save slots** with CRC validation

---

## Building

### Requirements

- [devkitPro](https://devkitpro.org/) with devkitARM and libtonc
- [Maxmod](https://maxmod.org/) (included with devkitPro)

### With Docker (recommended)

A Docker development environment is provided separately. Start the container and run:

```bash
docker exec gba-dev bash -c "cd /workspace && make"
```

The ROM is output to `build/game.gba`.

### Without Docker

Set `DEVKITPRO` and `DEVKITARM` in your environment, then:

```bash
make
```

### Headless testing

```bash
docker exec gba-dev bash -c "cd /workspace && mgba-rom-test -S 0x03 --log-level 15 build/game.gba"
```

---

## Project Structure

```
source/
  main.c           — Entry point, game loop
  engine/          — Reusable GBA systems (video, input, audio, save, entity, camera, collision, text)
  game/            — Game logic (player, enemies, loot, HUD, inventory, world)
  states/          — State machine (title, terminal hub, net/gameplay, gameover, win)
include/
  engine/          — Engine headers
  game/            — Game headers
  states/          — State headers
  mgba/            — mGBA debug logging header
audio/
  music/           — MOD tracker files (9 tracks)
  sfx/             — WAV sound effects (18 clips)
tools/
  sprite_gen.py    — Generates player + enemy 4bpp tile data from hex grids
  boss_gen.py      — Generates boss sprite data
  generate_audio.py
Makefile
```

---

## GBA Technical Notes

- **CPU**: ARM7TDMI @ 16.78 MHz, no FPU, no cache
- **Display**: Mode 0 — BG0 (UI), BG1 (64×32 scrolling world), BG2 (parallax)
- **Audio**: Maxmod, 16 KHz mixing, 8 channels
- **Save**: SRAM 32 KB, 8-bit access only, magic + CRC validation, 3 slots
- All game logic uses fixed-point math (no floats). Static allocation only — no malloc.

---

## License

Source code: MIT
Audio assets: see individual file headers for attribution
