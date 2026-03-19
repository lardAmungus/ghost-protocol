# Ghost Protocol — Character Build & Leveling Redesign

**Date**: 2026-03-19
**Status**: Approved (design phase)

## Summary

Replace the three-starting-class system (Assault/Infiltrator/Technomancer) with a single classless character that evolves through a 3-tier class tree. Overhaul stat allocation, skill progression, shop, controls, crafting UI, and add character customization.

---

## 1. Character Creation & Early Game

### New Game Flow

Title → Intro Scene → **Character Customization** → Terminal

### Character Customization

Presented at new game start, before entering the terminal. Two choices:

1. **Suit color** — 24 preset palette options (full spectrum). Recolors sprite indices 2-4 (dark/medium/bright armor). Outline (index 1) and detail colors remain fixed.
2. **Visor color** — 24 preset palette options. Recolors sprite indices 5-7 (dim/mid/bright visor glow).

Player sees a live preview of their character as they cycle colors. Choices are permanent for that save file.

### Levels 0–4 (Classless)

- Player starts at level 0 with base stats (ATK=5, DEF=5, SPD=5, LCK=5)
- Skill points and stat points are earned on **level-up** (transition from level N to N+1), not at level 0
- Each level-up (0→1 through 3→4) adds **1 point to each of ATK, DEF, SPD, LCK** (4 points/level)
- Level 4→5 triggers class selection; from 5 onward, stat allocation switches to 5 points/level (2+2+1) — the extra point represents class specialization focus
- Double jump available from the start — universal, never lost
- No active skills yet — basic weapon attack only
- 1 skill point earned per level-up, banked until T1 class is chosen (4 banked points available at level 4→5 transition, plus the point earned at level 5 = 5 points total)

### Level 5 — Tier 1 Class Selection

On reaching level 5, a selection screen presents **Trojan / Infiltrator / Technomancer**. Each option shows its stat allocation pattern and initial active skills. Choice is permanent.

---

## 2. Stat Allocation & Progression

### Auto-Allocation (Level 5+)

Each level-up distributes 5 points to 3 of 4 base stats. The 4th stat is equipment-only.

| Class | Primary (2 pts each) | Secondary (1 pt) | Equipment-Only |
|-------|---------------------|-------------------|----------------|
| **Trojan** | ATK, DEF | SPD | LCK |
| **Infiltrator** | SPD, LCK | ATK | DEF |
| **Technomancer** | ATK, LCK | DEF | SPD |

### Level Cap

- **Base cap**: 40
- **Endgame cap**: 99 (unlocked when Bug Bounty mode is entered for the first time — `game_stats.endgame_unlocked` flag, same trigger as current implementation)
- T3 at level 55 requires endgame to reach, which is by design — T3 is endgame content

### XP Curve

Existing quadratic formula retained: `BASE_XP * level * (100 + level) / 100` where `BASE_XP = 90`. Endgame Bug Bounty missions with threat level scaling provide the XP throughput needed for 40→99 progression. Exact tuning deferred to playtesting.

### Stat Recomputation Triggers

- On level-up
- On equipment change
- On tier evolution selection

---

## 3. Class Tier Tree

### Tier Unlock Levels

- **T1**: Level 5 (3 choices)
- **T2**: Level 25 (2 choices, specific to T1 class)
- **T3**: Level 55 (2 choices, specific to T2 spec — requires endgame content)

All tier choices are permanent. Skills from earlier tiers are kept and accumulate.

### Tree Structure

```
TROJAN ────────┬─ Juggernaut ──────┬─ Bastion
               │                   └─ Warlord
               └─ Berserker ──────┬─ Reaver
                                   └─ Demolisher

INFILTRATOR ───┬─ Specter ─────────┬─ Wraith
               │                   └─ Shade
               └─ Edge Runner ────┬─ Razor
                                   └─ Chrome Phantom

TECHNOMANCER ──┬─ Architect ───────┬─ Machinist
               │                   └─ Conduit
               └─ Netweaver ──────┬─ Daemon
                                   └─ Gridrunner
```

### Active Skills by Class & Tier

#### Trojan

