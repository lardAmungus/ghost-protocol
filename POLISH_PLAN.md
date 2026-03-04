# Ashen Circuit — Polish Plan

Goal: Take the game from "technically complete" to "polished release" — a game you'd
be proud to put on a flash cart and hand to someone.

Current state: Phase 21 complete. 25 source files, ~185KB ROM. Full quest loop works.
All systems functional. Visuals are minimal/geometric. Audio is procedurally generated.

---

## Phase A: Player Feel (highest impact, lowest effort)

The player character is the thing you interact with every frame. Small improvements
here have outsized impact on how the game *feels*.

### A1. Invincibility flash improvement
- Current: 60-frame i-frames with uniform 4-frame flicker after damage
- Change: Fast flicker (2-frame) for first 20 frames, slow flicker (8-frame) for
  remaining 40 — telegraphs "almost over" to the player
- File: `player.c` (~5 lines)

### A2. Attack feedback arc
- Current: Static attack frame held for 12 frames, sword is part of sprite
- Add: During PSTATE_ATTACK, spawn a temporary "slash arc" OBJ sprite (8x8, 1 tile,
  lasts 6 frames) in front of the player. Simple 3-pixel white arc shape. Gives a
  visual swoosh without changing the player sprite.
- Files: `player.c`, `gfx_data.c` (add slash tile + palette entry), `sprite.c` (alloc/free)

### A3. Damage numbers
- Current: Enemy hit plays SFX + screen shake. No indication of how much damage.
- Add: On hit, briefly display damage number as a text overlay above the enemy
  using `text_put_char()`. Float upward for 20 frames then clear. Use existing
  BG0 text layer.
- Files: `state_world.c` or new `floattext.c` (~60 lines)

### A4. Smoother camera
- Current: Linear 1/8 lerp with 120x80 dead zone
- Change: Tighten dead zone to 80x48 (less "dead" feeling), increase lerp to 1/4
  for faster tracking. Add look-ahead: offset camera target 16px in player's
  facing direction so you see more of what's ahead.
- File: `camera.c` (~10 lines)

---

## Phase B: Enemy & Boss Animation

Currently most enemies are static sprites that slide around. Adding even 2-frame
idle/walk cycles makes the world feel alive.

### B1. Enemy idle animation (all 7 types)
- Current: Slime and Bat have 2-frame toggle. Others are static.
- Add: 2-frame idle animation for Skeleton, Crawler, Spore, Construct, Wisp.
  Each needs 1 additional 16x16 frame (4 tiles) in gfx_data.c. Toggle on
  existing `anim_timer` (already in enemy.c for slime/bat).
- Sprite budget: +28 tiles (7 types x 4 tiles). Current usage is ~91 tiles of 1024.
  Plenty of room.
- Files: `gfx_data.c` (sprite data), `enemy.c` (extend anim to all types)

### B2. Enemy death effects
- Current: All 7 enemy types have identical 20-frame flicker death
- Add: Type-specific death behavior:
  - Slime: Flatten sprite (switch to squash frame) then fade
  - Skeleton: Scatter (brief random offset per frame before despawn)
  - Bat/Wisp: Spiral downward (modify y each frame during death)
  - Others: Existing flicker is fine
- 2 new death frames needed: slime squash, skeleton scatter (8 tiles total)
- File: `enemy.c` (~30 lines in death handler)

### B3. Boss attack telegraphs
- Current: Boss attacks are invisible — hitbox appears, no sprite change
- Add: 1 extra boss frame per boss for "windup" pose (arms raised / glowing /
  lunging). During BPHASE_ATTACK1 first 8 frames and BPHASE_ATTACK2 first 8
  frames, show windup frame. Remaining frames show normal frame.
- Sprite budget: +5 frames x 16 tiles = 80 tiles. Still within budget.
- Files: `gfx_data.c`, `boss.c` (frame selection in draw)

### B4. Boss vulnerability glow
- Current: Rapid 1-frame blink during BPHASE_VULNERABLE (every other frame hidden)
- Change: Instead of hiding, palette-swap to a brighter palette during vulnerable
  phase. Create 1 "glow" palette per boss (brighten all colors by +4 per channel).
  Swap OBJ_PALBANK during vulnerability, restore on exit.
