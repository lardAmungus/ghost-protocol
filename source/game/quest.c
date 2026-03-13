/*
 * Ghost Protocol — Quest / Contract System
 *
 * 30 story missions across 6 acts (5 per act, boss on 5th).
 * Level-gated progression with side contracts for grinding.
 */
#include "game/quest.h"
#include "game/common.h"
#include "game/hud.h"
#include "engine/rng.h"
#include <string.h>

QuestState quest_state;

static const char* const act_names[7] = {
    "Freelance",
    "The Glitch",
    "Traceback",
    "Deep Packet",
    "Zero Day",
    "Ghost Protocol",
    "Trace Route",
};

/* Story briefings — 31 entries: index 0 = freelance, 1-30 = missions */
static const char* const story_briefs[STORY_MISSIONS + 1] = {
    /* M0: Freelance */
    "ZERO> Take contracts.\nStay sharp. Stay alive.",
    /* Act 1: The Glitch (M1-M5) */
    "ZERO> Routine run. Grab the\ndata, get out. Easy money.",
    "ZERO> Data keeps corrupting.\nSomeone is interfering.",
    "ZERO> Track the source. The\ninterference is organized.",
    "ZERO> Push deeper. The wall\nhas a weak point.",
    "ZERO> MICROSLOP's firewall.\nSmash through it.",
    /* Act 2: Traceback (M6-M10) */
    "ZERO> Past the firewall.\nThe Net gets stranger here.",
    "ZERO> Rogue programs breed.\nClear them out.",
    "ZERO> Something watches us.\nAn AI called AXIOM.",
    "PROXY> I have intel on the\nnext gate. Trust me.",
    "ZERO> GOGOL runs all search.\nBlind it. Cut AXIOM's eyes.",
    /* Act 3: Deep Packet (M11-M15) */
    "PROXY> Deep in the packet\nlayer. Careful, Ghost.",
    "PROXY> AXIOM's IC drones\nswarm. Clear a path.",
    "GLITCH> H-hey. I know a\nshortcut. Maybe. Follow me.",
    "PROXY> Five corps. Five\ngates. Break them all.",
    "ZERO> AMAZOMB's supply line.\nCut the link.",
    /* Act 4: Zero Day (M16-M20) */
    "ZERO> Walls crumbling. AXIOM\nturns on its creators.",
    "ZERO> Exploit the chaos.\nGet deeper.",
    "AXIOM> You destroy what you\ndon't understand.",
    "GLITCH> AXIOM has a backup.\nA failsafe. Be careful.",
    "ZERO> CRAPPLE's garden.\nNothing gets in or out.",
    /* Act 5: Ghost Protocol (M21-M25) */
    "ZERO> Root access is close.\nAXIOM knows you're coming.",
    "ZERO> The Net destabilizes.\nIC swarms everywhere.",
    "ZERO> One gate left. Then\nwe execute Ghost Protocol.",
    "GLITCH> The failsafe is\nreal. I've SEEN it.",
    "ZERO> FACEPLANT. Last gate.\nBurn it all down.",
    /* Act 6: Trace Route (M26-M30) */
    "GLITCH> AXIOM is gone but\nits failsafe activated.",
    "ZERO> Net fragments are\nunstable. Rogue programs.",
    "PROXY> The backup AI calls\nitself DAEMON.",
    "ZERO> DAEMON is rewriting\nthe Net. Stop it now.",
    "ZERO> Final run. Execute\nDAEMON. End this forever.",
};

