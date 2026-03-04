# GBA Action RPG

## Build (all commands run via docker exec gba-dev)
- `docker exec gba-dev bash -c "cd /workspace && make"` — Incremental build
- `docker exec gba-dev bash -c "cd /workspace && make clean && make"` — Full rebuild
- `docker exec gba-dev bash -c "cd /workspace && mgba-rom-test -S 0x03 --log-level 15 build/game.gba"` — Headless test

## Critical Rules
- YOU MUST use fixed-point math (8.8 or 4.12). GBA has NO FPU.
- NEVER use malloc/free. Use static allocation only.
- ALL hardware register access MUST use volatile pointers.
- NEVER write 8-bit values to VRAM, OAM, or Palette RAM.
- SRAM access is 8-bit ONLY — never use memcpy on SRAM.
- Always compile ROM code with -mthumb. ARM mode only in IWRAM.
- Use int (32-bit) for loop variables and locals, never u8/u16.
- Test with mgba-rom-test after every significant change.
- Commit after every working feature.

## Architecture
- Language: C (libtonc) | Target: ARM7TDMI | Build: devkitARM Makefile
- Display: Mode 0, 3 BG layers (BG0=UI SBB31/CBB0, BG1=World SBB28-29/CBB1 64x32, BG2=Parallax SBB30/CBB2 half-speed scroll)
- Audio: Maxmod (16 KHz mixing, 8 channels)
- Save: SRAM 32 KB with magic number + checksum validation
- Container: gba-dev (docker exec for all build/test commands)

## Key Files
- source/main.c — Entry point, game loop, state machine (title, world, gameover states)
- source/engine/ — Reusable systems (video, input, audio, save, entity, camera, rng, sprite, collision, text)
- source/game/ — Game logic (player, enemy, npc, dialogue, hud, inventory, item_drop, world)
- source/states/ — State machine states (title, world, gameover, win/credits)
- include/ — Headers (mirrored structure: engine/, game/, states/, mgba/)
- include/game/gfx_data.h — Extern declarations for sprite/tile/palette data (uses RGB15_C macro)
- source/game/gfx_data.c — All sprite/tile/palette array definitions (single copy in ROM)
- include/states/state_ids.h — Shared state ID constants (STATE_NONE, STATE_TITLE, etc.)
- graphics/ — Source PNGs + .grit sidecar files (placeholder)
- audio/ — .xm tracker files + .wav sound effects (placeholder)
- maps/ — Tiled .tmx/.json level data (placeholder)
- tools/ — Python asset conversion scripts

## Project Location
- Host source: /opt/docker/projects/gba/src/
- Design docs: /opt/docker/projects/gba/docs/game-design.md
- Story design: /opt/docker/projects/gba/docs/story-design.md
- Environment docs: /opt/docker/projects/gba/docs/gba-dev-environment.md

## OBJ Palette Banks
- 0: Player
- 1: Slime enemies
- 2: Skeleton enemies
- 3: NPCs
- 4: Item drops
- 5: Boss Rotmaw
- 6: Boss Ironhusk
- 7: Bat enemies
- 8: Crawler enemies
- 9: Spore enemies
- 10: Boss Gloomfang
- 11: Construct enemies
- 12: Wisp enemies
- 13: Boss Voss
- 14: Boss Siphon

## OBJ Tile Layout
- 0-19: Player (5 frames x 4 tiles)
- 20-27: Slime (2 frames x 4 tiles)
- 28-35: Skeleton (2 frames x 4 tiles, alt at 93-96)
- 36-39: NPC (1 frame x 4 tiles)
- 40: Item gem (1 tile)
- 48-63: Boss (32x32 = 16 tiles)
- 64-71: Bat (2 frames x 4 tiles)
- 72-75: Crawler (2 frames x 4 tiles, alt at 97-100)
- 76-79: Spore (2 frames x 4 tiles, alt at 101-104)
- 80-83: Construct (2 frames x 4 tiles, alt at 105-108)
- 84-91: Wisp (2 frames x 4 tiles)
- 92: Slash arc (1 tile)
- 93-96: Skeleton idle1 alt frame
- 97-100: Crawler idle1 alt frame
- 101-104: Spore idle1 alt frame
- 105-108: Construct idle1 alt frame
- 109-112: Slime squash death frame