| Tier | Spec | Skills |
|------|------|--------|
| T1 | Trojan (base) | **Charged Shot** (3x damage projectile), **Iron Skin** (DEF doubled, 3s) |
| T2 | Juggernaut | **Iron Protocol** (absorb shield → bonus armor), **Seismic Stomp** (AoE slow), **Warcry Pulse** (+50% ATK buff) |
| T2 | Berserker | **Chrome Rage** (2x ATK, 0.5x DEF, lifesteal), **Typhoon Burst** (360 AoE), **Adrenaline Surge** (kill → 3x next hit) |
| T3 | Bastion | **Fortress Mode** (immobile, reflect, 80% DR), **Aegis Field** (projectile barrier wall) |
| T3 | Warlord | **Concussive Charge** (dash+stun), **Warpath** (stacking ATK on consecutive hits) |
| T3 | Reaver | **Blood Circuit** (25% lifesteal, 8s), **Last Stand** (below 25% HP → 2x ATK, KB immune) |
| T3 | Demolisher | **Shockwave Slam** (ground-traveling wave), **Payload Rounds** (shots explode with 50% splash) |

#### Infiltrator

| Tier | Spec | Skills |
|------|------|--------|
| T1 | Infiltrator (base) | **Shadow Step** (teleport 32px forward), **Phase Shot** (projectile pierces walls) |
| T2 | Specter | **Phase Cloak** (invis 4s, first hit 3x), **Decoy Hologram** (aggro decoy, 5s), **Shadow Strike** (teleport behind enemy + backstab) |
| T2 | Edge Runner | **Overclock Reflexes** (+50% SPD, 2x attack speed, 5s), **Blade Storm** (mark+dash 3 enemies), **Ricochet Shot** (bounces between 3 enemies) |
| T3 | Wraith | **Phase Walk** (intangible 3s, AoE pulse on exit), **Marked for Death** (target takes 2x damage, 6s) |
| T3 | Shade | **Smoke Cloud** (area blind+stealth, 6s), **Wire Trap** (invisible stun mine) |
| T3 | Razor | **Nerve Strike** (dash-kill resets cooldown), **Flurry** (5-hit combo, stacking damage) |
| T3 | Chrome Phantom | **Bullet Time** (enemies half speed, 4s), **Shuriken Barrage** (5-projectile fan) |

#### Technomancer

| Tier | Spec | Skills |
|------|------|--------|
| T1 | Technomancer (base) | **Turret Deploy** (6 projectiles in cone), **Scan Pulse** (5 wide piercing beams) |
| T2 | Architect | **Sentry Network** (deploy 2 turrets), **Tesla Mine** (AoE electric trap), **Repair Pulse** (heal 20% + reduce cooldowns 30%) |
| T2 | Netweaver | **Contagion** (chaining DoT, spreads 3x), **Synapse Burn** (burst nuke, kill refunds 50% CD), **System Crash** (screen-wide stun 2s) |
| T3 | Machinist | **Drone Swarm** (3 homing explosive drones), **Bastille Field** (immobilize zone, 4s) |
| T3 | Conduit | **Shield Siphon** (absorb shield → heals on break), **Temporal Anchor** (snapshot position+HP, rewind after 5s) |
| T3 | Daemon | **Overclock Protocol** (no cooldowns, costs 5% HP per use, 6s), **Cascade Failure** (forking projectile, splits to 2 targets) |
| T3 | Gridrunner | **Exploit** (+50% damage debuff on target, 6s), **Feedback Loop** (store damage dealt → AoE burst release after 8s) |

### Movement

Double jump is universal across all classes. Dash-type skills (Shadow Step, Concussive Charge, Nerve Strike, Phase Walk) are class-specific active skills that can be leveled like any other skill — ranking up increases distance/damage and reduces cooldown.

### Tier Selection UI

When selecting a tier, the screen shows the two (or three for T1) choices side-by-side with skill names and brief descriptions.

---

## 4. Skill Point System

### Earning

1 skill point per level. Points earned at levels 0-4 are banked until T1 class is chosen at level 5 (5 points available immediately).

### Spending

Points are invested into active skills only. No passive stat bonuses — those are handled by auto-allocation and equipment.

### Ranks

- **Max rank per skill**: 10
- Ranking up improves the skill:
  - **Damage skills**: +10-15% damage per rank
  - **Buff/debuff skills**: +duration or +intensity per rank
  - **Cooldown**: reduced by a small amount per rank
  - **Deployables**: +duration, +damage, or +count per rank

### Budget

A level 99 player has 99 skill points. A full build path (e.g. Infiltrator → Specter → Wraith) grants 2 + 3 + 2 = 7 skills. Maxing all 7 at rank 10 = 70 points, leaving 29 for prioritization. Even at max level, not everything can be maxed — meaningful choices required.

### Activation