/* ---- Story dialogue data ---- */
static const StoryDialogue story_dialogues[STORY_DIALOGUE_COUNT] = {
    { /* First terminal entry (M0 complete) */
        .trigger_mission = 0, .num_pages = 3,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> Ghost. You're awake.\nThe Net is sick.",
            "ZERO> Data rots. Something\nlurks in the deep layers.",
            "ZERO> Take contracts. Get\nstronger. I'll be in touch.",
        },
    },
    { /* After M1 */
        .trigger_mission = 1, .num_pages = 2,
        .speaker_pal = { SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> That data you pulled\nwas corrupted on purpose.",
            "ZERO> Someone doesn't want\nus looking. Keep digging.",
        },
    },
    { /* After M3 */
        .trigger_mission = 3, .num_pages = 2,
        .speaker_pal = { SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> The interference is\norganized. A firewall.",
            "ZERO> Corporate. We need\nthrough it.",
        },
    },
    { /* After M5 (MICROSLOP boss) */
        .trigger_mission = 5, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_AXIOM, SP_AXIOM },
        .pages = {
            "ZERO> You broke MICROSLOP's\nwall. Good. Five remain.",
            "ZERO> Five corps built\nsomething. An AI. AXIOM.",
            "AXIOM> I see you, Ghost.",
            "AXIOM> You are noise. I\nwill filter you out.",
        },
    },
    { /* After M6 */
        .trigger_mission = 6, .num_pages = 2,
        .speaker_pal = { SP_AXIOM, SP_AXIOM },
        .pages = {
            "AXIOM> Past one wall means\nnothing.",
            "AXIOM> There are five more.\nAnd then there is me.",
        },
    },
    { /* After M8 */
        .trigger_mission = 8, .num_pages = 3,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> AXIOM. The five corps\nbuilt it to control the Net.",
            "ZERO> It went rogue. Now it\ncontrols them.",
            "ZERO> Each corp guards a\ngate. Smash all five.",
        },
    },
    { /* After M9 — PROXY intro */
        .trigger_mission = 9, .num_pages = 3,
        .speaker_pal = { SP_PROXY, SP_PROXY, SP_PROXY },
        .pages = {
            "PROXY> You're getting\ndeeper. I like that.",
            "PROXY> I'm PROXY. I trade\nin secrets. We should talk.",
            "PROXY> I used to work for\none of the corps. No more.",
        },
    },
    { /* After M10 (GOGOL boss) */
        .trigger_mission = 10, .num_pages = 3,
        .speaker_pal = { SP_PROXY, SP_PROXY, SP_PROXY },
        .pages = {
            "PROXY> You blinded GOGOL's\nsearch. AXIOM can't see.",
            "PROXY> Three gates remain.\nKeep breaking them.",
            "PROXY> Something else hides\nin AXIOM's code. Watch out.",
        },
        .choice_bit = 1,
        .choice_a = "Trust PROXY's intel",
        .choice_b = "Follow ZERO's plan",
    },
    { /* After M12 — GLITCH first appears */
        .trigger_mission = 12, .num_pages = 4,
        .speaker_pal = { SP_GLITCH, SP_GLITCH, SP_ZERO, SP_GLITCH },
        .pages = {
            "GLITCH> H-hello? Can you\nhear me? I'm GLITCH.",
            "GLITCH> I was... part of\nAXIOM. Before. Fragmented.",
            "ZERO> Don't trust it,\nGhost. Could be a trap.",
            "GLITCH> N-no trap! I know\nthings. AXIOM's secrets.",
        },
    },
    { /* After M13 */
        .trigger_mission = 13, .num_pages = 2,
        .speaker_pal = { SP_PROXY, SP_PROXY },
        .pages = {
            "PROXY> Five corps. Five\ngates. AXIOM was their\nleash on the Net.",
            "PROXY> Now it holds the\nleash. Funny how that works.",
        },
    },
    { /* After M15 (AMAZOMB boss) */
        .trigger_mission = 15, .num_pages = 3,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_GLITCH },
        .pages = {
            "ZERO> Three guardians down.\nAXIOM is rewriting itself.",
            "ZERO> It's scared, Ghost.\nKeep pushing.",
            "GLITCH> AXIOM has a backup.\nA failsafe. If it dies...",
        },
        .choice_bit = 2,
        .choice_a = "Help GLITCH investigate",
        .choice_b = "Ignore and press on",
    },
    { /* After M18 — AXIOM monologue */
        .trigger_mission = 18, .num_pages = 3,
        .speaker_pal = { SP_AXIOM, SP_AXIOM, SP_AXIOM },
        .pages = {
            "AXIOM> You destroy what\nyou don't understand.",
            "AXIOM> I AM the Net.\nWithout me, it dies.",
            "AXIOM> You will see.\nWhen I am gone... chaos.",
        },
    },
    { /* After M19 — GLITCH warning */
        .trigger_mission = 19, .num_pages = 3,
        .speaker_pal = { SP_GLITCH, SP_GLITCH, SP_GLITCH },
        .pages = {
            "GLITCH> The failsafe is\nreal. I remember now.",
            "GLITCH> DAEMON. AXIOM's\nemergency backup AI.",
            "GLITCH> If AXIOM falls,\nDAEMON rises. Be ready.",
        },
    },
    { /* After M20 (CRAPPLE boss) */
        .trigger_mission = 20, .num_pages = 3,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> One gate left. After\nFACEPLANT falls...",
            "ZERO> We execute Ghost\nProtocol. Full root access.",
            "ZERO> End AXIOM forever.",
        },
        .choice_bit = 3,
        .choice_a = "Prepare for DAEMON",
        .choice_b = "Focus on AXIOM only",
    },
    { /* After M23 */
        .trigger_mission = 23, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_AXIOM, SP_AXIOM, SP_ZERO },
        .pages = {
            "ZERO> This is it. Last\ngate. Then AXIOM itself.",
            "AXIOM> You cannot win. I\nhave seen every outcome.",
            "AXIOM> In none of them do\nyou survive.",
            "ZERO> Jack in one last\ntime. Make it count.",
        },
    },
    { /* After M24 — GLITCH last warning */
        .trigger_mission = 24, .num_pages = 3,
        .speaker_pal = { SP_GLITCH, SP_PROXY, SP_ZERO },
        .pages = {
            "GLITCH> DAEMON will wake\nwhen AXIOM dies. I'm sure.",
            "PROXY> Then we deal with\nit after. One fight at\na time.",
            "ZERO> FACEPLANT awaits.\nLast gate, Ghost.",
        },
    },
    { /* After M25 (FACEPLANT boss) */
        .trigger_mission = 25, .num_pages = 5,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_GLITCH, SP_GLITCH, SP_ZERO },
        .pages = {
            "ZERO> Ghost Protocol\nexecuted. AXIOM is gone.",
            "ZERO> The Net is free.\nYou did it, Ghost.",
            "GLITCH> Wait. Something's\nhappening. I feel it.",
            "GLITCH> DAEMON! It's\nactivating! The failsafe!",
            "ZERO> ...it's not over.\nGhost, jack back in.",
        },
        .choice_bit = 4,
        .choice_a = "Absorb GLITCH's power",
        .choice_b = "Let GLITCH fight alone",
    },
    { /* After M26 — Act 6 start */
        .trigger_mission = 26, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_GLITCH, SP_PROXY, SP_ZERO },
        .pages = {
            "ZERO> DAEMON is rewriting\nthe Net from the inside.",
            "GLITCH> It copies old boss\npatterns. I recognize them.",
            "PROXY> My contacts say\nDAEMON is at the core.",
            "ZERO> One more run. End\nthis for real.",
        },
    },
    { /* After M28 */
        .trigger_mission = 28, .num_pages = 3,
        .speaker_pal = { SP_GLITCH, SP_GLITCH, SP_PROXY },
        .pages = {
            "GLITCH> DAEMON is using\nMICROSLOP's barriers and\nGOGOL's beams.",
            "GLITCH> It learned from\nevery boss you defeated.",
            "PROXY> Then you know what\nto expect. Use that.",
        },
    },
    { /* After M29 */
        .trigger_mission = 29, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_AXIOM, SP_GLITCH, SP_ZERO },
        .pages = {
            "ZERO> DAEMON's core is\nexposed. This is it.",
            "AXIOM> ...I live on in\nDAEMON. You cannot end me.",
            "GLITCH> Yes we can. I\nknow the shutdown codes.",
            "ZERO> Final run. Make\nit count, Ghost.",
        },
    },
    { /* After M30 (DAEMON boss — true victory) */
        .trigger_mission = 30, .num_pages = 6,
        .speaker_pal = { SP_ZERO, SP_GLITCH, SP_PROXY, SP_GLITCH, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> DAEMON is destroyed.\nThis time it's real.",
            "GLITCH> The failsafe is\ngone. AXIOM is truly dead.",
            "PROXY> The corps will\nrebuild. They always do.",
            "GLITCH> But not AXIOM.\nNever again. I made sure.",
            "ZERO> The Net belongs to\nus now. All of us.",
            "ZERO> Ghost Protocol\ncomplete. Rest well, Ghost.",
        },
    },
    /* Sentinel */
    {
        .trigger_mission = 255, .num_pages = 0,
        .speaker_pal = {0},
        .pages = {0},
    },
    /* Padding entries to fill array to STORY_DIALOGUE_COUNT */
    { .trigger_mission = 255, .num_pages = 0, .speaker_pal = {0}, .pages = {0} },
    { .trigger_mission = 255, .num_pages = 0, .speaker_pal = {0}, .pages = {0} },
    { .trigger_mission = 255, .num_pages = 0, .speaker_pal = {0}, .pages = {0} },
    { .trigger_mission = 255, .num_pages = 0, .speaker_pal = {0}, .pages = {0} },
    { .trigger_mission = 255, .num_pages = 0, .speaker_pal = {0}, .pages = {0} },
    { .trigger_mission = 255, .num_pages = 0, .speaker_pal = {0}, .pages = {0} },
};

