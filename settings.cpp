#include "settings.h"
#include "screensavers.h"
#include <EEPROM.h>

#define SETTINGS_VERSION 3
#define BLOCK_MAGIC 0x4B42  // "KB"

Settings settings;

// Os blocos gravados na flash não podem se sobrepor
static_assert(sizeof(Settings) + 8 <= STORAGE_OFS_STATS - STORAGE_OFS_SETTINGS, "Settings nao cabe no bloco");
static bool dirty = false;
static bool storage_started = false;

struct BlockHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t reserved;
  uint16_t len;
  uint16_t crc;
};

static uint16_t crc16(const uint8_t *p, uint16_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*p++) << 8;
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

static void storage_begin() {
  if (!storage_started) {
    EEPROM.begin(STORAGE_SIZE);
    storage_started = true;
  }
}

bool storage_read(uint16_t offset, uint8_t version, void *data, uint16_t len) {
  storage_begin();
  BlockHeader h;
  EEPROM.get(offset, h);
  if (h.magic != BLOCK_MAGIC || h.version != version || h.len != len) return false;
  uint8_t *dst = (uint8_t *)data;
  for (uint16_t i = 0; i < len; i++) dst[i] = EEPROM.read(offset + sizeof(BlockHeader) + i);
  return crc16(dst, len) == h.crc;
}

void storage_write(uint16_t offset, uint8_t version, const void *data, uint16_t len) {
  storage_begin();
  BlockHeader h = { BLOCK_MAGIC, version, 0, len, crc16((const uint8_t *)data, len) };
  EEPROM.put(offset, h);
  const uint8_t *src = (const uint8_t *)data;
  for (uint16_t i = 0; i < len; i++) EEPROM.write(offset + sizeof(BlockHeader) + i, src[i]);
}

void storage_commit() {
  storage_begin();
  EEPROM.commit();  // apaga e grava 1 setor de flash (~50 ms, pausa o core 1)
}

// -------------------------------------------------------------------
void settings_defaults(Settings &s) {
  s.ss_type = ss_count();  // Todos
  s.ss_start_min = 5;
  s.ss_switch_s = 30;
  s.ss_speed = 10;
  s.ss_asteroids = 12;
  s.ss_color = 80;
  s.brightness = 100;
  s.dim_brightness = 30;
  s.dim_after_s = 60;
  s.flip = false;
  s.antilock = true;
  s.antilock_s = 15;
  s.pomo_focus_min = 25;
  s.pomo_break_min = 5;
  s.break_remind_min = 0;
  s.remap = true;
  s.layer = false;
  s.vol_sync = true;
  s.vol_sync_level = 70;
}

void settings_validate(Settings &s) {
  s.ss_type = constrain(s.ss_type, 0, ss_count());
  s.ss_start_min = constrain(s.ss_start_min, 0, 120);
  s.ss_switch_s = constrain(s.ss_switch_s, 10, 600);
  s.ss_speed = constrain(s.ss_speed, 1, 20);
  s.ss_asteroids = constrain(s.ss_asteroids, 1, MAX_ASTEROIDS);
  s.ss_color = constrain(s.ss_color, 10, 100);
  s.brightness = constrain(s.brightness, 10, 100);
  s.dim_brightness = constrain(s.dim_brightness, 0, 100);
  s.dim_after_s = constrain(s.dim_after_s, 0, 3600);
  s.antilock_s = constrain(s.antilock_s, 5, 600);
  s.pomo_focus_min = constrain(s.pomo_focus_min, 1, 120);
  s.pomo_break_min = constrain(s.pomo_break_min, 1, 60);
  s.break_remind_min = constrain(s.break_remind_min, 0, 240);
  s.vol_sync_level = constrain(s.vol_sync_level, 0, 100);
  // bools: qualquer valor diferente de 0/1 vira false
  bool *flags[] = { &s.flip, &s.antilock, &s.remap, &s.layer, &s.vol_sync };
  for (bool *f : flags) {
    uint8_t raw;
    memcpy(&raw, f, 1);
    if (raw > 1) *f = false;
  }
}

void settings_load() {
  Settings tmp;
  if (storage_read(STORAGE_OFS_SETTINGS, SETTINGS_VERSION, &tmp, sizeof(tmp))) {
    settings = tmp;
    settings_validate(settings);
  } else {
    settings_defaults(settings);
    dirty = true;  // grava os padrões na próxima oportunidade
  }
}

void settings_mark_dirty() { dirty = true; }
bool settings_is_dirty() { return dirty; }

void settings_save_if_dirty() {
  if (!dirty) return;
  storage_write(STORAGE_OFS_SETTINGS, SETTINGS_VERSION, &settings, sizeof(settings));
  storage_commit();
  dirty = false;
}