## Known Issues / Gotchas
- enemy_post_move_all() removed — merged into enemy_update_all() (single-pass).
- Player has PSTATE_ATTACK_COOLDOWN (8 frames) after each attack — player_is_attacking() only true during PSTATE_ATTACK.
- Player has PSTATE_DEAD (30-frame death animation) before STATE_GAMEOVER transition.
- video_shake_update() called from player_update() — must be called every frame.
- Pause menu is handled inside state_world_update (not a separate top-level state).
- Area transitions use REG_BLDCNT/REG_BLDY for fade — must clear on state exit.
- RGB15() is an inline function in libtonc, NOT a macro. Use RGB15_C() for static initializers.
- devkitPro env vars are NOT set in non-login docker shells. Paths hardcoded in Makefile.
- build/ is a docker volume mount — cannot rm -rf it. Clean individual file types.
- s32 on ARM is long int — use %ld not %d in format strings.
- ATTR2_ID()|ATTR2_PALBANK() returns int — cast to u16 to avoid conversion warnings.
- entity_spawn sets oam_index=OAM_NONE (0xFF). Despawn checks != OAM_NONE before freeing.
- entity_get_high_water() returns upper bound for entity iteration — use instead of MAX_ENTITIES.
- player_post_move() handles tile collision + world bounds — call after entity_update_all().
- headless_check() gated behind mgba_ok — game runs normally on real hardware.
- entity_init() properly despawns all entities before clearing (frees OAM sprites).
- Draw functions accept (s32 cam_x, s32 cam_y) — never mutate entity positions for rendering.
- save_write() auto-computes CRC-16 checksum before writing to SRAM.
- Use memset16/memcpy16 for VRAM writes, never memset (8-bit writes silently dropped).
- World map is 64x32 tiles — uses two consecutive screenblocks (SBB 28+29).
- Entity.last_hit_id tracks which attack generation last hit this entity (prevents multi-hit).
- enemy_post_move_all() handles enemy tile collision — call after entity_update_all().
- text_put_char() is an inline direct VRAM write — use for single-char rendering in tight loops.
- extern game_request_state lives in state_ids.h — do NOT redeclare in .c files.
- quest_flags[0] = boss defeat bits (1<<BOSS_TYPE), quest_flags[1] = NPC gift bits (1<<NPC_TYPE).
- boss_defeated() helper in state_world.c checks quest_flags[0] before spawning bosses.
- Antidote grants HAZARD_IMMUNITY_DURATION (300 frames, defined in player.h) — checked in state_world hazard damage block.
- npc_gift_given tracked in npc.c — use npc_get/set_gift_flags() for save persistence.
- quest_flags[2] = area visited bits (1<<AREA_ID) — triggers first-visit lore dialogue.
- BPHASE_VULNERABLE: 20-frame window after boss attack2, player deals 2x damage.
- Boss rapid-flash during vulnerability (phase_timer & 1) — telegraphs "hit me now!"
- Enemy kill: hud_notify "+NXP" (30 frames) + video_shake(2,1) for positive feedback.
- boss_init() MUST be called in load_area() — resets all boss statics (entity ptr, phase, timers).
- REG_BLDCNT/BLDY must be cleared in ALL state exit functions (gameover, win, world).
- Relay cores force-replace consumable slot if inventory full — quest items never lost.
- Equipment bonuses: Iron Sword +3 ATK (inventory_has check), Leather Armor -2 dmg (min 1).
- Enemy drops use rand_range(100) for percentage-based loot tables (type-specific).
- Pause menu has 4 options: Resume/Items/Save/Quit. Items opens inventory submenu.
- NPC dialogue branches on quest progress via npc_get_dialogue() + world_boss_defeated().
- dialogue_open() guards against overwriting active dialogue (returns silently if active).
- HUD shows "IM" indicator at col 28 row 1 when hazard immunity is active.
- Dialogue box is 6 rows tall (DBOX_Y=13, DBOX_H=6) with 4 content rows (LINES_PER_PAGE=4).
- hud_notify_clear() must be called before overlaying UI on row 9 (e.g., pause menu).
- npc_gift_given is reset on every state_world_enter (before save restore overrides it).
- Boss relay core drop has fallback: if entity pool full, adds directly to inventory.
- Conduit lock at Ashfall x=36 y=18-21: requires 3 relay cores, tracked in quest_flags[3] bit 0.
- world_set_tile() modifies both VRAM screenblock and collision map at runtime.
- Conduit unlock must be re-applied after load_area (tiles regenerated from scratch each load).
- All areas have bidirectional transitions (doorways opened in left walls at x=0-1, y=18-21).
- inventory_use() calls hud_notify internally for antidotes — callers should NOT add their own.
- Slash arc uses OBJ tile 92 (single 8x8), player palette bank 0. Auto-freed after SLASH_DURATION.
- floattext_clear() must be called on area transitions (load_area does this).
- Camera look-ahead lerps at 64 units/frame to prevent jarring jumps on direction change.
- Floating damage numbers render on BG0 rows 3-12 only (avoids HUD rows 0-2 and dialogue 13+).
- Enemy alt frame tiles: Skeleton 93-96, Crawler 97-100, Spore 101-104, Construct 105-108, Slime squash 109-112.
- Boss palette manipulation in boss_draw: attack telegraph (brighten), vulnerability (golden tint). Auto-restored via dirty check on phase transition.
- Slime death uses anim_frame=0xFF as flag for squash sprite — draw function checks this to select SLIME_SQUASH_BASE tile.
- BG2 parallax: SBB30/CBB2, tile 1 filled, palette bank 6. world_load_parallax() loads per-area tile+palette.
- Water palette animation: swaps BG palette indices 10+11 every 16 frames in state_world_draw (ambient_timer).
- Conduit pulse: modifies pal_bg_mem[5*16+3] with triangle wave brightness in state_world_draw.
- Area name display excludes BG0 from blend (BLD_TOP without BLD_BG0) so text stays bright on dark background.
- ambient_timer reset to 0 in load_area() — prevents stale palette state on area transitions.
- hud_load_gfx() called once in main.c after text_init() — loads HP bar tiles (96-100), dialogue border tiles (101-109), and palettes (bank 1=gold, 2=dialogue, 7=HP).
- HP bar uses direct screenblock writes (map[0*32+8+i]) with HUD_PAL_HP palette bits — not text_print.
- Dialogue border uses HUD_DBOX_* tile IDs with HUD_PAL_DBOX_BITS — defined in hud.h for cross-module use.
- Speaker name highlight: dialogue_draw detects [Name] pattern at page_start, applies HUD_PAL_GOLD_BITS to those chars.
- BG0 CBB0 tile layout: 0-94=font, 96-100=HP bar, 101-109=dialogue border.
- Title screen subtitle typed character-by-character via text_put_char() at 3 frames/char — uses subtitle_pos/subtitle_timer statics.
- Save preview on title uses save_read() result — shows area name from title_area_names[] + "Lv" + level.
- Game over cursor menu: has_save gates between "Continue from save"/"Return to title" cursor menu vs simple "Press START" blink.
- Win screen captures final_level/final_bosses/final_areas BEFORE save invalidation in state_win_enter().
- Win screen flow: Epilogue → Credits → Stats → THE END. Credits skip (START) goes to Stats, not THE END.
- audio_fade_music(frames) starts linear volume fade; audio_update_fade() must be called every frame in main loop.
- audio_play_music() resets fade state (fade_remaining=0) and restores mmSetModuleVolume(1024) before starting new track.
- SFX_STEP plays on walk anim_frame toggle (every 16 frames), only when anim_frame==0 (one step per full cycle).
- SFX_CONDUIT_PULSE plays when pulse==3 (peak brightness) in conduit barrier animation.
- SFX_LOW_HP replaces SFX_HIT for low HP warning — heartbeat double-thud sound.
- Enemy spawn variety: 2-3 slots per area use rand_range() to pick from area-appropriate enemy types. rng.h included in state_world.c.
- Hidden items: item_drop_spawn() called in each area's spawn function for exploration rewards (herbs, antidotes at off-path positions).
- Boss damaged palette: boss_base_color() applies red/dark tint when HP <= 50%. All palette effects (brighten, tint, restore) use boss_base_color(gfx->palette[i]) as base instead of raw palette.
- Pause minimap: area_short[] (3-char abbreviations), quest_flags[2] visited bits, current area shown with brackets. Drawn at row 13-14 below menu options.
- World tile count: WTILE_COUNT=133 (indices 0-132). Tiles 0-30 are base/shared; 31-59 Millhaven; 60-77 Fen; 78-97 Thornridge; 98-114 Moonstone; 115-132 Ashfall.
- fill_rect(x0,y0,w,h,tile,pb,col) helper in world.c for solid rectangles — eliminates for loops.
- Each area's tile-loader (load_millhaven_tiles etc.) loads enrichment tiles as well as base tiles. All tiles for an area must be loaded before map generation for that area.
- enemy_reset_grace_timer() sets 60-frame grace period — enemies won't enter ESTATE_CHASE. Called from load_area().
- Default spawn_y changed from 100 to 160 — puts player on cobblestone path at village center.
- No enemy should spawn within 80px of any area entry point (player spawn or transition spawn).