- **SELECT** (press): Opens skill wheel showing all unlocked skills with current rank and cooldown status
- **D-pad**: Navigate skills in the wheel
- **A**: Slot the highlighted skill (closes wheel)
- **B / SELECT**: Close wheel without changing
- **R** (press): Fire the currently slotted skill. If no skill is slotted or player has no skills yet (levels 0-4), R does nothing (no SFX, no visual feedback). If skill is on cooldown, R does nothing.
- One active skill slot at a time
- First skill unlocked at T1 is auto-slotted so the player doesn't have to open the wheel immediately

---

## 5. Shop Restructure

### Items

| Item | Effect | Type |
|------|--------|------|
| **Health Pack** | Restore 20 HP | Instant (no limit) |
| **CD Reset** | Reset all skill cooldowns | Instant (no limit) |
| **XP Booster** | +15% XP gain | Charge-based (max 5) |
| **Shield Charge** | +5 DEF | Charge-based (max 5) |
| **Credit Finder** | +15% credit drops | Charge-based (max 5) |
| **Loot Magnet** | Increased rare item drop chance | Charge-based (max 5) |

All items have fixed prices (no scaling).

### Charge System

Applies to XP Booster, Shield Charge, Credit Finder, and Loot Magnet:

- **Max 5 charges** per buff type
- Charges are tracked independently per buff
- Buying an item when charges exist adds charges (capped at 5)
- Charges persist across all progression events (level-ups, stat recomputes, etc.)

**Charge consumption**:

| Event | Charges consumed |
|-------|-----------------|
| Complete a contract / bug bounty / mission | 1 |
| Die during a level | 1 |
| Exit a level early | 1 |
| Retry after death, then exit during retry | 2 (1 for death + 1 for exit) |
| Retry after death, then complete the mission | 2 (1 for death + 1 for completion) |

**Edge cases**:
- 0 charges = buff inactive, no consumption occurs, no UI indication of the buff
- Charges cannot go negative — consumption is skipped if charges are already 0
- Buying at max charges (5) is blocked — shop shows "MAX" instead of price

### Removed Items

- ATK +1, DEF +1, SPD +1, LCK +1 (stats are auto-allocated)
- Repair Prog (redundant full heal, badly named)
- Overclock (was actually +5 max HP, badly named)

---

## 6. Weapon Visuals

Equipped weapons change the player sprite's weapon attachment (sprite indices B-C).

- Weapons vary in **size and shape** — enough variation that progression is visible, nothing comically oversized or tiny
- Visual tied to **weapon type** (pistol vs cannon vs blade, etc.)
- Higher rarity / later-game weapons get slightly more detailed sprites
- Weapon palette is driven by the **weapon itself**, independent of player suit color
- Implementation: weapon sprites use 1-2 additional OBJ tiles attached to the player's position, offset based on facing direction

Stat system for weapons (ATK bonuses, types, rarity tiers) remains unchanged.

---

## 7. Controls

### Skill Activation (Reworked)

| Input | Action |
|-------|--------|
| **SELECT** | Open skill wheel (overlay) |
| **D-pad** (in wheel) | Navigate skills |
| **A** (in wheel) | Slot highlighted skill, close wheel |
| **B / SELECT** (in wheel) | Close wheel, no change |
| **R** | Fire slotted skill (if off cooldown) |

### HUD

Currently slotted skill name and cooldown bar always visible on HUD.

### Unchanged

- D-pad: movement
- A: jump (double jump always available)
- B: attack
- L: (unchanged from current)
- START: pause menu

---

## 8. Crafting Menu Redesign

### Problem

Current crafting is blind — player selects items and discovers the result after committing.

### Solution

All crafting operations show a **preview of the result** before confirming.

### Operations

- **Fuse**: Select 3 items of same rarity → the result item is generated before confirming so the preview shows the actual item (type, stats, rarity letter). If the player cancels, the generated result is discarded. This makes fusing deterministic from the player's perspective — they see what they'll get.
- **Forge**: Select item + pay shards → preview shows rerolled stat ranges (min-max)
- **Salvage**: Select item → shows exact shard value

### Crafting Flow

1. Choose operation (Fuse / Forge / Salvage)
2. Select item(s)
3. **Preview screen** shows result (or result range for Forge)
4. Confirm (A) or Cancel (B)

### Rarity Letter Grades

Replaces full rarity names everywhere in UI (inventory, loot drops, crafting, shop):

| Rarity | Letter | Color |
|--------|--------|-------|
| Common | **C** | White |
| Uncommon | **B** | Green |
| Rare | **A** | Blue |
| Epic | **A+** | Purple |
| Legendary | **S** | Orange |
| Mythic | **S+** | Gold |