- Files: `gfx_data.c` (5 glow palettes), `boss.c` (~8 lines in draw)

---

## Phase C: World & Environment

The world is currently flat and static. Adding ambient life makes exploration engaging.

### C1. Animated water tiles
- Current: Water/poison/cave water are static colored tiles
- Add: 2-frame water animation. On VBlank, every 16 frames swap the water tile
  in CBB1 between frame 0 and frame 1. Affects all water tiles on screen at once
  (palette-based trick: shift the water palette entry between two blues).
- This is a BG palette animation, not a tile swap — zero CPU cost.
- File: `world.c` or `state_world.c` (~8 lines in draw, palette write)

### C2. Parallax background (BG2)
- Current: BG2/BG3 disabled (no tile data loaded)
- Add: Load a simple 1-tile repeating pattern per area into BG2 (stars for caves,
  clouds for mountains, haze for fen, embers for citadel). Scroll BG2 at half
  camera speed for parallax depth.
- 1 tile + 1 palette per area = minimal VRAM cost. BG2 uses SBB30/CBB2 (already
  reserved in architecture).
- Files: `gfx_data.c` (5 parallax tiles), `world.c` (load on area change),
  `state_world.c` (scroll BG2 at half rate in draw)

### C3. Area entry text
- Current: Area name shown via hud_notify for 90 frames on first visit
- Change: Display area name centered on screen in large text (2x scale via
  doubling characters) for 120 frames with fade-in/fade-out using BG blend.
  More dramatic "you have arrived" moment.
- File: `state_world.c` (~20 lines)

### C4. Conduit visual feedback
- Current: Conduit barrier is static tiles until unlocked
- Add: Pulse conduit tile palette between two brightness levels (8-frame cycle).
  Stops pulsing after unlock (becomes normal floor). Same palette animation
  technique as C1.
- File: `state_world.c` (~6 lines in draw)

---

## Phase D: UI & HUD

### D1. Graphical HP bar
- Current: ASCII `[==========]` with `*` at low HP
- Change: Use BG0 tile graphics for HP bar. Define 3 tiles: full segment, half
  segment, empty segment. Draw HP bar using these tiles instead of text characters.
  Red-to-green gradient via palette. ~12 pixels wide per segment.
- Files: `gfx_data.c` (3 bar tiles in CBB0), `hud.c` (rewrite HP bar render)

### D2. Dialogue box border
- Current: ASCII `+--+` / `|  |` / `+--+` box
- Change: Use custom tile graphics for dialogue border. 9 tiles: 4 corners,
  4 edges, 1 fill. Load into CBB0 alongside font. Gives a proper RPG textbox look.
- Files: `gfx_data.c` (9 border tiles), `dialogue.c` (use tile IDs instead of ASCII)

### D3. NPC speaker name highlight
- Current: Speaker name is `[Elder]` as plain text in dialogue string
- Change: Render first line (speaker name) using a different palette row on BG0.
  BG0 supports 16 palettes — use palette 1 for speaker names (yellow/gold text)
  vs palette 0 for normal text (white). Parse `[Name]` prefix in dialogue_draw.
- Files: `dialogue.c` (~15 lines), `gfx_data.c` (1 gold text palette)

### D4. Item pickup popup
- Current: hud_notify shows "+1 Herb" etc. as blinking text at row 9
- Change: Show item name + icon rising from pickup position. Use a dedicated
  OBJ sprite for the item icon, animate it floating upward for 40 frames,
  pair with text. More satisfying "got it" feedback.
- Files: `item_drop.c` or `hud.c` (~25 lines)

---

## Phase E: Title & End Screens

### E1. Title screen art
- Current: Plain text "ASHEN CIRCUIT" centered
- Add: ASCII art logo (8-10 rows of styled text characters forming a logo).
  Add a subtitle that types out character-by-character (reuse dialogue reveal
  system). Background could use BG1 with a simple tile pattern.
- File: `state_title.c` (~40 lines)

### E2. Save file preview
- Current: "Press START: Continue" with no info about the save
- Add: When save exists, show: area name, player level, herb count below the
  Continue prompt. Read save data on title enter to display preview.
- File: `state_title.c` (~15 lines)