## Current Phase
Complete 12-phase overhaul done. ~311KB ROM, zero warnings.
Looter RPG systems: 8-byte Item struct, 5 rarity tiers, procedural item generation,
equipment system (3 slots: weapon/armor/accessory) with stat aggregation, combat depth
(weapon types, crits), gold currency (enemy/boss drops, NPC shops), boss quest rewards
(5 named drops), expanded 256-byte SaveData, equipment UI with stat comparison, rarity-
colored text (BG palette banks 8-11), Harvest Moon warm palette overhaul.
NPC shops: Herbalist sells herbs (5g) and antidotes (10g), Alchemist sells random gear (50g).
Guard gives starter weapon. Equipment-aware NPC dialogue with gameplay tips.
HUD shows: ATK, DEF, herbs, cores, gold, weapon rarity tag, immunity indicator.
Full quest loop: explore > loot > equip > defeat bosses > collect cores > unlock conduit > win.

## Session Log
- 2026-02-28: Scaffold created, all engine modules, build system, headless test passing.
- 2026-02-28: Added player rendering, enemy system, combat, world tiles, camera, HUD.
- 2026-02-28: Created story design doc (Ashen Circuit). Added dialogue, NPC, inventory,
  item drops, XP/leveling, game over state. Bug audit and fixes. 62KB ROM, 0 warnings.
- 2026-02-28: Multi-area system (Millhaven hub, Verdant Fen, Thornridge Mountains). SRAM
  save/load wired to game state. Area transitions with auto-save. Title screen continue.
  New tile types (fen: swamp/ruin/poison, mountain: cliff/snow/spikes). 68KB ROM.
- 2026-02-28: Phase 5 (Moonstone Caverns, 3 new enemy types, Gloomfang boss). 76KB ROM.
- 2026-02-28: Comprehensive audit & optimization pass. Fixed: OAM sprite leak on area
  transitions, boss double-update via enemy_update_all, 64x32 world map SBB addressing,
  VRAM byte-write bugs (memset→memset16), camera lerp jitter (snap threshold), knockback
  oscillation (clamp-to-zero friction), diagonal speed 41% too fast (181/256 normalization),
  boss lunge undodgeable (lock direction on first frame), attack2 hitbox 1-frame window
  (→3-frame), enemy despawn use-after-free, inventory stacking overflow. Optimizations:
  gfx_data.h static→extern (eliminated ~30KB duplication), DMA3 OAM copy, multiply-and-shift
  RNG (no division), entity high-water mark iteration, LTO enabled, -ffast-math removed,
  shared state_ids.h, save_write auto-checksum, HUD redundant iteration removed. 75KB ROM.
- 2026-02-28: Ashfall Citadel (final area). New enemies: Construct (marching automaton),
  Wisp (erratic homing flame). New bosses: Voss (fast swordsman), Siphon (stationary machine).
  6 new tile types (ash_floor/wall, conduit, gear, blight_wall, energy_floor). Three-section
  map: Outer Wall, Inner Keep, Nexus Chamber (boss arena). NPC_ALCHEMIST dialogue. 80KB ROM.
- 2026-02-28: Wired Siphon as two-phase final boss (Voss→Siphon sequence in Ashfall Citadel).
- 2026-02-28: Second audit pass. Critical: fixed text_clear_all 8-bit VRAM write (→memset16),
  headless_check gated behind mgba_ok (was calling Stop() on real hardware after 30 frames).
  High: added player tile collision (axis-separated) + world bounds clamping, fixed
  siphon_pending not reset on state re-entry, was_defeated persistence across frames.
  Performance: entity_get_high_water() exposed and used in all iteration loops (enemy, npc,
  item_drop, contact damage), entity_despawn shrinks high-water mark, rand_range() replaces
  modulo, >>3 replaces /8, &7 replaces %8. Cleanup: OAM_NONE constant replaces magic 0xFF,
  removed redundant obj_hide calls, dialogue blink uses u32 mask, audio_update moved after
  VBlank, boss HP bar cleared on defeat. 81KB ROM.
- 2026-02-28: Win/credits state (STATE_WIN). Three phases: epilogue (story conclusion),
  scrolling credits, THE END screen with return-to-title. Siphon defeat triggers win_pending
  flag → dialogue closes → STATE_WIN. SRAM save invalidated on win (fresh start). 83KB ROM.