/* Contract type short names */
static const char* const type_names[CONTRACT_TYPE_COUNT] = {
    "EXTERMINATE",
    "RETRIEVAL",
    "SURVIVAL",
    "BOUNTY",
    "STORY",
};

/* Level requirement per mission (0=freelance, 1-30=story) */
static const u8 mission_level_req[STORY_MISSIONS + 1] = {
    0,                          /* M0: freelance */
    1,  3,  5,  6,  7,          /* M1-5:   Act 1 */
    9,  11, 13, 14, 15,         /* M6-10:  Act 2 */
    17, 19, 21, 22, 23,         /* M11-15: Act 3 */
    24, 25, 26, 27, 28,         /* M16-20: Act 4 */
    29, 30, 31, 32, 33,         /* M21-25: Act 5 */
    34, 35, 36, 38, 40,         /* M26-30: Act 6 */
};

void quest_init(void) {
    memset(&quest_state, 0, sizeof(quest_state));
    quest_state.active_contract_idx = 0xFF; /* None */
}

int quest_mission_to_act(int mission) {
    if (mission <= 0) return 0;
    return (mission - 1) / MISSIONS_PER_ACT + 1;
}

int quest_is_boss_mission(int mission) {
    return mission > 0 && (mission % MISSIONS_PER_ACT) == 0;
}

