#include "engine/save.h"
#include <string.h>

#define SRAM ((volatile u8*)0x0E000000)

/* Required for flash cart and emulator auto-detection of save type */
static const char __attribute__((used)) sram_id[] = "SRAM_Vnnn";

static u16 crc16(const u8* data, int len) {
    u16 crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (u16)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (u16)((crc >> 1) ^ 0xA001);
            else
                crc = (u16)(crc >> 1);
        }
    }
    return crc;
}

void save_write_slot(SaveData* data, int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return;

    const u8* payload = (const u8*)&data->player_class;
    int payload_len = (int)sizeof(SaveData) - (int)(payload - (const u8*)data);
    data->checksum = crc16(payload, payload_len);

    const u8* src = (const u8*)data;
    int offset = slot * SAVE_SLOT_SIZE;

    /* Write in 32-byte chunks, re-enabling interrupts between chunks
     * so Maxmod can service its Timer 0 audio DMA IRQ */
    int total = (int)sizeof(SaveData);
    for (int base = 0; base < total; base += 32) {
        int end = base + 32;
        if (end > total) end = total;
        u16 ime_prev = REG_IME;
        REG_IME = 0;
        for (int i = base; i < end; i++) {
            SRAM[offset + i] = src[i];
        }
        REG_IME = ime_prev;
    }
}

int save_read_slot(SaveData* data, int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;

    u8* dst = (u8*)data;
    int offset = slot * SAVE_SLOT_SIZE;
    for (int i = 0; i < (int)sizeof(SaveData); i++) {
        dst[i] = SRAM[offset + i];
    }

    if (data->magic != SAVE_MAGIC) return 0;

    const u8* payload = (const u8*)&data->player_class;
    int payload_len = (int)sizeof(SaveData) - (int)(payload - (const u8*)data);
    u16 computed = crc16(payload, payload_len);
    if (computed != data->checksum) return 0;

    return 1;
}

void save_defaults(SaveData* data) {
    memset(data, 0, sizeof(SaveData));
    data->magic = SAVE_MAGIC;
    data->player_class = 0;
    data->player_level = 1;
    data->player_hp = 30;
    data->player_max_hp = 30;
    data->player_xp = 0;
    data->player_atk = 5;
    data->player_def = 3;
    data->player_spd = 3;
    data->player_lck = 1;
    data->credits = 100;
    data->ability_unlocks = 0;
    data->quest_act = 0;
    data->equipped_idx = 0xFF;
    data->inventory_count = 0;

    const u8* payload = (const u8*)&data->player_class;
    int payload_len = (int)sizeof(SaveData) - (int)(payload - (const u8*)data);
    data->checksum = crc16(payload, payload_len);
}

int save_slot_exists(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;
    int offset = slot * SAVE_SLOT_SIZE;

    /* Read just the magic number */
    u32 magic = 0;
    u8* m = (u8*)&magic;
    for (int i = 0; i < 4; i++) {
        m[i] = SRAM[offset + i];
    }
    return magic == SAVE_MAGIC;
}

/* Legacy API — slot 0 */
void save_write(SaveData* data) {
    save_write_slot(data, 0);
}

int save_read(SaveData* data) {
    return save_read_slot(data, 0);
}