- 2026-02-28: Third audit pass. Bugs fixed: player attack multi-hit (added per-attack
  generation counter in Entity.last_hit_id), inventory full-stack blocking new slot (break
  instead of return 0), uninitialized SaveData in state_win (memset to zero). Code quality:
  consolidated extern game_request_state into state_ids.h (removed from 5 .c files), replaced
  hardcoded contact damage switch with enemy_get_contact_damage() lookup, fixed text_print_int
  INT_MIN undefined behavior (unsigned negation). New features: enemy tile collision with
  axis-separated resolve + world bounds + patrol direction reversal on wall hit
  (enemy_post_move_all). Performance: dialogue per-char rendering replaced text_print() calls
  with inline text_put_char() direct VRAM write, text_clear_rect uses memset16 per row. 83KB ROM.
- 2026-02-28: Real audio assets. Python script (tools/generate_audio.py) procedurally generates
  9 MOD tracker music files and 14 WAV sound effects. audio.c rewritten with Maxmod calls,
  lookup tables mapping MUS_/SFX_ enums to soundbank constants. Makefile updated with AUDIO
  paths, absolute AUDIOFILES resolution, and bin2o pattern rule for soundbank.bin.
- 2026-02-28: Fourth audit + optimization pass. Critical bugs fixed: boss 12x damage (added
  per-attack hit dedup via last_hit_id), hazard tiles now deal damage (poison/spikes/energy),
  area transitions widened to tx=62 tw=2 (tile 63 unreachable due to bounds clamp), boss tile
  collision implemented (boss_post_move). BG2/BG3 parallax disabled (no tile data = garbage).
  Performance: IWRAM_CODE on collision_tile_at/point_solid/point_hazard/collision_aabb/
  entity_update_all, collision ty*64 via <<6 shift, sprite_alloc O(1) free-hint, entity_spawn
  O(1) free-hint with despawn hint update, enemy_draw_all lookup table replaces 7-way switch,
  HUD herb count deferred after dirty check. Entity struct: removed unused state/anim_id fields,
  last_hit_id upgraded u8→u16 (attack_gen too), oam_index moved for better packing (24→22 bytes).
  Audio: wired all 14 SFX (heal on herb, dialogue on open, menu_select on title/gameover/win,
  transition on area change). Dead code removed: world_load_test_map, world_get_collision_map/
  width/height, boss_is_active, ENT_PROJECTILE, TILE_ONEWAY/TILE_LADDER. ~166KB ROM.
- 2026-02-28: Fifth audit + optimization pass. Performance: removed dead BG2/BG3 scroll register
  writes (4 MMIO/frame saved), boss HP bar dirty check (skip ~20 VRAM writes when HP unchanged),
  dialogue incremental rendering (only new chars written to VRAM), blink text dirty tracking in
  title/gameover/win (skip VRAM writes 29 of every 30 frames), merged enemy_update_all +
  enemy_post_move_all into single entity pass (eliminates duplicate iteration). IWRAM: enemy_ai_update
  + enemy_check_player_attack promoted to ARM mode. Code quality: extracted shared collision_rect_solid
  (IWRAM) replacing 3 duplicate 4-corner checks in player/enemy/boss, enemy_spawn reuses enemy_gfx[]
  table (eliminated 7-way switch), boss_load_gfx uses BossGfxInfo table (eliminated 5-way switch).
  Bug fix: HUD herb count now always scanned (was stale when only herbs changed). ~166KB ROM.
- 2026-02-28: Sixth audit pass (diminishing returns — codebase well-optimized). Performance: spore/wisp
  patrol speed division replaced with >>1 shift, crawler rush speed *2 replaced with <<1 shift, dialogue
  page advance clears content area only (skip border redraw), dialogue blink arrow uses text_put_char
  with dirty check + bounds guard (was text_print every frame). Code quality: moved dialogue blink
  counter from static-local to file scope. ~166KB ROM.
- 2026-02-28: Gameplay polish pass. Combat feel: screen shake on player damage (6-frame 2px, 15-frame
  3px on death), 8-frame attack cooldown after swing (prevents spam), low HP warning beep at <=25% HP,
  HP bar uses '*' at low HP, death animation delay (30 frames flicker before gameover). Feedback:
  "LEVEL UP!" notification for 90 frames on level, "SAVED" notification on auto-save, relay core
  counter on HUD (Cores: N/4), hud_notify() system for centered blinking messages. Balance: Voss
  HP 35→50, Siphon HP 50→70 + dmg 6→7, boss hitbox 28→30px, Spore HP 4→6 + dmg 3→4, Wisp HP 3→5
  + dmg 3→4, Construct HP 10→14 + dmg 5→6, herb healing 5→10 HP. UX: pause menu via START (Resume/
  Save/Quit with cursor navigation), area transition fade (REG_BLDCNT fade-to-black) with area name
  display on entry. ~167KB ROM.
- 2026-02-28: Seventh audit — bug fix + persistence pass. State machine fixes: prevented pause
  during death animation (PSTATE_DEAD guard), game_request_state honored during fade transitions,
  moved START pause check after dialogue handling (allows dialogue to close naturally). Persistence:
  quest_flags[16] array tracks boss defeats (flags[0] bitfield) and NPC gift state (flags[1]),
  saved/restored in SRAM SaveData.flags[]. Defeated bosses don't respawn on area re-entry. Voss
  defeat with prior save correctly spawns Siphon directly. NPC item giving: Herbalist gives 2
  antidotes on first interaction (npc_gift_given bitfield). Antidote now functional: grants 300
  frames (5 sec) hazard immunity via player_set_hazard_immunity(), blocking poison/spike/energy
  tile damage. Boss HP bar cleared before Siphon spawn (prevents stale Voss text). ~167KB ROM.
- 2026-02-28: Player experience polish pass. Feedback: item pickup notifications ("Herb +1",
  "Relay Core!") via hud_notify, herb use feedback ("HP +10"/"No herbs!"/"HP full!"), antidote
  use shows "Immune!", enemy kill shows "+NXP" notification with 2-frame screen shake. Combat:
  boss vulnerability windows (BPHASE_VULNERABLE, 20 frames after attack2, 2x damage, rapid
  flash visual), damage-scaled screen shake (intensity = 1+dmg/2, frames = 4+dmg, capped),
  low HP beep fires immediately on threshold + faster interval (24 frames vs 40). UI: NPC
  dialogue prefixed with "[Speaker]" name tags, HUD always shows herb count + relay core
  counter (even when 0), area name display extended to 90 frames. Immersion: area lore
  dialogue on first visit (tracked in quest_flags[2] bitfield, 5 unique lore texts). ~168KB ROM.
