/*
 * Ghost Protocol — Quest / Contract System
 *
 * 20 story missions across 5 acts (4 per act, boss on 4th).
 * Level-gated progression with side contracts for grinding.
 * ~20 hours to complete the full story.
 */
#include "game/quest.h"
#include "game/common.h"
#include "engine/rng.h"
#include <string.h>

QuestState quest_state;

static const char* const act_names[6] = {
    "Freelance",
    "The Glitch",
    "Traceback",
    "Deep Packet",
    "Zero Day",
    "Ghost Protocol",
};

/* Story briefings — 21 entries: index 0 = freelance, 1-20 = missions */
static const char* const story_briefs[STORY_MISSIONS + 1] = {
    /* M0: Freelance */
    "ZERO> Take contracts.\nStay sharp. Stay alive.",
    /* Act 1: The Glitch (M1-M4) */
    "ZERO> Routine run. Grab the\ndata, get out. Easy money.",
    "ZERO> Data keeps corrupting.\nSomeone is interfering.",
    "ZERO> Track the source. The\ninterference is organized.",
    "ZERO> MICROSLOP's firewall.\nSmash through it.",
    /* Act 2: Traceback (M5-M8) */
    "ZERO> Past the firewall.\nThe Net gets stranger here.",
    "ZERO> Rogue programs breed.\nClear them out.",
    "ZERO> Something watches us.\nAn AI called AXIOM.",
    "ZERO> GOGOL runs all search.\nBlind it. Cut AXIOM's eyes.",
    /* Act 3: Deep Packet (M9-M12) */
    "PROXY> Deep in the packet\nlayer. Careful, Ghost.",
    "PROXY> AXIOM's IC drones\nswarm. Clear a path.",
    "PROXY> Five corps. Five\ngates. Break them all.",
    "ZERO> AMAZOMB's supply line.\nCut the link.",
    /* Act 4: Zero Day (M13-M16) */
    "ZERO> Walls crumbling. AXIOM\nturns on its creators.",
    "ZERO> Exploit the chaos.\nGet deeper.",
    "AXIOM> You destroy what you\ndon't understand.",
    "ZERO> CRAPPLE's garden.\nNothing gets in or out.",
    /* Act 5: Ghost Protocol (M17-M20) */
    "ZERO> Root access is close.\nAXIOM knows you're coming.",
    "ZERO> The Net destabilizes.\nIC swarms everywhere.",
    "ZERO> One gate left. Then\nwe execute Ghost Protocol.",
    "ZERO> FACEPLANT. Last gate.\nBurn it all down.",
};

