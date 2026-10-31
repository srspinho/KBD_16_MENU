#include "stats.h"
#include "settings.h"

#define STATS_VERSION 1
#define SAVE_INTERVAL_MS (10UL * 60UL * 1000UL)  // no máximo 1 gravação a cada 10 min
#define SAVE_IDLE_MS (20UL * 1000UL)             // ... e só com o teclado parado
#define BREAK_GAP_MS (5UL * 60UL * 1000UL)       // 5 min sem digitar = pausa
#define ACTIVE_GAP_MS (60UL * 1000UL)            // intervalos < 1 min contam como "digitando"

struct PersistentStats {
  uint32_t total;
  uint16_t peak_kpm;
  uint16_t reserved;
  uint32_t usage[256];
};

static_assert(STORAGE_OFS_STATS + 8 + sizeof(PersistentStats) <= STORAGE_SIZE, "Estatisticas nao cabem na EEPROM");

static PersistentStats ps;
static bool ps_dirty = false;
static uint32_t last_save_ms = 0;

static uint32_t session = 0;
static uint32_t today = 0;
static uint16_t history[STATS_HISTORY];
static uint16_t current_second = 0;
static uint32_t second_start = 0;
static bool history_changed = true;
static uint32_t kpm_sum = 0;

static uint32_t last_key_ms = 0;
static uint32_t streak_start_ms = 0;
static uint32_t active_ms = 0;
static bool any_key = false;

void stats_begin() {
  if (!storage_read(STORAGE_OFS_STATS, STATS_VERSION, &ps, sizeof(ps))) {
    memset(&ps, 0, sizeof(ps));
  }
  second_start = millis();
}

void stats_on_key(uint8_t k) {
  uint32_t now = millis();
  session++;
  today++;
  ps.total++;
  ps.usage[k]++;
  ps_dirty = true;
  current_second++;

  if (!any_key || now - last_key_ms >= BREAK_GAP_MS) streak_start_ms = now;
  if (any_key && now - last_key_ms < ACTIVE_GAP_MS) active_ms += now - last_key_ms;
  last_key_ms = now;
  any_key = true;
}

void stats_update() {
  uint32_t now = millis();
  while (now - second_start >= 1000) {
    second_start += 1000;
    kpm_sum -= history[0];
    memmove(history, history + 1, (STATS_HISTORY - 1) * sizeof(history[0]));
    history[STATS_HISTORY - 1] = current_second;
    kpm_sum += current_second;
    current_second = 0;
    history_changed = true;
    if (kpm_sum > ps.peak_kpm) {
      ps.peak_kpm = (uint16_t)min<uint32_t>(kpm_sum, 65535);
      ps_dirty = true;
    }
  }
  stats_save_if_needed(false);
}

void stats_new_day() { today = 0; }

uint32_t stats_session() { return session; }
uint32_t stats_today() { return today; }
uint32_t stats_total() { return ps.total; }
uint16_t stats_kpm() { return (uint16_t)kpm_sum; }
uint16_t stats_peak_kpm() { return ps.peak_kpm; }
uint32_t stats_active_seconds() { return active_ms / 1000; }
uint32_t stats_last_key_ms() { return last_key_ms; }

uint32_t stats_streak_seconds() {
  if (!any_key) return 0;
  uint32_t now = millis();
  if (now - last_key_ms >= BREAK_GAP_MS) return 0;
  return (now - streak_start_ms) / 1000;
}

const uint16_t *stats_history() { return history; }

bool stats_history_changed() {
  bool c = history_changed;
  history_changed = false;
  return c;
}

uint8_t stats_top_keys(uint8_t *keys, uint32_t *counts, uint8_t n) {
  uint8_t found = 0;
  for (int k = 4; k < 256; k++) {
    uint32_t c = ps.usage[k];
    if (c == 0) continue;
    // inserção ordenada
    int pos = found;
    while (pos > 0 && counts[pos - 1] < c) pos--;
    if (pos >= n) continue;
    int last = (found < n) ? found : n - 1;
    for (int j = last; j > pos; j--) {
      keys[j] = keys[j - 1];
      counts[j] = counts[j - 1];
    }
    keys[pos] = (uint8_t)k;
    counts[pos] = c;
    if (found < n) found++;
  }
  return found;
}

void stats_reset_session() {
  session = 0;
  active_ms = 0;
  any_key = false;
}

void stats_reset_all() {
  stats_reset_session();
  today = 0;
  memset(&ps, 0, sizeof(ps));
  ps_dirty = true;
  stats_save_if_needed(true);
}

void stats_save_if_needed(bool force) {
  if (!ps_dirty) return;
  uint32_t now = millis();
  if (!force) {
    if (now - last_save_ms < SAVE_INTERVAL_MS) return;
    if (any_key && now - last_key_ms < SAVE_IDLE_MS) return;  // não grava no meio da digitação
  }
  storage_write(STORAGE_OFS_STATS, STATS_VERSION, &ps, sizeof(ps));
  storage_commit();
  ps_dirty = false;
  last_save_ms = now;
}