- 2026-02-28: Production quality pass. CRITICAL fixes: REG_BLDCNT/BLDY cleared in
  state_gameover_exit + state_win_exit (dark overlay persisted to title screen), attack_gen reset
  in player_init (stale generation counter), boss_init() resets all boss statics on area load,
  HUD dirty check now tracks max_hp (level-up stat gains were invisible until HP changed).
  GAME-BREAKING fix: relay cores can no longer be lost — quest items force-replace consumable
  slot if inventory is full. Equipment system wired: Iron Sword adds +3 ATK (checked via
  inventory_has in player_get_attack), Leather Armor reduces incoming damage by 2 (checked in
  player_take_damage, minimum 1 dmg). Item drop variety: type-specific loot tables replace
  flat 33% herb — Spores drop Antidote 25% + Herb 20%, Constructs drop Iron Sword 10% + Herb
  30%, Skeletons drop Leather Armor 8% + Herb 25%, others default 33% Herb. Uses rand_range(100)
  for percentage-based drops. ~168KB ROM.
- 2026-02-28: UX polish + bug fix pass. Inventory UI: pause menu now has 4 options (Resume/
  Items/Save/Quit), Items submenu lists all inventory with cursor navigation + A to use
  consumables from menu, equipment shows [Equip] tag. Dialogue branching: all 6 NPCs now
  have quest-aware dialogue — Elder tracks relay core count (0/partial/all 4), Guard/Herbalist
  update after Rotmaw, Miner after Ironhusk, Explorer after Gloomfang, Alchemist after Voss.
  npc_get_dialogue() selects from default/post-boss variants using world_boss_defeated().
  Bug fixes: double SFX_DIALOGUE on NPC interaction (dialogue_open already plays it),
  inventory_add break→continue (search all slots of same type for stacking space), XP bar
  now uses ceiling division (matches HP bar), save load loop uses INVENTORY_SIZE not hardcoded
  16, dialogue_open guards against overwriting active dialogue, header comments corrected
  (herb heals 10 not 5, sword +3 not +2). HUD: hazard immunity "IM" indicator when antidote
  active, immunity tracked in dirty check. ~171KB ROM.
- 2026-02-28: Production hardening pass (Phase 18). CRITICAL: dialogue box expanded DBOX_H 4→6 (DBOX_Y
  15→13) giving 4 content rows — fixes silently clipped 3-line dialogue pages (Elder quest
  instructions, Alchemist boss guidance were truncated). hud_notify cleared on pause entry
  via hud_notify_clear() — prevents notification text corrupting pause menu at row 9. Relay
  core drop safety: boss death falls back to inventory_add if entity pool full (quest items
  never lost). npc_gift_given reset to 0 on every state_world_enter (fixes Herbalist never
  giving antidotes on new game after completed run). item_cursor clamps to 0 when inventory
  fully emptied from pause menu. Pause menu herb use shows specific feedback ("HP +10" /
  "HP full!" / "Immune!") instead of generic "Used!". Item drops show "Full!" when inventory
  can't accept. ~171KB ROM.
- 2026-02-28: Relay core conduit lock mechanic (Phase 19). Nexus Chamber entrance in Ashfall
  (x=36) is now blocked by solid conduit tiles. Player must collect 3 relay cores from area
  bosses (Rotmaw, Ironhusk, Gloomfang), then press B near the conduit barrier to consume
  cores and open the path. Not enough cores shows "needs 3 cores" dialogue. Unlock state
  tracked in quest_flags[3] bit 0, persisted in SRAM save. world_set_tile() API added for
  runtime tile/collision modification. Conduit restored on save-load and area re-entry.
  Relay cores now have a real mechanical purpose — the game's core quest loop is complete:
  explore areas → defeat bosses → collect cores → unlock Nexus → defeat Voss → defeat
  Siphon → win. ~172KB ROM.
- 2026-02-28: Final polish pass (Phase 20). Back-transitions fixed: opened doorways in left
  walls of Thornridge, Moonstone, and Ashfall (solid walls at x=0-1 blocked tx=0 transitions).
  Players can now backtrack to earlier areas for grinding. Double hud_notify on antidote from
  pause menu fixed (inventory_use already handles notification). Elder dialogue updated to
  clarify "3 cores open the Nexus, 4th completes the Circuit." Alchemist dialogue now
  explicitly tells player to "press B to override" the conduit barrier. Inventory menu: quest
  items (relay cores, equipment) show "Quest item!" when A is pressed instead of silent
  failure. ~172KB ROM.
- 2026-02-28: Code cleanup pass (Phase 21). Removed dead first_enter variable and unreachable
  else branch in state_world_enter (save check always runs, exit always reset it). Extracted
  HAZARD_IMMUNITY_DURATION constant (300 frames) from hardcoded value in inventory.c to
  player.h define. sprite_free() now guards against double-free and updates next_free_hint
  for better O(1) alloc locality. Deep audit confirmed no remaining critical/high issues —
  prior "missing boss_get_type" finding was false positive (functions exist at boss.c:500).
  ~185KB ROM (includes audio soundbank).
- 2026-02-28: Player feel polish — Phase A (Phase 22). A1: Invincibility flash now uses
  fast 2-frame flicker for first 20 frames, slow 8-frame flicker for remaining 40 —
  telegraphs "almost over" to player. A2: Attack slash arc — 8x8 white arc OBJ sprite
  spawns in front of player during PSTATE_ATTACK (8 frames, auto-freed). Uses tile 92,
  player palette bank 0 (index 7 = white). A3: Floating damage numbers — new floattext
  module (floattext.c/h) shows damage dealt above enemies/bosses as rising text on BG0.
  Up to 4 simultaneous floats, 24-frame duration, blinks in last 8 frames. Wired into
  enemy_check_player_attack and boss_check_player_attack. A4: Camera improvements —
  dead zone tightened 24x16 → 16x12, lerp speed doubled 1/8 → 1/4, 16px look-ahead
  in player facing direction (smooth lerp transition on direction change). ~192KB ROM.