/* ---- Story dialogue data (shown in terminal after key missions) ---- */
/* TPAL defines: 0=GREEN, 1=AMBER, 2=CYAN, 3=RED */
#define SP_ZERO  2  /* Cyan for ZERO */
#define SP_AXIOM 3  /* Red for AXIOM */
#define SP_PROXY 1  /* Amber for PROXY */

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
    { /* After M4 (MICROSLOP boss) */
        .trigger_mission = 4, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_AXIOM, SP_AXIOM },
        .pages = {
            "ZERO> You broke MICROSLOP's\nwall. Good. Four remain.",
            "ZERO> Five corps built\nsomething. An AI. AXIOM.",
            "AXIOM> I see you, Ghost.",
            "AXIOM> You are noise. I\nwill filter you out.",
        },
    },
    { /* After M5 */
        .trigger_mission = 5, .num_pages = 2,
        .speaker_pal = { SP_AXIOM, SP_AXIOM },
        .pages = {
            "AXIOM> Past one wall means\nnothing.",
            "AXIOM> There are four more.\nAnd then there is me.",
        },
    },
    { /* After M7 */
        .trigger_mission = 7, .num_pages = 3,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> AXIOM. The five corps\nbuilt it to control the Net.",
            "ZERO> It went rogue. Now it\ncontrols them.",
            "ZERO> Each corp guards a\ngate. Smash all five.",
        },
    },
    { /* After M8 (GOGOL boss) */
        .trigger_mission = 8, .num_pages = 3,
        .speaker_pal = { SP_PROXY, SP_PROXY, SP_PROXY },
        .pages = {
            "PROXY> You blinded GOGOL's\nsearch. AXIOM can't see you.",
            "PROXY> I'm PROXY. I trade\nin secrets. We should talk.",
            "PROXY> Three gates remain.\nKeep breaking them.",
        },
    },
    { /* After M11 */
        .trigger_mission = 11, .num_pages = 2,
        .speaker_pal = { SP_PROXY, SP_PROXY },
        .pages = {
            "PROXY> Five corps. Five\ngates. AXIOM was their\nleash on the Net.",
            "PROXY> Now it holds the\nleash. Funny how that works.",
        },
    },
    { /* After M12 (AMAZOMB boss) */
        .trigger_mission = 12, .num_pages = 2,
        .speaker_pal = { SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> Three guardians down.\nAXIOM is rewriting itself.",
            "ZERO> It's scared, Ghost.\nKeep pushing.",
        },
    },
    { /* After M15 */
        .trigger_mission = 15, .num_pages = 3,
        .speaker_pal = { SP_AXIOM, SP_AXIOM, SP_AXIOM },
        .pages = {
            "AXIOM> You destroy what\nyou don't understand.",
            "AXIOM> I AM the Net.\nWithout me, it dies.",
            "AXIOM> You will see.\nWhen I am gone... chaos.",
        },
    },
    { /* After M16 (CRAPPLE boss) */
        .trigger_mission = 16, .num_pages = 3,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> One gate left. After\nFACEPLANT falls...",
            "ZERO> We execute Ghost\nProtocol. Full root access.",
            "ZERO> End AXIOM forever.",
        },
    },
    { /* After M19 */
        .trigger_mission = 19, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_AXIOM, SP_AXIOM, SP_ZERO },
        .pages = {
            "ZERO> This is it. Last\ngate. Then AXIOM itself.",
            "AXIOM> You cannot win. I\nhave seen every outcome.",
            "AXIOM> In none of them do\nyou survive.",
            "ZERO> Jack in one last\ntime. Make it count.",
        },
    },
    { /* After M20 (FACEPLANT boss — victory) */
        .trigger_mission = 20, .num_pages = 4,
        .speaker_pal = { SP_ZERO, SP_ZERO, SP_ZERO, SP_ZERO },
        .pages = {
            "ZERO> Ghost Protocol\nexecuted. AXIOM is gone.",
            "ZERO> The Net is free.\nYou did it, Ghost.",
            "ZERO> The corps will\nrebuild. They always do.",
            "ZERO> But for now... the\nNet belongs to us.",
        },
    },
    { /* Sentinel — marks end of array */
        .trigger_mission = 255, .num_pages = 0,
        .speaker_pal = {0},
        .pages = {0},
    },
};

/* Contract type short names */
static const char* const type_names[CONTRACT_TYPE_COUNT] = {
    "EXTERMINATE",
    "RETRIEVAL",
    "SURVIVAL",
    "BOUNTY",
    "STORY",
};