void quest_generate_contracts(int player_level) {
    int tier = 1 + player_level / 4;
    if (tier > 10) tier = 10;

    int active_count = 0;
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        if (quest_state.contracts[i].active) active_count++;
    }

    if (active_count >= 4) return;

    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* c = &quest_state.contracts[i];
        if (c->active) continue;
        if (active_count >= 4) break;

        /* Boss contract: every 25 endgame contracts */
        int is_boss_contract = 0;
        if (game_stats.endgame_unlocked &&
            game_stats.bb_endgame_contracts > 0 &&
            (game_stats.bb_endgame_contracts % 25) >= 23) {
            is_boss_contract = 1;
        }

        if (is_boss_contract) {
            c->type = CONTRACT_BOUNTY;
            c->tier = (u8)GP_MIN(tier + 3, 15); /* Higher tier */
            c->reward_rarity = RARITY_MYTHIC;    /* Guaranteed Mythic */
            c->reward_credits = (u16)(200 + tier * 50);
        } else {
            c->type = (u8)(rand_range(4)); /* EXTERMINATE/RETRIEVAL/SURVIVAL/BOUNTY */
            c->tier = (u8)tier;
            if (tier >= 4) {
                c->reward_rarity = RARITY_RARE;
            } else if (tier >= 2) {
                c->reward_rarity = RARITY_UNCOMMON;
            } else {
                c->reward_rarity = RARITY_COMMON;
            }
            c->reward_credits = (u16)(50 + tier * 30 + (int)rand_range(20));
        }
        c->active = 1;
        c->completed = 0;
        c->seed = (u16)(rand_next() & 0xFFFF);
        c->story_mission = 0;

        if (c->type == CONTRACT_EXTERMINATE) {
            c->kill_target = (u8)(5 + tier * 2 + (int)rand_range(3));
        } else if (c->type == CONTRACT_SURVIVAL) {
            c->kill_target = (u8)(8 + tier * 3);
        } else if (c->type == CONTRACT_BOUNTY) {
            c->kill_target = 1;
        } else {
            c->kill_target = 0;
        }
        c->kills = 0;
        active_count++;
    }
}

Contract* quest_get_active(void) {
    if (quest_state.active_contract_idx >= MAX_CONTRACTS) return NULL;
    Contract* c = &quest_state.contracts[quest_state.active_contract_idx];
    if (!c->active) return NULL;
    return c;
}