- 2026-02-28: Enemy & boss animation polish — Phase B (Phase 23). B1: All 7 enemy types
  now have idle animation — added alt frames for Skeleton, Crawler, Spore, Construct (tiles
  93-112). EnemyGfxInfo table uses tile_alt_ofs for frame switching at anim_frame>=16.
  B2: Type-specific death effects — slimes show squash frame on death (tile 109), all
  enemies have improved death flicker (slow 4-frame at start, fast 1-frame at end).
  B3: Boss attack telegraph — palette brightened toward white during first 8 frames of
  BPHASE_ATTACK1/ATTACK2 windup (alternating frames). B4: Boss vulnerability palette
  glow — BPHASE_VULNERABLE now uses golden palette tint (pal_tint toward 31,28,8) instead
  of simple blinking, making the "hit me now" window more visually distinct. Palette auto-
  restored on phase transition via dirty check. ~192KB ROM.
- 2026-02-28: World & environment polish — Phase C (Phase 24). C1: Water palette
  animation — swaps BG palette indices 10 and 11 every 16 frames via ambient_timer
  in state_world_draw. Affects Millhaven water, Fen poison, Cavern cave water, and
  Citadel energy floor. Zero CPU cost. C2: Parallax background — BG2 enabled with
  per-area 1-tile repeating pattern (stars/clouds/haze/embers/dots) at half camera
  scroll speed. Uses SBB30/CBB2, palette bank 6. world_load_parallax() in world.c.
  C3: Area entry text improved — area name displayed centered at rows 7-9 with
  decorative dashes, BG0 excluded from blend so text stays bright against darkened
  world. Text cleared on fade-in start. C4: Conduit barrier pulse — palette index 3
  in citadel palette bank cycled with triangle wave brightness (0-3-0 over 32 frames).
  Stops pulsing after conduit unlock. ~192KB ROM.
- 2026-02-28: UI & HUD polish — Phase D (Phase 25). D1: Graphical HP bar — replaced
  ASCII [=======] with custom tile graphics (full/half/empty segments). Uses BG palette
  bank 7 with green fill that switches to red at <=25% HP. Tiles 96-100 in CBB0.
  D2: Dialogue box border — replaced ASCII +--+ box with 9-tile graphical border
  (4 corners, 4 edges, 1 dark fill). Uses BG palette bank 2 with light border + dark
  tinted fill. Tiles 101-109 in CBB0. draw_box() now writes directly to screenblock.
  D3: Speaker name highlight — [Speaker] tags in dialogue rendered with BG palette bank
  1 (gold/yellow text) instead of white. dialogue_draw detects [Name] prefix at page
  start and applies gold palette bits to those characters. D4: Item pickup popup — item
  pickups now spawn a rising "+1" floattext at the pickup world position alongside the
  existing hud_notify text. hud_load_gfx() called once at startup to load all UI tile
  graphics and palettes. ~192KB ROM.
- 2026-02-28: Title & end screens polish — Phase E (Phase 26). E1: Title screen ASCII
  art logo — replaced plain "ASHEN CIRCUIT" with multi-line ASCII art (rows 2-9), typed
  subtitle revealing character-by-character at 3 frames/char (row 11). E2: Save file
  preview — title screen shows area name + level below Continue prompt when save exists.
  E3: Game over retry with stats — death screen shows level, area name, bosses defeated
  count. If save exists, cursor menu offers "Continue from save" (→STATE_WORLD) or
  "Return to title". E4: Win screen final stats — new WIN_STATS phase between credits
  and THE END. Captures player level, boss defeat count, and area count before save
  invalidation. Stats displayed for 5 seconds or skipped with A. Credits skip (START)
  now goes to stats instead of straight to THE END. ~192KB ROM.
- 2026-02-28: Audio polish — Phase F (Phase 27). F1: Music composition rewritten —
  all 9 tracks now use 4-8 patterns (up from 1-2), with velocity variation (Cxx volume
  effect), proper drum patterns on channel 4 (kick/snare/hihat via noise pitch), distinct
  per-area character (title: atmospheric arpeggios, fen: dissonant tritones, citadel:
  industrial pulse, boss: relentless double-time climax, caverns: echo delays). Helper
  functions drum_rock/drum_fast/drum_sparse/drum_march for reusable patterns. F2: Three
  new SFX added — SFX_STEP (quiet footstep on walk frame toggle, every 16 frames),
  SFX_CONDUIT_PULSE (electric hum at conduit barrier peak brightness), SFX_LOW_HP
  (heartbeat double-thud replacing SFX_HIT beep). SFX enum expanded to 17. F3: Music
  fade on area transition — audio_fade_music(frames) uses mmSetModuleVolume() linear
  fade to 0, audio_update_fade() called per frame in main loop. Fade triggers on area
  transition start (FADE_OUT_FRAMES). audio_play_music() resets fade state and restores
  full volume. ~183KB ROM.
- 2026-02-28: Gameplay depth — Phase G (Phase 28). G1: Enemy spawn variety — each area
  now has 2-3 randomized spawn slots using rand_range(). Millhaven can spawn bats,
  Thornridge can spawn crawlers/wisps, Fen varies slime/skeleton/spore, Moonstone adds
  wisps, Ashfall varies constructs/skeletons/crawlers. Same total enemy count per area
  but different composition each visit. G2: Hidden items — 1-2 item pickups placed in
  each area at off-path locations (herbs, antidotes). Rewards exploration. G3: Boss
  damaged palette — boss_base_color() applies red/dark tint (increased red, decreased
  green/blue) when HP <= 50%. All palette effects (attack telegraph, vulnerability glow)
  layer on top of the damaged base. Dirty check compares against expected base color.
  G4: Pause screen minimap — 5-area row display below menu options shows visited areas
  as 3-letter abbreviations (MLH/FEN/MTN/CAV/ASH), unvisited as "...", current area
  in brackets. Uses quest_flags[2] visited bits. ~197KB ROM.