### E3. Game over retry improvement
- Current: Plain "GAME OVER" text, press START to return to title
- Change: Show player stats at death (Level, Area, Bosses defeated). Add
  "Continue from save?" option that loads directly into world state instead
  of going through title screen first.
- File: `state_gameover.c` (~25 lines)

### E4. Win screen stats
- Current: Epilogue → credits scroll → THE END
- Add: Before THE END, show a "Final Stats" screen: Level reached, bosses
  defeated count, areas explored count. Reads from quest_flags before save
  is invalidated.
- File: `state_win.c` (~20 lines)

---

## Phase F: Audio Polish

### F1. Regenerate music with better composition
- Current: Procedurally generated MOD files — functional but robotic
- Improve: Update `tools/generate_audio.py` with:
  - Longer patterns (4-8 instead of 1-4)
  - Velocity variation (notes aren't all same volume)
  - Arpeggios and chord progressions that resolve properly
  - Drums/percussion on channel 4
  - Each track gets a distinct feel matching its area
- File: `tools/generate_audio.py` (rewrite music generation)

### F2. Additional SFX
- Current: 14 SFX covering major events
- Add 4-6 more:
  - `SFX_STEP` — quiet footstep every 16 frames during walk (subtle)
  - `SFX_CONDUIT_PULSE` — when near active conduit barrier
  - `SFX_LOW_HP` — replace beep with a heartbeat sound
  - `SFX_DODGE` — when narrowly avoiding an attack (optional, complex)
- Files: `audio.h` (new IDs), `tools/generate_audio.py` (new WAVs), `audio.c` (map)

### F3. Music fade on area transition
- Current: Music stops abruptly, new track starts immediately
- Add: `audio_fade_music(int frames)` that uses Maxmod's `mmSetModuleVolume()`
  to fade out over N frames before starting new track. Call during area
  transition fade-to-black.
- Files: `audio.c` (~15 lines), `state_world.c` (call during transition)

---

## Phase G: Gameplay Depth (stretch goals)

These add actual new content/mechanics. Only pursue after A-F are solid.

### G1. Enemy spawn variety
- Current: Each area spawns fixed enemy types at fixed positions
- Change: Add 2-3 spawn point variations per area using RNG. Different enemy
  compositions on each visit. Increases replayability.
- File: `world.c` (~30 lines per area)

### G2. Hidden items
- Current: All items come from enemy drops or NPC gifts
- Add: 1-2 hidden item pickups per area (health herb in a corner, equipment
  behind a breakable wall). Rewards exploration.
- Files: `world.c`, `state_world.c` (~20 lines)

### G3. Boss phase 2 visual change
- Current: Bosses speed up at 50% HP but look identical
- Add: At 50% HP, swap boss palette to a "damaged" variant (darker/redder).
  Simple palette swap, no new sprites needed.
- Files: `gfx_data.c` (5 damaged palettes), `boss.c` (~5 lines)

### G4. Minimap on pause
- Current: Pause menu shows Resume/Items/Save/Quit
- Add: Tiny 5x1 area map on pause screen showing which areas are visited
  (filled boxes for visited areas from quest_flags[2], hollow for unvisited).
  Simple text-based, no graphics needed.
- File: `state_world.c` (~15 lines in pause draw)

---

## Implementation Order

Priority is based on: **player-facing impact** / **implementation effort**.

```
Phase A (Player Feel)      — 1-2 sessions, immediate feel improvement
Phase B (Enemy Animation)  — 2-3 sessions, world feels alive
Phase D (UI/HUD)           — 1-2 sessions, professional appearance
Phase E (Title/End)        — 1 session, first/last impression
Phase C (World/Environment)— 2-3 sessions, immersion
Phase F (Audio)            — 1-2 sessions, atmosphere
Phase G (Gameplay Depth)   — 2-3 sessions, replayability (stretch)
```

## ROM Budget

Current: ~185KB. GBA max: 32MB. Even with all additions we'll stay well under 256KB.
No concerns about running out of space for sprites, tiles, or audio.

## OBJ Tile Budget

Current usage: ~91 tiles of 1024 available. Phase B adds ~116 tiles (enemy frames +
boss frames). Phase A adds ~1 tile (slash arc). Total: ~208 tiles. Comfortable margin.