Color coding matches existing rarity palette assignments.

---

## 9. What Gets Removed

- CLASS_ASSAULT renamed to CLASS_TROJAN. CLASS_INFILTRATOR and CLASS_TECHNOMANCER keep their names. The concept of "starting class" is removed — class is chosen at level 5, not character creation.
- Assault as a starting class → replaced by classless start with class selection at level 5
- Old skill tree (3 branches × 4 passives × 3 ranks) → active skill ranking
- Old evolution system (Vanguard/Commando/Phantom/Striker/Architect/Hacker) → new 3-tier tree
- Old ability wheel (hold R to open, release to fire) → SELECT + R scheme
- Shop stat boosts (ATK/DEF/SPD/LCK +1, Repair Prog, Overclock)

## 10. What Stays

- Equipment system (weapon/armor/accessory, rarity tiers, loot drops)
- XP curve formula (may need tuning)
- Credits/currency system
- Mission/contract structure
- Bug Bounty endgame
- All enemy types, bosses, level generation
- Save system (expanded to store: chosen colors, tier choices, skill ranks, buff charges)

---

## 11. Implementation Notes

### Skill Data Structure

All 48 skills (6 T1 + 18 T2 + 24 T3) are defined in a global `skill_table[]` indexed by `SKILL_*` enum values. Each player stores:

```c
u8 skill_ranks[MAX_PLAYER_SKILLS]; /* max 7 skills per build path, rank 0-10 */
u8 slotted_skill;                   /* index into player's unlocked skills */
u8 tier_choices[3];                 /* T1: class enum, T2: spec enum, T3: spec enum */
u8 suit_color;                      /* 0-23 palette preset index */
u8 visor_color;                     /* 0-23 palette preset index */
```

The `tier_choices` array determines which skills are unlocked. A lookup function maps (class, T2 choice, T3 choice) → list of skill IDs from the global table.

### Color Palette Storage

24 suit presets and 24 visor presets stored as const arrays of RGB15 triplets in ROM (24 × 3 × 2 bytes = 144 bytes each). At runtime, the selected preset's 3 colors are written into the appropriate OBJ palette bank 0 indices.

### Weapon OBJ Tiles

Weapon attachment sprites use OBJ tiles in a reserved range (e.g. 280-295). Each weapon type gets 1-2 tiles. With 8 weapon types this requires 8-16 tiles — well within the 1024 tile limit. Only the equipped weapon's tiles are loaded at a time. Weapon attachment uses 1 additional OAM entry (player already uses 1, total becomes 2 — well within the 128 OAM limit).

### Complex Skill Edge Cases

- **Fortress Mode** (Bastion): Player is rooted — cannot move, cannot be knocked back. Canceled early by pressing A (jump). If activated mid-air, player drops to ground first (gravity still applies), then roots. Duration is timer-based (4s).
- **Temporal Anchor** (Conduit): Snapshots x, y, HP only (10 bytes). On rewind, player teleports to snapshot position. If snapshot position is now inside a solid tile (e.g. breakable wall was placed), nudge upward until clear — same logic as spawn validation. World/enemy state does NOT rewind.
- **Feedback Loop** (Gridrunner): Damage accumulator is `u16` (max 65535). Overflow is capped, not wrapped. AoE burst on expiry damages all on-screen enemies.

### Save Migration

Old saves are **incompatible** with the redesign. The `SAVE_MAGIC` value is changed to a new constant, causing old saves to fail validation and be treated as empty (fresh start). This is acceptable since the redesign fundamentally changes progression — there is no meaningful migration path from the old 3-class system.

### L Button

L button has no current function in the existing build. It remains unbound. Can be assigned in future if needed (e.g. quick-swap between two slotted skills as a future enhancement).

---

## Design References

Class tree names and skill concepts drawn from:
- **Cyberpunk 2077**: Solo, Netrunner, Berserk OS, Contagion quickhack, Overclock mode
- **Warframe**: Rhino (Iron Skin, Roar, Stomp), Valkyr (Hysteria), Ash (Blade Storm, Smoke Screen), Loki (Decoy), Vauban (Bastille, Tesla), Protea (Temporal Anchor)
- **Deus Ex**: TITAN augmentation, Typhoon, Glass-Shield Cloaking, Reflex Booster
- **Mass Effect**: Vanguard (Biotic Charge), Infiltrator (Tactical Cloak), Engineer (Sentry Turret, Combat Drone)
- **Shadowrun**: Street Samurai, Decker (Mark Target), Rigger (drones), Technomancer (Complex Forms)