- 2026-03-01: Second playtest bug fix pass (Phase 30). One bug fixed. MEDIUM: title screen
  subtitle ("The blight spreads. The Circuit falters." — 40 chars) overflowed past screen
  col 30 via text_put_char (no bounds check); chars 32-39 wrapped to row 12 cols 0-7,
  printing "alters." visibly on screen below the subtitle. Fixed with tx<30 guard. Also
  audited floattext, enemy AI, npc, hud, dialogue, state_title/gameover/win — no further
  bugs found. Notable: text_print has bounds check; text_put_char does not (GBA VRAM
  screenblock is 32 wide; cols 30-31 off-screen; col 32+ wraps to next row). ~197KB ROM.
- 2026-03-01: Playtest bug fix pass (Phase 29). Three bugs fixed. CRITICAL: quest_flags
  were zeroed before load_area() during save-load in state_world_enter — boss_defeated()
  and area-lore checks saw all-zero flags so every previously defeated boss respawned and
  area lore re-triggered on every continue. Fix: restore quest_flags from sd.flags and
  call npc_set_gift_flags() BEFORE load_area(). Removed now-redundant post-load conduit
  check. MINOR: pause menu resume clear (text_clear_rect) was cols 8-21 but minimap spans
  cols 5-24 — ghost text remained at edges after resuming. Fixed both resume paths to
  clear cols 5-24 (width 20). MINOR: fade-in cleanup block (if remaining==0) was dead code
  since remaining is always ≥1 inside the fade_timer>0 guard — fixed to check fade_timer==0
  so REG_BLDCNT is properly cleared after fade ends. ~197KB ROM, zero warnings.
- 2026-03-01: World visual enrichment — Phases 31-33 (map detail pass). Added 103 new tile
  types across all 5 areas (WTILE_COUNT: 8 → 133). Phase 31A: shared edge/transition tiles
  (grass corners, stone edges, water edges, path tiles). Phase 31B: Millhaven village tiles
  (houses via draw_house(), fences, well, lantern, market stall, crate, barrel, sign, cobble,
  flower, bridge, garden, stone staircase, water crossing). Phase 32A: Verdant Fen enrichment
  (poison top, swamp edges, fen bridge over pool 1, lily pads in pools 2-3, mushrooms, roots,
  swamp grass, vine walls on ruins, ruin arch spanning temple, ruin pillars, moss stone, ruin
  floor in hollow, shrine, skull, overgrown path). Phase 32C: Thornridge enrichment (snow
  edges, cliff edges at summit, snow drifts, ice patch on high platform, mine beams/walls/
  lanterns, cave entrance arch, scaffold+chain, boulders, rock piles, mtn path, pine trees,
  mine cart on rails, summit flag). Phase 33A: Moonstone Caverns enrichment (cave edges,
  crystal arch in grotto ceiling, crystal cluster + glowpool + crystal floor inside grotto,
  crystal pillar, bridge stone over pools, waterfall, fungus near pools, drips from stalactites,
  cave moss on walls, rubble, cave column, explorer camp with tent/fire/book pile). Phase 33C:
  Ashfall Citadel enrichment (citadel edges, pipe network H/V/corner extending conduit system,
  vents, panels, structural pillars with base+top, banners, rubble ash, blight cracks, gate
  frame around conduit barrier, throne for Siphon, circuit nodes at junctions, dark windows,
  chains hanging from nexus ceiling, glowing platforms in nexus arena). fill_rect() helper used
  for bridge, crystal floor, and platform sections to eliminate unused-function warning. ~296KB
  ROM, zero warnings. All 5 areas now have distinct, detailed visual environments.
- 2026-03-01: Release polish pass (Phase 34). Comprehensive UI/display audit and fix pass
  targeting 30x20 tile GBA screen. Title screen: replaced overflowing CIRCUIT ASCII art
  (33-35 chars, clipped at col 30) with styled spaced text "-=  C I R C U I T  =-" (21
  chars, fits perfectly). Subtitle shortened from 40 to 30 chars. HUD: HP numeric display
  reformatted as tight "hp/max" buffer (no gap at single-digit HP). Minimap: changed spacing
  from 4-col to 5-col per area, fixed bracket overlap with abbreviation text. Stats screens:
  aligned gameover/win stat values to consistent column (16), deduplicated win stats drawing
  into draw_stats_page() helper. Credits: recalculated all 17 credit line positions for true
  center on 30-col screen (were shifted 1-2 cols right). "ASHEN CIRCUIT" on THE END screen
  centered (col 7→8). Dialogue: replaced UTF-8 em-dash in Alchemist dialogue with ASCII "--"
  (em-dash rendered as 3 blank tiles). Conduit SFX: fixed 4-frame stutter by adding
  (ambient_timer & 3)==0 edge guard (pulse==3 lasts 4 frames due to >>2 division). Boss
  relay core: added hud_notify("Relay Core!", 60) on entity-pool-full fallback path (was
  silent direct inventory add). Comment fix: hud.h notification row 10→9. ~302KB ROM, zero
  warnings.
- 2026-03-01: Hardening & combat feel polish (Phase 35). A: Attack cooldown no longer roots
  player — PSTATE_ATTACK_COOLDOWN now falls through to movement input (can reposition but not
  re-attack), removes 0.13s forced immobility per swing. B: Hazard immunity "IM" indicator
  blinks on 4-frame cycle during last 60 frames (1 second warning before expiry). C: text_put_char
  gains unsigned bounds check (tx<30, ty<20) — prevents VRAM corruption from out-of-range
  writes. D: audio_update_fade division guard — fade_total<=0 triggers clean stop instead of
  divide-by-zero. E: SRAM save_write wraps byte-write loop in REG_IME disable/restore for
  interrupt safety during Maxmod timer IRQs. F: Fixed divide-by-zero in Maxmod during mmStart —
  build_mod() was accepting bpm parameter but never writing it as Fxx effect in MOD pattern data.
  Maxmod/mmutil defaulted to BPM=0 internally, causing BIOS division-by-zero on every music
  start. Fix: inject BPM as F effect on row 1 of first pattern alongside speed on row 0.
  ~193KB ROM (clean rebuild — prior ~302KB included stale build artifacts), zero warnings.
