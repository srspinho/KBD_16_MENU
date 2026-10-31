#include "pomodoro.h"
#include "settings.h"

static PomoPhase phase = POMO_OFF;
static bool paused = false;
static uint32_t phase_total_ms = 0;
static uint32_t remaining_ms = 0;
static uint32_t last_tick = 0;
static uint16_t cycles = 0;

static void enter(PomoPhase p) {
  phase = p;
  paused = false;
  phase_total_ms = (uint32_t)(p == POMO_FOCUS ? settings.pomo_focus_min : settings.pomo_break_min) * 60000UL;
  remaining_ms = phase_total_ms;
  last_tick = millis();
}

void pomo_start_pause() {
  if (phase == POMO_OFF) {
    enter(POMO_FOCUS);
  } else {
    paused = !paused;
    last_tick = millis();
  }
}

void pomo_reset() {
  phase = POMO_OFF;
  paused = false;
  remaining_ms = 0;
  cycles = 0;
}

PomoEvent pomo_update() {
  if (phase == POMO_OFF) return POMO_EV_NONE;
  uint32_t now = millis();
  uint32_t dt = now - last_tick;
  last_tick = now;
  if (paused) return POMO_EV_NONE;
  if (dt < remaining_ms) {
    remaining_ms -= dt;
    return POMO_EV_NONE;
  }
  if (phase == POMO_FOCUS) {
    cycles++;
    enter(POMO_BREAK);
    return POMO_EV_FOCUS_DONE;
  }
  enter(POMO_FOCUS);
  return POMO_EV_BREAK_DONE;
}

PomoPhase pomo_phase() { return phase; }
bool pomo_paused() { return paused; }
uint32_t pomo_remaining_s() { return (remaining_ms + 999) / 1000; }
uint32_t pomo_phase_total_s() { return phase_total_ms / 1000; }
uint16_t pomo_cycles() { return cycles; }