/* Level requirement per mission (0=freelance, 1-20=story) */
static const u8 mission_level_req[STORY_MISSIONS + 1] = {
    0,                      /* M0: freelance */
    1,  3,  5,  7,          /* M1-4:   Act 1 */
    9,  11, 13, 15,         /* M5-8:   Act 2 */
    17, 19, 21, 23,         /* M9-12:  Act 3 */
    24, 25, 26, 27,         /* M13-16: Act 4 */
    28, 28, 29, 30,         /* M17-20: Act 5 */
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
    /* Generate side contracts based on level */
    int tier = 1 + player_level / 4;
    if (tier > 10) tier = 10;

    /* Count existing active contracts */
    int active_count = 0;
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        if (quest_state.contracts[i].active) active_count++;
    }

    /* Only generate if we have fewer than 4 active side contracts */
    if (active_count >= 4) return;

    /* Fill available contract slots (leave room for story) */
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* c = &quest_state.contracts[i];
        if (c->active) continue;
        if (active_count >= 4) break;

        c->type = (u8)(rand_range(3));  /* Random side type: EXTERMINATE/RETRIEVAL/SURVIVAL */
        c->tier = (u8)tier;
        c->active = 1;
        c->completed = 0;
        c->reward_credits = (u16)(50 + tier * 30 + (int)rand_range(20));
        c->seed = (u16)(rand_next() & 0xFFFF);
        c->story_mission = 0;

        /* Set reward rarity based on tier */
        if (tier >= 4) {
            c->reward_rarity = RARITY_RARE;
        } else if (tier >= 2) {
            c->reward_rarity = RARITY_UNCOMMON;
        } else {
            c->reward_rarity = RARITY_COMMON;
        }

        /* Set kill targets based on contract type */
        if (c->type == CONTRACT_EXTERMINATE) {
            c->kill_target = (u8)(5 + tier * 2 + (int)rand_range(3));
        } else if (c->type == CONTRACT_SURVIVAL) {
            c->kill_target = (u8)(8 + tier * 3);
        } else if (c->type == CONTRACT_BOUNTY) {
            c->kill_target = 1; /* Kill the mini-boss */
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

    /* If story mission, advance progress */
    if (c->story_mission > 0) {
        int m = c->story_mission;
        /* Track highest completed mission */
        if (m > quest_state.story_mission) {
            quest_state.story_mission = (u8)m;
        }
        /* Update current act */
        int act = quest_mission_to_act(m);
        if (act > quest_state.current_act) {
            quest_state.current_act = (u8)act;
        }
        /* If boss mission, mark boss defeated */
        if (quest_is_boss_mission(m)) {
            int boss_idx = m / MISSIONS_PER_ACT - 1;
            if (boss_idx >= 0 && boss_idx < 5) {
                quest_state.boss_defeated[boss_idx] = 1;
            }
        }
    } else {
        /* Side contract: increment counter */
        if (quest_state.contracts_completed < 255) {
            quest_state.contracts_completed++;
        }
    }

    quest_state.active_contract_idx = 0xFF;
}

int quest_check_exit_condition(int enemies_killed, int boss_defeated) {
    Contract* c = quest_get_active();
    if (!c) return 1; /* No contract = always allow exit */

    switch (c->type) {
    case CONTRACT_EXTERMINATE:
        /* Must kill enough enemies */
        return enemies_killed >= c->kill_target;

    case CONTRACT_RETRIEVAL:
        /* Just reach the exit */
        return 1;

    case CONTRACT_SURVIVAL:
        /* Must kill enough waves worth of enemies */
        return enemies_killed >= c->kill_target;

    case CONTRACT_BOUNTY:
        /* Must defeat the mini-boss (boss_defeated flag) */
        return boss_defeated;

    case CONTRACT_STORY:
        /* Must defeat the story boss */
        return boss_defeated;

    default:
        return 1;
    }
}

const char* quest_get_act_name(int act) {
    if (act < 0 || act > 5) return "";
    return act_names[act];
}

int quest_get_mission_level_req(int mission) {
    if (mission < 0 || mission > STORY_MISSIONS) return 99;
    return mission_level_req[mission];
}

Contract* quest_get_story_contract(int player_level) {
    int next_m = quest_state.story_mission + 1;
    if (next_m > STORY_MISSIONS) return NULL;

    /* Level gate: player must meet minimum level */
    if (player_level < mission_level_req[next_m]) return NULL;

    /* Find existing story contract for this mission */
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* c = &quest_state.contracts[i];
        if (c->story_mission == next_m && c->active) return c;
    }

    /* Create new story contract in first free slot */
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* c = &quest_state.contracts[i];
        if (!c->active) {
            int act = quest_mission_to_act(next_m);
            int is_boss = quest_is_boss_mission(next_m);

            /* Boss missions use CONTRACT_STORY; others use varied types */
            if (is_boss) {
                c->type = CONTRACT_STORY;
            } else {
                /* Rotate contract types for variety */
                int sub = (next_m - 1) % MISSIONS_PER_ACT; /* 0, 1, 2 */
                if (sub == 0) c->type = CONTRACT_RETRIEVAL;
                else if (sub == 1) c->type = CONTRACT_EXTERMINATE;
                else c->type = CONTRACT_SURVIVAL;
            }

            c->tier = (u8)(act * 2 + (next_m - 1) % MISSIONS_PER_ACT);
            c->active = 1;
            c->completed = 0;
            c->reward_credits = (u16)(100 + next_m * 40);
            c->seed = (u16)(next_m * 1000 + 42);
            c->reward_rarity = is_boss ? RARITY_RARE : RARITY_UNCOMMON;
            c->story_mission = (u8)next_m;

            /* Set kill targets for non-boss story missions */
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