- 2026-03-01: Playability overhaul (Phase 36). Three critical playtest issues fixed. A: Player
  spawn moved from (120,100) to (120,160) — now on cobblestone path at village center instead
  of empty space above village. B: Enemy spawn distances audited for all 5 areas — no enemy
  within 80px of any entry point. Millhaven: moved (130,80)→(400,80) and (200,104)→(240,140).
  Fen: moved (60,168)→(120,140). Thornridge: moved (60,144)→(140,144). Ashfall: moved
  (60,168)→(140,168) and (50,120)→(120,90). C: Attack cooldown animation state machine fixed —
  animation state update now guarded with `if (player_state != PSTATE_ATTACK_COOLDOWN)` to
  prevent PSTATE_WALK overwriting cooldown when player moves during cooldown period. D: Added
  60-frame (1 second) spawn grace period via enemy_reset_grace_timer() — enemies won't enter
  ESTATE_CHASE after area load, preventing instant aggression after area transitions, lore
  dialogues, and save loads. Grace timer decremented in enemy_update_all(), gates ESTATE_CHASE
  transitions in enemy_ai_update(). ~193KB ROM, zero warnings.
- 2026-03-01: Real playtest bug fix pass (Phase 36B). Four critical issues from mgba-qt testing.
  A: Player sprite HFLIP inverted — base sprite faces LEFT by default (eye at pixel 2), but code
  applied HFLIP when facing_left. Fixed: `if (!facing_left)` for both player sprite and slash arc.
  B: Map mostly dots — old side-scrolling fill_ground() approach left top 60% of map as transparent
  tiles (index 0 = transparent on GBA). This is a TOP-DOWN game, not side-scrolling. Fixed: all 5
  areas now fill entire 64x32 map with walkable ground tiles (grass/swamp/snow/cave/ash) + solid
  border walls. Removed dead fill_ground() function. C: Dialogue box fill nearly invisible —
  RGB15(2,2,6) too dark. Fixed to RGB15(3,3,12). D: Player stuck after dialogue — cobblestone path
  tiles at y=20-21 were TILE_SOLID, and player spawned at (120,160) = tile y=20. Collision reverted
  all movement. Fixed: cobblestone, path tiles, bridges, ruin floor/stairs all changed to TILE_EMPTY.
  Also: removed duplicate hazard placements in Fen/Ashfall from new base fill code, fixed Thornridge
  spike pits from 6-tall columns to 3x2 patches. ~193KB ROM, zero warnings.
- 2026-03-01: Complete looter RPG overhaul (12 phases). Phase 0: Bug fixes (sprite artifacts,
  Fen doorway, dialogue readability). Phases 1-4: Item data model (8-byte Item struct, 5 rarity
  tiers, prefix/suffix name tables), procedural item generation (item_gen.c with area-scaled
  stats and rarity rolls, boss named drops), equipment system (3 slots, stat aggregation, swap
  mechanics), loot table overhaul (25% gear drop rate, rarity-colored gem sprites, gear drop
  entity tracking). Phase 6: Combat depth (weapon types sword/heavy/light, critical hits 5%+
  equipment bonus, equipment-based player stats). Phase 5: Equipment UI (pause menu equip screen
  with rarity-colored names, stat comparison +N/-N, scrolling inventory). Phase 7: Save system
  expansion (256-byte SaveData with full Item inventory, equipped items, quest flags). Phase 8:
  HUD overhaul (ATK/DEF from equipment, rarity-colored weapon tag, herb/core counts). Phase 9:
  Harvest Moon palettes (warm earth tones across all 24 palette arrays — player, enemies, NPCs,
  all 5 area BGs, parallax, UI). Phase 10: Tile art improvements (denser grass/water/tree tiles,
  player sprite hair highlights and shirt detail). Phase 11: Story & NPC depth (gold currency
  from enemy/boss kills, Herbalist shop sells herbs 5g/antidotes 10g, Alchemist shop sells
  random gear 50g, Guard gives starter weapon, equipment-aware NPC dialogue with gameplay tips,
  gold on HUD and pause menu, gold saved/loaded in SRAM). ~311KB ROM, zero warnings.
- 2026-03-01: Visual & gameplay depth pass (Phase 38). A: Circuit board backgrounds — title,
  gameover, and win screens now have animated scrolling circuit board BG1 backgrounds using 6
  menu tiles (dark/trace_h/trace_v/node/corner/cross) + area-specific palettes (blue/teal for
  title+win, red/amber for gameover). Slow diagonal scroll in draw functions. B: Dialogue box
  palette fix — indices 2 and 4 were identical RGB15(3,3,10), shadow effect invisible. Fixed:
  brighter fill RGB15(4,4,13), darker shadow RGB15(1,1,6). C: Sprite art improvements — player
  body tiles reworked across all 5 frames (wider shirt, double gold belt, shirt highlights,
  wider walk stride), tree canopy fuller/rounder, flower tile cream petals, boss Rotmaw vine
  tendrils in Col 2. D: NPC extern fix — removed erroneous spr_npc_idle0/pal_npc externs from
  gfx_data.h (data is static in npc.c). E: Combat balance — boss vulnerability window 20→30
  frames, weapon-type cooldown scaling (Heavy +4, Light -3 frames), slash arc positioning
  symmetrized (sx+12→sx+8 facing right), weapon-type scaling added to boss combat (was missing),
  boss crits added (5%+equipment bonus, CRIT! notify + screen shake). F: Enemy balance — HP
  increased ~25% across all 7 types (Slime 6→8, Skeleton 12→15, Bat 5→7, Crawler 7→9, Spore
  6→8, Construct 14→18, Wisp 5→7), detection ranges increased 8-12px each for more engaging
  encounters. G: Antidote duration reduced 300→180 frames (5s→3s) for more tactical use. H:
  Off-screen sprite culling — enemies, NPCs, and item drops hidden when outside visible screen
  area (saves OAM bandwidth). ~249KB ROM, zero warnings.
