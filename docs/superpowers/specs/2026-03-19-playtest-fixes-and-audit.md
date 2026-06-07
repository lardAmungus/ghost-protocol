# Playtest Fixes & Complete Game Audit

## Overview

Post-redesign playtest revealed three issues and the need for a comprehensive code audit to verify game integrity after 14 commits rewrote core systems.

## Issue 1: Weapon Does Zero Damage (CRITICAL)

**Root cause:** `do_shoot()` in player.c has a `switch(cls)` with cases for CLASS_TROJAN (0), CLASS_INFILTRATOR (1), CLASS_TECHNOMANCER (2) — but no `default` case. When `cls = 0xFF` (classless, levels 0-4), the entire switch body is skipped. No projectile is spawned. The player sees muzzle flash particles and hears the SFX, but nothing flies out to deal damage.

**Secondary symptom:** `player_state.shoot_cooldown` is also never set when the switch is skipped, so `do_shoot()` fires every frame — producing muzzle flash/SFX spam with zero projectiles.

**Fix:** Add `default:` label before `case CLASS_TROJAN:` so they share the same code block. Classless players use Trojan weapon behavior (buster, standard cooldown). This matches the existing safety pattern in physics lookup where classless clamps to CLASS_TROJAN.

**Verification:** Confirm projectiles spawn and deal damage at level 0 with no equipped weapon (ATK=5, CLASSLESS_ATK). Confirm shoot_cooldown is set. Confirm behavior transitions correctly when player picks a class at L5.

## Issue 2: Text Skips Instantly

**Root cause:** `update_story()` in state_terminal.c line 808 does `if (input_hit(KEY_A) || input_hit(KEY_B))` and immediately advances `story_page++`. No typewriter rendering, no two-stage advance.

**New behavior (narrative text only):**
1. Text appears character-by-character via typewriter (2 frames/char)
2. Press A/B while text is typing → complete current page instantly (all remaining chars appear at once)
3. Press A/B when page is fully displayed → advance to next page (or close dialogue if last)

**Scope:**
- TSUB_STORY dialogues in state_terminal.c (ZERO/AXIOM/PROXY briefings)
- New STATE_INTRO prologue sequence
- Does NOT apply to: terminal menus, HUD text, title subtitle, status text

**Implementation:**
- Add `story_char_pos` and `story_char_timer` statics to state_terminal.c
- Reset `story_char_pos = 0` and `story_char_timer = 0` when entering TSUB_STORY (line 321-323) AND on each `story_page++` advance
- `update_story()`: tick typewriter each frame. On A/B: if `char_pos < len` → set `char_pos = len` (complete). Else → advance page and reset char_pos/timer.
- `draw_story()`: render only first `char_pos` characters of current page, not the full string. Existing per-character loop with 2-char buffer only needs a `char_idx < story_char_pos` guard.
- Handle multi-line pages: typewriter must track total chars across `\n` breaks

## Issue 3: No Intro / No World-Building

**Problem:** Title → Start → STATE_CHARSEL immediately. No context, no story setup. Feels like a demo.

**Solution:** New STATE_INTRO state between title and charsel (new-game only).

**Flow:**
- New game: Title → STATE_INTRO → STATE_CHARSEL → STATE_TERMINAL
- Continue: Title → STATE_TERMINAL (unchanged, skip intro)

**Content: 6 screens of prologue dialogue**

All lines must fit within 26 characters (dialogue box renders from column 3, screen is 30 columns).

Screen 1 — Terminal boot:
```
[GHOST PROTOCOL v2.1]
Secure link established.
Connection verified.
Status: UNREGISTERED
```

Screen 2 — ZERO introduces themselves:
```
ZERO> You're in. Took a
while to find you.
I'm ZERO -- your handler
from here on out.
```

Screen 3 — World situation:
```
ZERO> The corps own the
net now. MICROSLOP, GOGOL
AMAZOMB -- they carved it
up and locked it down.
```

Screen 4 — Your role:
```
ZERO> But you -- you're a
Ghost. No registration,
no trace. You don't exist
in their systems.
```

Screen 5 — The mission:
```
ZERO> We need you to jack
in. Extract data, break
their grip. Take them
apart from the inside.
```

Screen 6 — Call to action:
```
ZERO> First -- customize
your rig. Then we'll get
you started. First job's
waiting.
```

**Presentation:**
- Circuit board BG (same tile data as title screen — extract `load_circuit_bg()` as shared helper or duplicate loading code in state_intro.c)
- Dialogue box with bordered frame (existing draw_box style)
- ZERO speaker name in cyan/gold (existing speaker palette system)
- Two-stage text advance (same system as Issue 2)
- Page indicator: "1/6" in bottom-right
- Skip: START opens "Skip intro? A=Yes B=No" confirm (START avoids conflict with A/B text advance)

