#ifndef GAME_QUEST_H
#define GAME_QUEST_H

#include <tonc.h>

/*
 * Ghost Protocol — Quest / Contract System
 *
 * 30 story missions across 6 acts (5 missions per act, boss on 5th).
 * Procedural side contracts for grinding XP/credits/loot between missions.
 * Story: The Glitch → Traceback → Deep Packet → Zero Day → Ghost Protocol → Trace Route
 */

/* Contract types */
enum {
    CONTRACT_EXTERMINATE = 0,  /* Kill N enemies */
    CONTRACT_RETRIEVAL,        /* Reach point + exit */
    CONTRACT_SURVIVAL,         /* Survive N waves (kill threshold) */
    CONTRACT_BOUNTY,           /* Mini-boss hunt */
    CONTRACT_STORY,            /* Main story mission (boss fight) */
    CONTRACT_TYPE_COUNT
};

/* Story mission count */
#define STORY_MISSIONS 30
#define MISSIONS_PER_ACT 5

/* Contract tier determines difficulty + rewards */
#define MAX_CONTRACTS 8

typedef struct {
    u8  type;           /* CONTRACT_* type */
    u8  tier;           /* 1-10 difficulty */
    u8  active;         /* 1 if available */
    u8  completed;      /* 1 if done */
    u16 reward_credits;
    u16 seed;           /* Level generation seed */
    u8  reward_rarity;  /* Minimum rarity for reward item */
    u8  story_mission;  /* 0 = side contract, 1-20 = story mission */
    u8  kill_target;    /* For exterminate/survival: kills needed */
    u8  kills;          /* Current kill count */
} Contract;

/* Quest state */
typedef struct {
    u8  current_act;           /* Story act 0-5 (derived from story_mission) */
    u8  story_mission;         /* Highest completed story mission (0-20) */
    u8  boss_defeated[6];      /* Which story bosses beaten (6 = DAEMON) */
    u8  choice_flags;          /* Story choices (2 bits × 4 choices) */
    u8  active_contract_idx;   /* Index of current contract (-1=none) */
    u8  contracts_completed;   /* Total side contracts completed */
    Contract contracts[MAX_CONTRACTS];
} QuestState;

extern QuestState quest_state;

/* ---- Story dialogue system ---- */
#define STORY_DIALOGUE_MAX_PAGES 8
#define STORY_DIALOGUE_COUNT 28

/* Speaker palette indices */
#define SP_ZERO   2  /* Cyan for ZERO */
#define SP_AXIOM  3  /* Red for AXIOM */
#define SP_PROXY  1  /* Amber for PROXY */
#define SP_GLITCH 0  /* Green for GLITCH (magenta in-game via pal tweak) */

typedef struct {
    u8  trigger_mission;    /* Show after completing this mission (0=first entry) */
    u8  num_pages;          /* Up to 8 pages */
    u8  speaker_pal[STORY_DIALOGUE_MAX_PAGES]; /* TPAL_* for speaker color */
    const char* pages[STORY_DIALOGUE_MAX_PAGES];
    u8  choice_bit;         /* Which choice_flags bit (0=none, 1-4=choice slot) */
    const char* choice_a;   /* Text for option A (NULL=no choice) */
    const char* choice_b;   /* Text for option B */
} StoryDialogue;

/* Get story dialogue triggered after completing a mission (or NULL). */
const StoryDialogue* quest_get_story_dialogue(int completed_mission);

/* Initialize quest system. */
void quest_init(void);

/* Generate side contracts based on player level. */
void quest_generate_contracts(int player_level);

/* Get the active contract (or NULL). */
Contract* quest_get_active(void);

/* Mark active contract as completed. */
void quest_complete_active(void);

/* Check if exit is allowed for current contract. */
int quest_check_exit_condition(int enemies_killed, int boss_defeated);

/* Get story act name (act 0-5). */
const char* quest_get_act_name(int act);

/* Get required level for a story mission (1-20). */
int quest_get_mission_level_req(int mission);

/* Get next story contract if available (level-gated). */
Contract* quest_get_story_contract(int player_level);

/* Get contract type short name. */
const char* quest_get_type_name(int type);

/* Get story briefing text for a mission (0-20). */
const char* quest_get_story_brief(int mission);

/* Check if all story bosses are defeated. */
int quest_is_story_complete(void);

/* Check if a mission number is a boss fight. */
int quest_is_boss_mission(int mission);

/* Get act number from mission number. */
int quest_mission_to_act(int mission);

#endif /* GAME_QUEST_H */