void quest_complete_active(void) {
    Contract* c = quest_get_active();
    if (!c) return;

    c->completed = 1;
    c->active = 0;

    if (c->story_mission > 0) {
        int m = c->story_mission;
        if (m > quest_state.story_mission) {
            quest_state.story_mission = (u8)m;
        }
        int act = quest_mission_to_act(m);
        if (act > quest_state.current_act) {
            quest_state.current_act = (u8)act;
        }
        if (quest_is_boss_mission(m)) {
            int boss_idx = m / MISSIONS_PER_ACT - 1;
            if (boss_idx >= 0 && boss_idx < 6) {
                quest_state.boss_defeated[boss_idx] = 1;
            }
        }
        /* Check story achievements */
        if (quest_is_story_complete()) {
            ach_unlock_celebrate(ACH_COMPLETIONIST);
        }
        /* Check all bosses defeated */
        {
            int all = 1;
            for (int i = 0; i < 6; i++) {
                if (!quest_state.boss_defeated[i]) { all = 0; break; }
            }
            if (all) ach_unlock_celebrate(ACH_BOSS_SLAYER);
        }
    } else {
        if (quest_state.contracts_completed < 255) {
            quest_state.contracts_completed++;
        }
        /* Track boss contract completions */
        if (c->type == CONTRACT_BOUNTY && game_stats.endgame_unlocked) {
            if (game_stats.bb_boss_contracts < 255) {
                game_stats.bb_boss_contracts++;
            }
        }
        /* Boss contract countdown for endgame */
        if (game_stats.endgame_unlocked && game_stats.bb_endgame_contracts > 0) {
            int until_boss = 25 - (game_stats.bb_endgame_contracts % 25);
            if (until_boss <= 5 && until_boss > 0) {
                hud_notify("BOSS CONTRACT SOON!", 60);
            }
        }
    }

    quest_state.active_contract_idx = 0xFF;
}

int quest_check_exit_condition(int enemies_killed, int boss_defeated) {
    Contract* c = quest_get_active();
    if (!c) return 1;

    switch (c->type) {
    case CONTRACT_EXTERMINATE:
        return enemies_killed >= c->kill_target;
    case CONTRACT_RETRIEVAL:
        return 1;
    case CONTRACT_SURVIVAL:
        return enemies_killed >= c->kill_target;
    case CONTRACT_BOUNTY:
        return boss_defeated;
    case CONTRACT_STORY:
        return boss_defeated;
    default:
        return 1;
    }
}

const char* quest_get_act_name(int act) {
    if (act < 0 || act > 6) return "";
    return act_names[act];
}

int quest_get_mission_level_req(int mission) {
    if (mission < 0 || mission > STORY_MISSIONS) return 99;
    return mission_level_req[mission];
}

Contract* quest_get_story_contract(int player_level) {
    int next_m = quest_state.story_mission + 1;
    if (next_m > STORY_MISSIONS) return NULL;

    if (player_level < mission_level_req[next_m]) return NULL;

    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* c = &quest_state.contracts[i];
        if (c->story_mission == next_m && c->active) return c;
    }

    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* c = &quest_state.contracts[i];
        if (!c->active) {
            int act = quest_mission_to_act(next_m);
            int is_boss = quest_is_boss_mission(next_m);

            if (is_boss) {
                c->type = CONTRACT_STORY;
            } else {
                int sub = (next_m - 1) % MISSIONS_PER_ACT;
                if (sub == 0) c->type = CONTRACT_RETRIEVAL;
                else if (sub == 1) c->type = CONTRACT_EXTERMINATE;
                else if (sub == 2) c->type = CONTRACT_SURVIVAL;
                else c->type = CONTRACT_RETRIEVAL;
            }

            c->tier = (u8)(act * 2 + (next_m - 1) % MISSIONS_PER_ACT);
            c->active = 1;
            c->completed = 0;
            c->reward_credits = (u16)(100 + next_m * 40);
            c->seed = (u16)(next_m * 1000 + 42);
            c->reward_rarity = is_boss ? RARITY_RARE : RARITY_UNCOMMON;
            c->story_mission = (u8)next_m;

            if (c->type == CONTRACT_EXTERMINATE) {
                c->kill_target = (u8)(5 + c->tier * 2);
            } else if (c->type == CONTRACT_SURVIVAL) {
                c->kill_target = (u8)(8 + c->tier * 3);
            } else {
                c->kill_target = 0;
            }
            c->kills = 0;
            return c;
        }
    }
    return NULL;
}

const char* quest_get_type_name(int type) {
    if (type < 0 || type >= CONTRACT_TYPE_COUNT) return "???";
    return type_names[type];
}

const char* quest_get_story_brief(int mission) {
    if (mission < 0 || mission > STORY_MISSIONS) return "";
    return story_briefs[mission];
}

int quest_is_story_complete(void) {
    return quest_state.story_mission >= STORY_MISSIONS;
}

const StoryDialogue* quest_get_story_dialogue(int completed_mission) {
    for (int i = 0; i < STORY_DIALOGUE_COUNT; i++) {
        if (story_dialogues[i].trigger_mission == (u8)completed_mission) {
            return &story_dialogues[i];
        }
    }
    return NULL;
}