**New files:** `source/states/state_intro.c`, `include/states/state_intro.h`

**Modified:**
- `include/states/state_ids.h` — add STATE_INTRO, increment STATE_COUNT (currently 7 → 8)
- `source/states/state_title.c` — route new-game to STATE_INTRO instead of STATE_CHARSEL
- `source/main.c` — add state_intro_enter/update/draw/exit to state machine dispatch table
- Headless test path (main.c) — auto-transition at frame 5 must bypass STATE_INTRO (go directly to STATE_TERMINAL as before)

## Issue 4: Complete Game Audit

Full integrity audit of every source file, header, and tool script in the codebase. Not limited to redesign-affected code — the entire game from top to bottom.

### Audit Categories

**A. Deleted field references:** Any remaining use of `skill_tree[]`, `ability_unlocks`, `cooldown_ability[]`, `evolution`, `evolution_pending`, `last_used_ability`, `player_ability_wheel_is_open()`, old class names (CLASS_ASSAULT), old MAX_LEVEL without ENDGAME distinction.

**B. Classless (0xFF) safety:** Any code that indexes arrays by `player_class` without bounds checking — physics lookups, stat tables, class name lookups, save preview. All must handle 0xFF gracefully.

**C. Arithmetic safety:** Division by zero in skill cooldown/damage at rank 0, XP calculations at level 0, stat scaling with zero classless_levels. Integer overflow in damage/XP multiplication chains.

**D. Save data integrity:** pack_save/unpack_save field alignment, reserved[] size matches _Static_assert(512), all new fields (tier_choices, skill_ranks, slotted_skill, suit_color, visor_color, buff_charges) correctly packed/unpacked, byte ordering, SRAM 8-bit write compliance.

**E. Array bounds:** Tier choices array[3] indexed by tier 0-2, skill_ranks[7] indexed by skill slot, shop charges[4] indexed by charge type, enemy type arrays indexed by subtype.

**F. State machine integrity:** All state enter/exit functions properly clean up (REG_BLDCNT, blend, sprites, audio). No stale state leaking across transitions. Skill wheel closed on pause/death/state exit.

**G. Collision and physics:** Projectile-vs-enemy AABB correct, entity physics_resolve called (not raw velocity), boss/enemy spawn positions valid, camera bounds respected.

**H. Projectile system:** All 8 weapon-type projectile behaviors work for all 3 classes plus classless default. Per-class fire patterns (spread counts, pierce flags, damage multipliers) correct. Projectile flags (PROJ_ACTIVE, PROJ_ENEMY, PROJ_PIERCE, PROJ_PHASE) used correctly at all spawn sites.

**I. Audio:** Correct SFX for all new interactions (SELECT wheel, R fire, tier selection, color cycling). No orphaned SFX enum references. Music transitions clean.

**J. HUD correctness:** Display at level 0 with no skills, no weapon, classless. Cooldown bar with no slotted skill. XP bar at level 0. Score mode during bug bounty. All edge cases.

**K. Memory safety:** No stack-allocated large structs (SaveData must be static EWRAM_BSS). No buffer overflows in text rendering. text_put_char bounds checking. sprintf/format string safety.

**L. Tool scripts:** All tool output matches engine expectations (sample counts, enum ordering, tile data formats). Verify consistency with redesigned systems.

### Audit Scope — Files

**Engine (11 files):**
main.c, video.c, input.c, audio.c, camera.c, collision.c, entity.c, sprite.c, rng.c, text.c, save.c

**Game (24 files):**
player.c, enemy.c, boss.c, projectile.c, abilities.c, skills.c, shop.c, hud.c, loot.c, levelgen.c, networld.c, physics.c, particle.c, quest.c, bugbounty.c, terminal.c, colors.c, itemdrop.c, content_tilesets.c, content_sprites.c, content_levels.c, content_env_anim.c, content_cutscenes.c, content_codex.c

**States (6 files):**
state_title.c, state_charsel.c, state_terminal.c, state_net.c, state_gameover.c, state_win.c

**Headers (all .h files):**
Every header — struct layouts, enum values, constants, API signatures, macro correctness

**Tools (all scripts in tools/):**
generate_audio.py, generate_audio_code.py, generate_extra_content.py, generate_rom_data.py, generate_placeholder.py, boss_sprites.py, boss_gen.py, enemy_sprites.py, sprite_gen.py

### Fix Policy

- All issues fixed directly — no asking permission
- Every fix verified: does it break anything downstream? Does anything upstream break this fix?
- Build and test after each batch of fixes
- Zero warnings target maintained

## Deferred

- Weapon visual sprites (OBJ tile attachment per weapon type)
- Complex skill mechanics (Fortress Mode, Temporal Anchor, etc.)
- XP curve tuning
- L button binding
