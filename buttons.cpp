#include "buttons.h"
#include "config.h"

static const uint8_t PINS[BTN_COUNT] = { JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_OK };
static const uint32_t DEBOUNCE_MS = 25;

struct BtnState {
  bool stable;        // estado estável (true = pressionado)
  bool raw;
  uint32_t raw_since;
  uint32_t pressed_at;
  uint32_t next_repeat;
};

static BtnState st[BTN_COUNT];
static BtnEvent queue[8];
static uint8_t q_head = 0, q_tail = 0;

static void push(Btn b, bool repeat) {
  uint8_t next = (q_head + 1) % 8;
  if (next == q_tail) return;
  queue[q_head] = { b, repeat };
  q_head = next;
}

void buttons_begin() {
  for (int i = 0; i < BTN_COUNT; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    st[i] = { false, false, 0, 0, 0 };
  }
}

void buttons_update() {
  uint32_t now = millis();
  for (int i = 0; i < BTN_COUNT; i++) {
    BtnState &s = st[i];
    bool raw = digitalRead(PINS[i]) == LOW;
    if (raw != s.raw) {
      s.raw = raw;
      s.raw_since = now;
    }
    if (s.raw != s.stable && now - s.raw_since >= DEBOUNCE_MS) {
      s.stable = s.raw;
      if (s.stable) {
        s.pressed_at = now;
        s.next_repeat = now + 450;
        push((Btn)i, false);
      }
    }
    // Auto-repetição acelerada (o OK não repete)
    if (s.stable && i != BTN_OK && (int32_t)(now - s.next_repeat) >= 0) {
      uint32_t held = now - s.pressed_at;
      uint32_t interval = held > 3000 ? 25 : held > 1500 ? 60 : 150;
      s.next_repeat = now + interval;
      push((Btn)i, true);
    }
  }
}

bool buttons_next(BtnEvent &ev) {
  if (q_tail == q_head) return false;
  ev = queue[q_tail];
  q_tail = (q_tail + 1) % 8;
  return true;
}

bool buttons_any_down() {
  for (int i = 0; i < BTN_COUNT; i++)
    if (st[i].stable) return true;
  return false;
}
